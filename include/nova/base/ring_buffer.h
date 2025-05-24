//
// Created by liuxiang on 2025/5/24.
//

#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
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
    write_pos_++;
  }

  // Push an element to the buffer (move version)
  void Push(T&& item) {
    buffer_[write_pos_ & mask_] = std::move(item);
    write_pos_++;
  }

  // Emplace an element directly in the buffer at current write position
  template <typename... Args>
  void Emplace(Args&&... args) {
    buffer_[write_pos_ & mask_] = T(std::forward<Args>(args)...);
    write_pos_++;
  }

  // Access element by index from the beginning of writes
  // Index 0 is the first written element, index 1 is the second, etc.
  const T& operator[](size_type index) const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (index >= WrittenCount()) {
        throw std::out_of_range("Index out of range");
      }
    }

    // Calculate actual index in buffer
    size_type actual_index = index & mask_;
    return buffer_[actual_index];
  }

  // Access element by index (non-const version)
  T& operator[](size_type index) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (index >= WrittenCount()) {
        throw std::out_of_range("Index out of range");
      }
    }

    // Calculate actual index in buffer
    size_type actual_index = index & mask_;
    return buffer_[actual_index];
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
  [[nodiscard]] size_type Capacity() const {
    return mask_ + 1;
  }

  // Get current write position
  [[nodiscard]] size_type WritePosition() const {
    return write_pos_ & mask_;
  }

  // Get the number of elements written so far
  [[nodiscard]] size_type WrittenCount() const {
    return std::min(write_pos_, mask_ + 1);
  }

  // Get the most recently written element
  const T& Latest() const {
    if constexpr (NOVA_DEBUG_MODE) {
      if (write_pos_ == 0) {
        throw std::out_of_range("No elements written yet");
      }
    }
    return buffer_[(write_pos_ - 1) & mask_];
  }

  // Get the most recently written element (non-const version)
  T& Latest() {
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

}  // namespace nova
