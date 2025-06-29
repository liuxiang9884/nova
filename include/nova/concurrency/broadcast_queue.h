//
// Created by liuxiang on 2025/6/29.
//

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <new>
#include <optional>

#include "nova/common/hardware.h"

namespace nova {

namespace static_impl {

template <std::size_t N>
class alignas(nova::kCacheLineSize) FlexibleSPBroadcastQueue {
 public:
  using size_type = std::size_t;
  static constexpr size_type kAlignment = alignof(std::max_align_t);

  template <std::size_t Alignment = kAlignment>
  static constexpr size_type AlignUp(size_type value) noexcept {
    return (value + Alignment - 1) & ~(Alignment - 1);
  }

  template <typename T, typename... Args>
  T& Emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>) {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    // Calculate current position's write_size
    size_type object_start_pos =
        AlignUp<alignof(T)>(cached_write_pos_ + sizeof(size_type));
    size_type write_size = object_start_pos - cached_write_pos_ + sizeof(T);

    // Check if we need to wrap around
    if (cached_write_pos_ + write_size > buffer_.size()) [[unlikely]] {
      // Mark wrap around at current position
      auto wrap_ptr = buffer_.data() + cached_write_pos_;
      *reinterpret_cast<size_type*>(wrap_ptr) =
          0;  // 0 indicates wrap to beginning

      cached_write_pos_ = 0;
      object_start_pos =
          AlignUp<alignof(T)>(sizeof(size_type));  // Simplified calculation
      write_size = object_start_pos + sizeof(T);
    }

    // Write size field first (store total write_size for reader navigation)
    auto write_ptr = buffer_.data() + cached_write_pos_;

    // Write T object at properly aligned absolute position
    auto object_ptr = buffer_.data() + object_start_pos;
    T* ptr = new (object_ptr) T(std::forward<Args>(args)...);

    // Update cached position
    cached_write_pos_ += write_size;
    *reinterpret_cast<size_type*>(write_ptr) = cached_write_pos_;

    // Update atomic position for readers
    write_pos_.store(cached_write_pos_, std::memory_order_release);

    return *ptr;
  }

  // Reader interface - each reader maintains its own position
  template <typename T>
  std::optional<T*> TryRead(size_type& reader_pos) noexcept {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    // Get current write position
    size_type current_write_pos = write_pos_.load(std::memory_order_acquire);

    // Check if there's new data available
    if (reader_pos >= current_write_pos) {
      return std::nullopt;  // No new data
    }

    // Handle wrap around: if reader_pos >= buffer_size, wrap to 0
    if (reader_pos >= buffer_.size()) {
      reader_pos = 0;
      // Check again after wrap
      if (reader_pos >= current_write_pos) {
        return std::nullopt;
      }
    }

    // Read next write position
    auto pos_ptr = buffer_.data() + reader_pos;
    size_type next_pos = *reinterpret_cast<const size_type*>(pos_ptr);

    // Handle wrap around marker
    if (next_pos == 0) {
      reader_pos = 0;  // Wrap to beginning
      // Check if there's data after wrap
      if (reader_pos >= current_write_pos) {
        return std::nullopt;
      }
      // Re-read from beginning
      pos_ptr = buffer_.data() + reader_pos;
      next_pos = *reinterpret_cast<const size_type*>(pos_ptr);
    }

    // Calculate object position
    size_type object_offset = AlignUp<alignof(T)>(sizeof(size_type));
    auto object_ptr = pos_ptr + object_offset;

    // Update reader position to next entry
    reader_pos = next_pos;

    // Return pointer to the object
    return reinterpret_cast<T*>(object_ptr);
  }

  // Get current write position for new readers
  size_type GetCurrentWritePos() const noexcept {
    return write_pos_.load(std::memory_order_acquire);
  }

  // Check if reader has caught up with writer
  bool HasNewData(size_type reader_pos) const noexcept {
    size_type current_write_pos = write_pos_.load(std::memory_order_acquire);
    return reader_pos < current_write_pos;
  }

 private:
  alignas(nova::kCacheLineSize) std::array<std::byte, N> buffer_;
  alignas(nova::kCacheLineSize) std::atomic<uint64_t> write_pos_{0};
  uint64_t cached_write_pos_{0};
};
};  // namespace static_impl

}  // namespace nova
