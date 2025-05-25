#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "nova/common/macros.h"

namespace nova {

namespace static_impl {

// Type constraint for memory-mapped types
template <typename T>
concept MMapType = std::is_standard_layout_v<T> && std::is_trivial_v<T>;

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

  // Ensure N is a power of 2
  static_assert(N > 0 && (N & (N - 1)) == 0, "N must be a power of 2");

  // Construct an object at current position
  template <typename T, typename... Args>
    requires MMapType<T>
  T& Emplace(Args&&... args) {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    // Ensure alignment
    write_pos_ = (write_pos_ + alignof(T) - 1) & ~(alignof(T) - 1);
    write_pos_ = (write_pos_ + sizeof(T)) & (N - 1);

    T* ptr = new (&buffer_[write_pos_]) T(std::forward<Args>(args)...);
    latest_pos_ = write_pos_;
    write_pos_ += sizeof(T);
    ++write_count_;
    return *std::launder(ptr);
  }

  // Allocate raw memory of specified size
  std::byte* Allocate(size_type size) {
    // Ensure alignment
    write_pos_ = (write_pos_ + kAlignment - 1) & ~(kAlignment - 1);
    write_pos_ = (write_pos_ + size) & (N - 1);

    std::byte* ptr = &buffer_[write_pos_];
    latest_pos_ = write_pos_;
    write_pos_ += size;
    ++write_count_;
    return ptr;
  }

  // Allocate memory for type T without initialization
  template <typename T>
    requires MMapType<T>
  T& Allocate() {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    // Ensure alignment
    write_pos_ = (write_pos_ + alignof(T) - 1) & ~(alignof(T) - 1);
    write_pos_ = (write_pos_ + sizeof(T)) & (N - 1);

    T& ref = *std::launder(reinterpret_cast<T*>(&buffer_[write_pos_]));
    latest_pos_ = write_pos_;
    write_pos_ += sizeof(T);
    ++write_count_;
    return ref;
  }

  // Push data into the memory pool
  std::byte* Push(const void* data, size_type size) {
    // Ensure alignment
    write_pos_ = (write_pos_ + kAlignment - 1) & ~(kAlignment - 1);
    write_pos_ = (write_pos_ + size) & (N - 1);

    std::byte* ptr = &buffer_[write_pos_];
    std::memcpy(ptr, data, size);
    latest_pos_ = write_pos_;
    write_pos_ += size;
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
    return *std::launder(reinterpret_cast<T*>(&buffer_[offset]));
  }

  template <typename T>
    requires MMapType<T>
  const T& Read(size_type offset) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > N) {
        throw std::out_of_range("Read position out of range");
      }
    }
    return *std::launder(reinterpret_cast<const T*>(&buffer_[offset]));
  }

  template <typename T>
    requires MMapType<T>
  T& operator[](size_type offset) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > N) {
        throw std::out_of_range("Index out of range");
      }
    }
    return *std::launder(reinterpret_cast<T*>(&buffer_[offset]));
  }

  template <typename T>
    requires MMapType<T>
  const T& operator[](size_type offset) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (offset + sizeof(T) > N) {
        throw std::out_of_range("Index out of range");
      }
    }
    return *std::launder(reinterpret_cast<const T*>(&buffer_[offset]));
  }

  // Get current write position
  [[nodiscard]] size_type write_pos() const {
    return write_pos_;
  }

  // Get total number of writes
  [[nodiscard]] size_type write_count() const {
    return write_count_;
  }

  // Get available space
  [[nodiscard]] size_type available_space() const {
    return N - write_pos_;
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

  const std::byte* data() const {
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

  const std::byte* At(size_type pos) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= N) {
        throw std::out_of_range("Position out of range");
      }
    }
    return &buffer_[pos];
  }

 private:
  // Underlying storage with alignment
  alignas(kAlignment) std::array<std::byte, N> buffer_;
  // Current write position
  size_type write_pos_ = 0;
  // Most recently written position
  size_type latest_pos_ = 0;
  // Total number of writes
  size_type write_count_ = 0;
};

}  // namespace static_impl

}  // namespace nova