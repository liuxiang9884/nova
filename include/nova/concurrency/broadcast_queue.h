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
    if (cached_write_pos_ + write_size > buffer_.size() - sizeof(size_type))
        [[unlikely]] {
      memset(buffer_.data() + cached_write_pos_, 0, sizeof(size_type));
      cached_write_pos_ = 0;
      object_start_pos =
          AlignUp<alignof(T)>(sizeof(size_type));  // Simplified calculation
      write_size = object_start_pos + sizeof(T);
    }

    // Write size field first (store total write_size for reader navigation)
    auto write_ptr = buffer_.data() + cached_write_pos_;
    *reinterpret_cast<size_type*>(write_ptr) = write_size;

    // Write T object at properly aligned absolute position
    auto object_ptr = buffer_.data() + object_start_pos;
    T* ptr = new (object_ptr) T(std::forward<Args>(args)...);

    // Update cached position
    cached_write_pos_ += write_size;

    // Update atomic position for readers
    write_pos_.store(cached_write_pos_, std::memory_order_release);

    return *ptr;
  }

  template <typename T>
  std::optional<T*> TryRead(size_type& read_pos) noexcept {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    // Get current write position
    size_type current_write_pos = write_pos_.load(std::memory_order_acquire);

    // Check if there's new data available
    if (read_pos == current_write_pos) {
      return std::nullopt;
    }

    // Read the size field at current position
    auto size_ptr = buffer_.data() + read_pos;
    size_type entry_size = *reinterpret_cast<const size_type*>(size_ptr);

    // Check for wrap-around marker (size = 0)
    if (entry_size == 0) {
      // This is a wrap-around marker, jump to beginning
      read_pos = 0;
      if (read_pos == current_write_pos) {
        return std::nullopt;
      }

      // Read size from the beginning
      size_ptr = buffer_.data() + read_pos;
      entry_size = *reinterpret_cast<const size_type*>(size_ptr);
    }

    // Validate entry size
    if (entry_size == 0 || entry_size > N) {
      return std::nullopt;
    }

    // Ensure we don't read beyond valid data
    // When read_pos >= current_write_pos, it means writer has wrapped around
    // In this case, we can read until we hit a wrap marker (size = 0)
    // The wrap marker check above already handled this case
    if (read_pos < current_write_pos) {
      // Normal case: ensure we don't read beyond write position
      if (read_pos + entry_size > current_write_pos) {
        return std::nullopt;
      }
    }
    // If read_pos >= current_write_pos, we rely on the wrap marker (size = 0)
    // to detect when to wrap around, which was already handled above

    // Calculate object position with proper alignment
    constexpr size_type object_offset = AlignUp<alignof(T)>(sizeof(size_type));
    auto object_ptr = buffer_.data() + read_pos + object_offset;

    // Advance read position
    read_pos += entry_size;

    // Return pointer to the constructed object
    return reinterpret_cast<T*>(object_ptr);
  }

  // Get current write position for new readers
  size_type GetCurrentWritePos() const noexcept {
    return write_pos_.load(std::memory_order_acquire);
  }

 private:
  alignas(nova::kCacheLineSize) std::array<std::byte, N> buffer_;
  alignas(nova::kCacheLineSize) std::atomic<uint64_t> write_pos_{0};
  uint64_t cached_write_pos_{0};
};
};  // namespace static_impl

}  // namespace nova
