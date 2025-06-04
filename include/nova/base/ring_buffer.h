//
// Created by liuxiang on 2025/5/24.
//

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "nova/common/macros.h"

namespace nova {

// Ring buffer implementation using vector as underlying storage
// Only tracks write position, capacity is always a power of 2
template <typename T>
class RingBuffer {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using reference = T&;
  using const_reference = const T&;

  // Constructor with capacity parameter
  // The actual capacity will be the smallest power of 2 greater than or equal
  // to n
  explicit RingBuffer(size_type n = 16) {
    if (n == 0) {
      throw std::invalid_argument("RingBuffer capacity must be greater than 0");
    }

    // Calculate mask for the smallest power of 2 >= n
    mask_ = std::bit_ceil(n) - 1;
    buffer_.resize(mask_ + 1);
    write_pos_ = 0;
  }

  // Copy constructor
  RingBuffer(const RingBuffer& other)
      : mask_(other.mask_),
        buffer_(other.buffer_),
        write_pos_(other.write_pos_) {}

  // Move constructor
  RingBuffer(RingBuffer&& other) noexcept
      : mask_(other.mask_),
        buffer_(std::move(other.buffer_)),
        write_pos_(other.write_pos_) {
    other.mask_ = 15;
    other.buffer_.resize(16);
    other.write_pos_ = 0;
  }

  // Copy assignment operator
  RingBuffer& operator=(const RingBuffer& other) {
    if (this != &other) {
      mask_ = other.mask_;
      buffer_ = other.buffer_;
      write_pos_ = other.write_pos_;
    }
    return *this;
  }

  // Move assignment operator
  RingBuffer& operator=(RingBuffer&& other) noexcept {
    if (this != &other) {
      mask_ = other.mask_;
      buffer_ = std::move(other.buffer_);
      write_pos_ = other.write_pos_;

      other.mask_ = 15;
      other.buffer_.resize(16);
      other.write_pos_ = 0;
    }
    return *this;
  }

  // Destructor
  ~RingBuffer() = default;

  // Push an element to the buffer at current write position
  void Push(const T& item) {
    buffer_[write_pos_ & mask_] = item;
    ++write_pos_;
  }

  // Push an element to the buffer (move version)
  void Push(T&& item) {
    buffer_[write_pos_ & mask_] = std::move(item);
    ++write_pos_;
  }

  // Emplace an element directly in the buffer at current write position
  template <typename... Args>
  T& Emplace(Args&&... args) {
    T& item = buffer_[write_pos_ & mask_];
    item = T(std::forward<Args>(args)...);
    ++write_pos_;
    return item;
  }

  // Allocate memory at current write position without initialization
  // Returns reference to the allocated memory
  T& Allocate() {
    T& item = buffer_[write_pos_ & mask_];
    ++write_pos_;
    return item;
  }

  // Access element by index from the beginning of writes
  // Index 0 is the first written element, index 1 is the second, etc.
  const T& operator[](size_type index) const {
    return buffer_[index & mask_];
  }

  // Access element by index (non-const version)
  T& operator[](size_type index) {
    return buffer_[index & mask_];
  }

  // Get element at specific absolute position in buffer
  const T& At(size_type pos) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= mask_ + 1) {
        throw std::out_of_range("Position out of range");
      }
    }
    return buffer_[pos];
  }

  // Get element at specific absolute position in buffer (non-const version)
  T& At(size_type pos) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= mask_ + 1) {
        throw std::out_of_range("Position out of range");
      }
    }
    return buffer_[pos];
  }

  // Clear all elements from the buffer
  void Clear() {
    write_pos_ = 0;
    // Optionally clear the vector content
    std::fill(buffer_.begin(), buffer_.end(), T{});
  }

  // Get the capacity of the buffer
  [[nodiscard]] size_type capacity() const {
    return mask_ + 1;
  }

  // Get current write position
  [[nodiscard]] size_type write_position() const {
    return write_pos_ & mask_;
  }

  // Get latest write position
  [[nodiscard]] constexpr size_type latest_pos() const {
    return (write_pos_ - 1) & mask_;
  }

  // Get the number of elements written so far
  [[nodiscard]] size_type write_count() const {
    return write_pos_;
  }

  // Get the most recently written element
  const T& latest() const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (write_pos_ == 0) {
        throw std::out_of_range("No elements written yet");
      }
    }
    return buffer_[(write_pos_ - 1) & mask_];
  }

  // Get the most recently written element (non-const version)
  T& latest() {
    if constexpr (NOVA_DEBUG_MODE) {
      if (write_pos_ == 0) {
        throw std::out_of_range("No elements written yet");
      }
    }
    return buffer_[(write_pos_ - 1) & mask_];
  }

 private:
  // Mask for efficient modulo operation (capacity - 1)
  size_type mask_;
  // Underlying storage
  std::vector<T> buffer_;
  // Total number of elements written (also serves as write position)
  size_type write_pos_;
};

namespace static_impl {

// Static ring buffer implementation using std::array as underlying storage
// Capacity N must be a power of 2 and is fixed at compile time
//
// For shared memory usage, type T must satisfy strict constraints
template <typename T, std::size_t N>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
class RingBuffer {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using reference = T&;
  using const_reference = const T&;

  // Compile-time check that N is a power of 2
  static_assert(N > 0 && (N & (N - 1)) == 0, "Capacity N must be a power of 2");

  // Default constructor
  constexpr RingBuffer() : write_pos_(0) {}

  // Copy constructor
  RingBuffer(const RingBuffer& other) = default;

  // Move constructor
  RingBuffer(RingBuffer&& other) noexcept = default;

  // Copy assignment operator
  RingBuffer& operator=(const RingBuffer& other) = default;

  // Move assignment operator
  RingBuffer& operator=(RingBuffer&& other) noexcept = default;

  // Destructor
  ~RingBuffer() = default;

  // Push an element to the buffer at current write position
  void Push(const T& item) {
    buffer_[write_pos_ & kMask] = item;
    ++write_pos_;
  }

  // Push an element to the buffer (move version)
  void Push(T&& item) {
    buffer_[write_pos_ & kMask] = std::move(item);
    ++write_pos_;
  }

  // Emplace an element directly in the buffer at current write position
  template <typename... Args>
  constexpr T& Emplace(Args&&... args) {
    T& item = buffer_[write_pos_ & kMask];
    item = T(std::forward<Args>(args)...);
    ++write_pos_;
    return item;
  }

  // Allocate memory at current write position without initialization
  // Returns reference to the allocated memory
  constexpr T& Allocate() {
    T& item = buffer_[write_pos_ & kMask];
    ++write_pos_;
    return item;
  }

  // Access element by index from the beginning of writes
  // Index 0 is the first written element, index 1 is the second, etc.
  constexpr const T& operator[](size_type index) const {
    return buffer_[index & kMask];
  }

  // Access element by index (non-const version)
  constexpr T& operator[](size_type index) {
    return buffer_[index & kMask];
  }

  // Get element at specific absolute position in buffer
  constexpr const T& At(size_type pos) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= N) {
        throw std::out_of_range("Position out of range");
      }
    }
    return buffer_[pos];
  }

  // Get element at specific absolute position in buffer (non-const version)
  constexpr T& At(size_type pos) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (pos >= N) {
        throw std::out_of_range("Position out of range");
      }
    }
    return buffer_[pos];
  }

  // Clear all elements from the buffer
  void Clear() {
    write_pos_ = 0;
    // Optionally clear the array content
    std::fill(buffer_.begin(), buffer_.end(), T{});
  }

  // Get the capacity of the buffer
  [[nodiscard]] constexpr size_type capacity() const {
    return N;
  }

  // Get current write position in buffer
  [[nodiscard]] constexpr size_type write_position() const {
    return write_pos_ & kMask;
  }

  // Get latest write position
  [[nodiscard]] constexpr size_type latest_pos() const {
    return (write_pos_ - 1) & kMask;
  }

  // Get the number of elements written so far
  [[nodiscard]] constexpr size_type write_count() const {
    return write_pos_;
  }

  // Get the most recently written element
  constexpr const T& latest() const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (write_pos_ == 0) {
        throw std::out_of_range("No elements written yet");
      }
    }
    return buffer_[(write_pos_ - 1) & kMask];
  }

  // Get the most recently written element (non-const version)
  constexpr T& latest() {
    if constexpr (NOVA_DEBUG_MODE) {
      if (write_pos_ == 0) {
        throw std::out_of_range("No elements written yet");
      }
    }
    return buffer_[(write_pos_ - 1) & kMask];
  }

  // Check if buffer is empty
  [[nodiscard]] constexpr bool IsEmpty() const {
    return write_pos_ == 0;
  }

  // Get iterator to beginning of valid data
  constexpr auto begin() const {
    return buffer_.begin();
  }

  // Get iterator to end of buffer
  constexpr auto end() const {
    return buffer_.end();
  }

  // Get iterator to beginning of valid data (non-const)
  constexpr auto begin() {
    return buffer_.begin();
  }

  // Get iterator to end of buffer (non-const)
  constexpr auto end() {
    return buffer_.end();
  }

 private:
  // Compile-time mask for efficient modulo operation
  static constexpr size_type kMask = N - 1;

  // Underlying storage
  std::array<T, N> buffer_;
  // Total number of elements written (also serves as write position)
  size_type write_pos_;
};

}  // namespace static_impl
}  // namespace nova
