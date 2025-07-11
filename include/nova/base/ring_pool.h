#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <cstdio>

#include "nova/common/macros.h"

namespace nova {

// Type constraint for memory-mapped types
template <typename T>
concept MMapType = std::is_standard_layout_v<T> && std::is_trivial_v<T>;

// Dynamic ring memory pool implementation that supports arbitrary
// types and data lengths. Uses std::vector for dynamic memory allocation.
//
// Features:
// - Dynamic memory allocation with std::vector
// - Support for arbitrary POD types
// - Memory alignment for optimal performance
// - Thread-safe for single producer
// - Zero-copy operations
//
// Usage:
//   RingPool pool(1024);  // Create a 1KB pool
//   auto& data = pool.Emplace<MyData>(args...);  // Construct object
//   auto* raw = pool.Allocate(64);  // Allocate raw memory
//   auto& data = pool.Read<MyData>(offset);  // Read at offset
class RingPool {
 public:
  using size_type = std::size_t;
  static constexpr size_type kAlignment = alignof(std::max_align_t);

  // Constructor with capacity parameter
  explicit RingPool(size_type n = 16) {
    if (n == 0) {
      throw std::invalid_argument("RingPool capacity must be greater than 0");
    }

    buffer_.resize(n);
    write_pos_ = 0;
  }

  // Copy constructor
  RingPool(const RingPool& other) = default;

  // Move constructor
  RingPool(RingPool&& other) noexcept
      : write_pos_(other.write_pos_),
        latest_pos_(other.latest_pos_),
        write_count_(other.write_count_),
        buffer_(std::move(other.buffer_)) {
    other.buffer_.resize(16);
    other.write_pos_ = 0;
    other.latest_pos_ = 0;
    other.write_count_ = 0;
  }

  // Copy assignment operator
  RingPool& operator=(const RingPool& other) {
    if (this != &other) {
      buffer_ = other.buffer_;
      write_pos_ = other.write_pos_;
      latest_pos_ = other.latest_pos_;
      write_count_ = other.write_count_;
    }
    return *this;
  }

  // Move assignment operator
  RingPool& operator=(RingPool&& other) noexcept {
    if (this != &other) {
      buffer_ = std::move(other.buffer_);
      write_pos_ = other.write_pos_;
      latest_pos_ = other.latest_pos_;
      write_count_ = other.write_count_;

      other.buffer_.resize(16);
      other.write_pos_ = 0;
      other.latest_pos_ = 0;
      other.write_count_ = 0;
    }
    return *this;
  }

  // Destructor
  ~RingPool() = default;

  // Construct an object at current position
  template <typename T, typename... Args>
    requires MMapType<T>
  T& Emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>) {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    CalculateWritePos(sizeof(T), alignof(T));

    T* ptr = new (&buffer_[write_pos_]) T(std::forward<Args>(args)...);
    latest_pos_ = write_pos_;
    write_pos_ += sizeof(T);
    ++write_count_;
    return *ptr;
  }

  // Allocate raw memory of specified size
  std::byte* Allocate(size_type size) noexcept {
    CalculateWritePos(size, kAlignment);

    std::byte* ptr = &buffer_[write_pos_];
    latest_pos_ = write_pos_;
    write_pos_ += size;
    ++write_count_;
    return ptr;
  }

  // Allocate memory for type T without initialization
  template <typename T>
    requires MMapType<T>
  T& Allocate() noexcept {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    CalculateWritePos(sizeof(T), alignof(T));

    T& ref = reinterpret_cast<T&>(buffer_[write_pos_]);
    latest_pos_ = write_pos_;
    write_pos_ += sizeof(T);
    ++write_count_;
    return ref;
  }

  // Push data into the memory pool
  std::byte* Push(const void* data, size_type size) {
    // Ensure alignment
    write_pos_ = (write_pos_ + kAlignment - 1) & ~(kAlignment - 1);

    // Check if data would cross buffer boundary
    if (write_pos_ + size > buffer_.size()) [[unlikely]] {
      // Wrap to beginning (0 is always aligned)
      write_pos_ = 0;
    }

    std::byte* ptr = &buffer_[write_pos_];
    std::memcpy(ptr, data, size);
    latest_pos_ = write_pos_;
    write_pos_ = write_pos_ + size;
    ++write_count_;
    return ptr;
  }

  // Get the most recently written position
  [[nodiscard]] size_type latest_pos() const {
    return latest_pos_;
  }

  template <typename T>
    requires MMapType<T>
  T& Read(size_type offset) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > buffer_.size()) {
        throw std::out_of_range("Read position out of range");
      }
    }
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  template <typename T>
    requires MMapType<T>
  const T& Read(size_type offset) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > buffer_.size()) {
        throw std::out_of_range("Read position out of range");
      }
    }
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  std::byte* ReadBuffer(size_type offset) noexcept {
    return buffer_.data() + offset;
  }

  [[nodiscard]] std::byte* ReadBuffer(size_type offset) const noexcept {
    return const_cast<std::byte*>(buffer_.data() + offset);
  }

  template <typename T>
    requires MMapType<T>
  T& operator[](size_type offset) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > buffer_.size()) {
        throw std::out_of_range("Index out of range");
      }
    }
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  template <typename T>
    requires MMapType<T>
  const T& operator[](size_type offset) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > buffer_.size()) {
        throw std::out_of_range("Index out of range");
      }
    }
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  // Get current write position
  [[nodiscard]] constexpr size_type write_pos() const noexcept {
    return write_pos_;
  }

  // Get total number of writes
  [[nodiscard]] constexpr size_type write_count() const noexcept {
    return write_count_;
  }

  // Get available space
  [[nodiscard]] size_type available_space() const noexcept {
    return buffer_
        .size();  // For ring buffer, total capacity is always available
  }

  // Get total capacity
  [[nodiscard]] size_type capacity() const {
    return buffer_.size();
  }

  // Reset write position and count
  void Reset() {
    write_pos_ = 0;
    write_count_ = 0;
    latest_pos_ = 0;
  }

  // Direct access to underlying memory (use with caution)
  std::byte* data() {
    return buffer_.data();
  }

  [[nodiscard]] const std::byte* data() const {
    return buffer_.data();
  }

  // Get pointer at specific position
  std::byte* At(size_type pos) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= buffer_.size()) {
        throw std::out_of_range("Position out of range");
      }
    }
    return &buffer_[pos];
  }

  [[nodiscard]] const std::byte* At(size_type pos) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= buffer_.size()) {
        throw std::out_of_range("Position out of range");
      }
    }
    return &buffer_[pos];
  }

  // Get buffer size
  [[nodiscard]] size_type size() const noexcept {
    return buffer_.size();
  }

  // Check if buffer is empty (no writes yet)
  [[nodiscard]] constexpr bool empty() const noexcept {
    return write_count_ == 0;
  }

 private:
  // Hot data - frequently accessed together
  size_type write_pos_{0};    // Current write position
  size_type latest_pos_{0};   // Most recently written position
  size_type write_count_{0};  // Total number of writes

  // Cold data - less frequently accessed
  std::vector<std::byte> buffer_;  // Underlying storage

  // Helper functions for alignment and boundary checking
  static constexpr size_type AlignUp(size_type pos,
                                     size_type alignment) noexcept {
    return (pos + alignment - 1) & ~(alignment - 1);
  }

  void CalculateWritePos(size_type size, size_type alignment) noexcept {
    write_pos_ = AlignUp(write_pos_, alignment);
    write_pos_ *= (write_pos_ + size <= buffer_.size());
  }
};

namespace static_impl {

// Low-level static ring memory pool implementation that supports arbitrary
// types and data lengths. For shared memory usage, type T must satisfy strict
// constraints.
//
// Features:
// - Fixed-size memory pool with ring buffer behavior
// - Support for arbitrary POD types
// - Memory alignment for optimal performance
// - Thread-safe for single producer
// - Zero-copy operations
//
// Usage:
//   RingPool<1024> pool;  // Create a 1KB pool
//   auto& data = pool.Emplace<MyData>(args...);  // Construct object
//   auto* raw = pool.Allocate(64);  // Allocate raw memory
//   auto& data = pool.Read<MyData>(offset);  // Read at offset
template <std::size_t N>
class RingPool {
 public:
  using size_type = std::size_t;
  static constexpr size_type kAlignment = alignof(std::max_align_t);

  RingPool() = default;

  // Explicit destructor to handle large arrays safely
  ~RingPool() = default;

  // Construct an object at current position
  template <typename T, typename... Args>
    requires MMapType<T>
  T& Emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>) {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    CalculateWritePos(sizeof(T), alignof(T));

    T* ptr = new (&buffer_[write_pos_]) T(std::forward<Args>(args)...);
    latest_pos_ = write_pos_;
    write_pos_ += sizeof(T);
    ++write_count_;
    return *ptr;
  }

  // Allocate raw memory of specified size
  std::byte* Allocate(size_type size) noexcept {
    CalculateWritePos(size, kAlignment);
    std::cout << "write_pos_ = " << write_pos_ << ", alignment = " << kAlignment << std::endl;

    std::byte* ptr = buffer_.data() + write_pos_;

    printf("get pointer = %p\n", ptr);
    std::cout << "latest_pos_ = " << latest_pos_ << ", size = " << size << std::endl;

    latest_pos_ = write_pos_;
    write_pos_ += size;
    ++write_count_;
    return ptr;
  }

  // Allocate memory for type T without initialization
  template <typename T>
    requires MMapType<T>
  T& Allocate() noexcept {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    CalculateWritePos(sizeof(T), alignof(T));

    T& ref = reinterpret_cast<T&>(buffer_[write_pos_]);
    latest_pos_ = write_pos_;
    write_pos_ += sizeof(T);
    ++write_count_;
    return ref;
  }

  // Push data into the memory pool
  std::byte* Push(const void* data, size_type size) {
    // Ensure alignment
    write_pos_ = (write_pos_ + kAlignment - 1) & ~(kAlignment - 1);

    // Check if data would cross buffer boundary
    if (write_pos_ + size > N) [[unlikely]] {
      // Wrap to beginning (0 is always aligned)
      write_pos_ = 0;
    }

    std::byte* ptr = &buffer_[write_pos_];
    std::memcpy(ptr, data, size);
    latest_pos_ = write_pos_;
    write_pos_ = write_pos_ + size;
    ++write_count_;
    return ptr;
  }

  // Get the most recently written position
  [[nodiscard]] size_type latest_pos() const {
    return latest_pos_;
  }

  template <typename T>
    requires MMapType<T>
  T& Read(size_type offset) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > N) {
        throw std::out_of_range("Read position out of range");
      }
    }
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  template <typename T>
    requires MMapType<T>
  const T& Read(size_type offset) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > N) {
        throw std::out_of_range("Read position out of range");
      }
    }
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  std::byte* ReadBuffer(size_type offset) noexcept {
    return buffer_.data() + offset;
  }

  [[nodiscard]] std::byte* ReadBuffer(size_type offset) const noexcept {
    return const_cast<std::byte*>(buffer_.data() + offset);
  }

  template <typename T>
    requires MMapType<T>
  T& operator[](size_type offset) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > N) {
        throw std::out_of_range("Index out of range");
      }
    }
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  template <typename T>
    requires MMapType<T>
  const T& operator[](size_type offset) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > N) {
        throw std::out_of_range("Index out of range");
      }
    }
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  // Get current write position
  [[nodiscard]] constexpr size_type write_pos() const noexcept {
    return write_pos_;
  }

  // Get total number of writes
  [[nodiscard]] size_type write_count() const {
    return write_count_;
  }

  // Get available space
  [[nodiscard]] static constexpr size_type available_space() noexcept {
    return N;  // For ring buffer, total capacity is always available
  }

  // Get total capacity
  [[nodiscard]] constexpr size_type capacity() const {
    return N;
  }

  // Reset write position and count
  void Reset() {
    write_pos_ = 0;
    write_count_ = 0;
    latest_pos_ = 0;
  }

  // Direct access to underlying memory (use with caution)
  std::byte* data() {
    return buffer_.data();
  }

  [[nodiscard]] const std::byte* data() const {
    return buffer_.data();
  }

  // Get pointer at specific position
  std::byte* At(size_type pos) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= N) {
        throw std::out_of_range("Position out of range");
      }
    }
    return &buffer_[pos];
  }

  [[nodiscard]] const std::byte* At(size_type pos) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= N) {
        throw std::out_of_range("Position out of range");
      }
    }
    return &buffer_[pos];
  }

  // Get buffer size
  [[nodiscard]] static constexpr size_type size() noexcept {
    return N;
  }

  // Check if buffer is empty (no writes yet)
  [[nodiscard]] constexpr bool empty() const noexcept {
    return write_count_ == 0;
  }

 private:
  // Hot data - frequently accessed together
  size_type write_pos_{0};    // Current write position
  size_type latest_pos_{0};   // Most recently written position
  size_type write_count_{0};  // Total number of writes

  // Cold data - large buffer placed last
  alignas(kAlignment) std::array<std::byte, N> buffer_;

  // Helper functions for alignment and boundary checking
  static constexpr size_type AlignUp(size_type pos,
                                     size_type alignment) noexcept {
    return (pos + alignment - 1) & ~(alignment - 1);
  }

  void CalculateWritePos(size_type size, size_type alignment) noexcept {
    write_pos_ = AlignUp(write_pos_, alignment);
    write_pos_ *= (write_pos_ + size <= buffer_.size());
  }
};

}  // namespace static_impl

}  // namespace nova