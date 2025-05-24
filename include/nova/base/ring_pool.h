#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "nova/common/macros.h"

namespace nova {

// Low-level static ring memory pool implementation that supports arbitrary
// types and data lengths. For shared memory usage, type T must satisfy strict
// constraints.
template <std::size_t N>
class StaticRingPool {
 public:
  using size_type = std::size_t;

  // Construct an object at current position
  template <typename T, typename... Args>
    requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
             std::is_trivially_copyable_v<T> &&
             std::is_default_constructible_v<T> &&
             std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
  T& Emplace(Args&&... args) {
    if (write_pos_ + sizeof(T) >= N) [[unlikely]] {
      write_pos_ = 0;
    }

    T* ptr = new (&buffer_[write_pos_]) T(std::forward<Args>(args)...);
    latest_pos_ = write_pos_;
    write_pos_ += sizeof(T);
    ++write_count_;
    return *ptr;
  }

  // Allocate raw memory of specified size
  std::byte* Allocate(size_type size) {
    if (write_pos_ + size >= N) [[unlikely]] {
      write_pos_ = 0;
    }

    std::byte* ptr = &buffer_[write_pos_];
    latest_pos_ = write_pos_;
    write_pos_ += size;
    ++write_count_;
    return ptr;
  }

  // Allocate memory for type T without initialization
  template <typename T>
    requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
             std::is_trivially_copyable_v<T> &&
             std::is_default_constructible_v<T> &&
             std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
  T& Allocate() {
    if (write_pos_ + sizeof(T) >= N) [[unlikely]] {
      write_pos_ = 0;
    }

    T& ref = reinterpret_cast<T&>(buffer_[write_pos_]);
    latest_pos_ = write_pos_;
    write_pos_ += sizeof(T);
    ++write_count_;
    return ref;
  }

  // Push data into the memory pool
  std::byte* Push(const void* data, size_type size) {
    if (write_pos_ + size >= N) [[unlikely]] {
      write_pos_ = 0;
    }

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
    requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
             std::is_trivially_copyable_v<T> &&
             std::is_default_constructible_v<T> &&
             std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
  T& Read(size_type offset) {
    return reinterpret_cast<T&>(buffer_[offset]);
  }

  template <typename T>
    requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
             std::is_trivially_copyable_v<T> &&
             std::is_default_constructible_v<T> &&
             std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
  const T& Read(size_type offset) const {
    return reinterpret_cast<T&>(buffer_[offset]);
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
  // Underlying storage
  std::array<std::byte, N> buffer_;
  // Current write position
  size_type write_pos_{0};
  // Total number of writes
  size_type write_count_{0};
  // Latest written position
  size_type latest_pos_{0};
};

}  // namespace nova