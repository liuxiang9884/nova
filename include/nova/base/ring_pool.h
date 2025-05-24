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

// Low-level ring memory pool implementation that supports arbitrary types and
// data lengths
template <std::size_t N>
class RingPool {
 public:
  using size_type = std::size_t;
  static_assert(N > 0 && (N & (N - 1)) == 0, "N must be a power of 2");

  // Construct an object at current position
  template <typename T, typename... Args>
  T* Emplace(Args&&... args) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (write_pos_ + sizeof(T) > N) {
        throw std::runtime_error("Not enough space for type T");
      }
    }

    if (write_pos_ + sizeof(T) >= N) [[unlikely]] {
      write_pos_ = 0;
    }

    T* ptr = new (&buffer_[write_pos_]) T(std::forward<Args>(args)...);
    write_pos_ += sizeof(T);
    ++write_count_;
    return ptr;
  }

  // Get raw memory of specified size
  std::byte* Get(size_type size) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (size > N) {
        throw std::runtime_error("Requested size exceeds pool capacity");
      }
    }

    if (write_pos_ + size >= N) [[unlikely]] {
      write_pos_ = 0;
    }

    std::byte* ptr = &buffer_[write_pos_];
    write_pos_ += size;
    ++write_count_;
    return ptr;
  }

  // Copy data into the memory pool
  std::byte* Copy(const void* data, size_type size) {
    if constexpr (NOVA_DEBUG_MODE) {
      if (size > N) {
        throw std::runtime_error("Data size exceeds pool capacity");
      }
    }

    if (write_pos_ + size >= N) [[unlikely]] {
      write_pos_ = 0;
    }

    std::byte* ptr = &buffer_[write_pos_];
    std::memcpy(ptr, data, size);
    write_pos_ += size;
    ++write_count_;
    return ptr;
  }

  // Get current write position
  [[nodiscard]] size_type WritePosition() const {
    return write_pos_;
  }

  // Get total number of writes
  [[nodiscard]] size_type WriteCount() const {
    return write_count_;
  }

  // Get available space
  [[nodiscard]] size_type AvailableSpace() const {
    return N - write_pos_;
  }

  // Get total capacity
  [[nodiscard]] constexpr size_type Capacity() const {
    return N;
  }

  // Reset write position and count
  void Reset() {
    write_pos_ = 0;
    write_count_ = 0;
  }

  // Direct access to underlying memory (use with caution)
  std::byte* Data() {
    return buffer_.data();
  }

  const std::byte* Data() const {
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
  std::array<std::byte, N> buffer_;  // Underlying storage
  size_type write_pos_{0};           // Current write position
  size_type write_count_{0};         // Total number of writes
};

}  // namespace nova