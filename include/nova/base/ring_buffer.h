//
// Created by liuxiang on 2025/5/24.
//

#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "nova/common/hardware.h"

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

    // Calculate the smallest power of 2 >= n
    capacity_ = NextPowerOfTwo(n);
    mask_ = capacity_ - 1;  // For efficient modulo operation
    buffer_.resize(capacity_);
    write_pos_ = 0;
  }

  // Copy constructor
  RingBuffer(const RingBuffer& other)
      : capacity_(other.capacity_),
        mask_(other.mask_),
        buffer_(other.buffer_),
        write_pos_(other.write_pos_) {}

  // Move constructor
  RingBuffer(RingBuffer&& other) noexcept
      : capacity_(other.capacity_),
        mask_(other.mask_),
        buffer_(std::move(other.buffer_)),
        write_pos_(other.write_pos_) {
    other.capacity_ = 16;
    other.mask_ = 15;
    other.buffer_.resize(16);
    other.write_pos_ = 0;
  }

  // Copy assignment operator
  RingBuffer& operator=(const RingBuffer& other) {
    if (this != &other) {
      capacity_ = other.capacity_;
      mask_ = other.mask_;
      buffer_ = other.buffer_;
      write_pos_ = other.write_pos_;
    }
    return *this;
  }

  // Move assignment operator
  RingBuffer& operator=(RingBuffer&& other) noexcept {
    if (this != &other) {
      capacity_ = other.capacity_;
      mask_ = other.mask_;
      buffer_ = std::move(other.buffer_);
      write_pos_ = other.write_pos_;

      other.capacity_ = 16;
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
    buffer_[write_pos_] = item;
    write_pos_ = (write_pos_ + 1) & mask_;
  }

  // Push an element to the buffer (move version)
  void Push(T&& item) {
    buffer_[write_pos_] = std::move(item);
    write_pos_ = (write_pos_ + 1) & mask_;
  }

  // Access element by index relative to current write position
  // Index 0 is the most recently written element
  // Index 1 is the element before that, etc.
  const T& operator[](size_type index) const {
    if (index >= capacity_) {
      throw std::out_of_range("Index out of range");
    }

    // Calculate actual index (going backwards from write position)
    size_type actual_index = (write_pos_ + capacity_ - 1 - index) & mask_;
    return buffer_[actual_index];
  }

  // Access element by index (non-const version)
  T& operator[](size_type index) {
    if (index >= capacity_) {
      throw std::out_of_range("Index out of range");
    }

    // Calculate actual index (going backwards from write position)
    size_type actual_index = (write_pos_ + capacity_ - 1 - index) & mask_;
    return buffer_[actual_index];
  }

  // Get element at specific absolute position in buffer
  const T& At(size_type pos) const {
    if (pos >= capacity_) {
      throw std::out_of_range("Position out of range");
    }
    return buffer_[pos];
  }

  // Get element at specific absolute position in buffer (non-const version)
  T& At(size_type pos) {
    if (pos >= capacity_) {
      throw std::out_of_range("Position out of range");
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
  [[nodiscard]] size_type Capacity() const {
    return capacity_;
  }

  // Get current write position
  [[nodiscard]] size_type WritePosition() const {
    return write_pos_;
  }

  // Get the most recently written element
  const T& Latest() const {
    if (write_pos_ == 0) {
      return buffer_[capacity_ - 1];
    }
    return buffer_[write_pos_ - 1];
  }

  // Get the most recently written element (non-const version)
  T& Latest() {
    if (write_pos_ == 0) {
      return buffer_[capacity_ - 1];
    }
    return buffer_[write_pos_ - 1];
  }

 private:
  // Calculate the next power of 2 greater than or equal to n
  static size_type NextPowerOfTwo(size_type n) {
    if (n <= 1) return 1;

    // Handle the case where n is already a power of 2
    if ((n & (n - 1)) == 0) {
      return n;
    }

    // Find the next power of 2
    size_type power = 1;
    while (power < n) {
      power <<= 1;
    }
    return power;
  }

 private:
  // Buffer capacity (always a power of 2)
  size_type capacity_;
  // Mask for efficient modulo operation (capacity - 1)
  size_type mask_;
  // Underlying storage
  std::vector<T> buffer_;
  // Current write position
  size_type write_pos_;
};

}  // namespace nova
