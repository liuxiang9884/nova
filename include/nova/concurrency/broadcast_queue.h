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

    // Calculate required space: size field + padding + T object
    constexpr size_type size_field_size = sizeof(size_type);
    constexpr size_type object_offset = AlignUp<alignof(T)>(size_field_size);
    constexpr size_type total_size = object_offset + sizeof(T);

    // Get current write position
    size_type current_pos = write_pos_.load(std::memory_order_relaxed);

    // Check if we need to wrap around
    if (current_pos + total_size > N) {
      // Mark the end of valid data before wrapping
      last_valid_pos_.store(current_pos, std::memory_order_release);
      current_pos = 0;  // Wrap to beginning, overwriting old data
    }

    // Store the total size used by this entry
    auto size_ptr = buffer_.data() + current_pos;
    *reinterpret_cast<size_type*>(size_ptr) = total_size;

    // Construct T object at aligned position
    auto object_ptr = buffer_.data() + current_pos + object_offset;
    T* ptr = new (object_ptr) T(std::forward<Args>(args)...);

    // Update write position atomically
    size_type new_write_pos = current_pos + total_size;
    write_pos_.store(new_write_pos, std::memory_order_release);

    // Update last valid position if we didn't wrap
    if (current_pos != 0) {
      last_valid_pos_.store(new_write_pos, std::memory_order_release);
    }

    return *ptr;
  }

  template <typename T>
  std::optional<T*> TryRead(size_type& read_pos) noexcept {
    static_assert(alignof(T) <= kAlignment,
                  "Type alignment exceeds pool alignment");

    // Get current write position and last valid position
    size_type current_write_pos = write_pos_.load(std::memory_order_acquire);
    size_type last_valid = last_valid_pos_.load(std::memory_order_acquire);

    // Handle wrap around for read position
    if (read_pos >= N) {
      read_pos = 0;
    }

    // Simple check: if positions are equal, no new data
    if (read_pos == current_write_pos) {
      return std::nullopt;
    }

    // Check if we're reading beyond the valid data range
    if (current_write_pos < last_valid) {
      // Writer has wrapped around
      if (read_pos >= last_valid) {
        // We've reached the end of valid data, wrap to beginning
        read_pos = 0;
        if (read_pos == current_write_pos) {
          return std::nullopt;
        }
      }
    } else {
      // Normal case: no wrap
      if (read_pos >= current_write_pos) {
        return std::nullopt;
      }
    }

    // Ensure we have space to read at least the size field
    if (read_pos + sizeof(size_type) > N) {
      read_pos = 0;
      if (read_pos == current_write_pos) {
        return std::nullopt;
      }
    }

    // Read the size of this entry
    auto size_ptr = buffer_.data() + read_pos;
    size_type entry_size = *reinterpret_cast<const size_type*>(size_ptr);

    // Validate entry size
    if (entry_size == 0 || entry_size > N) {
      return std::nullopt;
    }

    // Check if this entry would extend beyond buffer
    if (read_pos + entry_size > N) {
      return std::nullopt;
    }

    // Additional check: ensure we don't read beyond valid data
    if (current_write_pos < last_valid) {
      // Writer has wrapped
      if (read_pos < current_write_pos || read_pos >= last_valid) {
        // We're either in the new data area or beyond valid data
        if (read_pos >= last_valid) {
          return std::nullopt;  // Beyond valid data
        }
      }
    } else {
      // Normal case
      if (read_pos + entry_size > current_write_pos) {
        return std::nullopt;  // Would read into unwritten area
      }
    }

    // Calculate object position
    constexpr size_type object_offset = AlignUp<alignof(T)>(sizeof(size_type));
    auto object_ptr = buffer_.data() + read_pos + object_offset;

    // Advance read position
    read_pos += entry_size;

    // Return pointer to the object
    return reinterpret_cast<T*>(object_ptr);
  }

  // Get current write position for new readers
  size_type GetCurrentWritePos() const noexcept {
    return write_pos_.load(std::memory_order_acquire);
  }

 private:
  alignas(nova::kCacheLineSize) std::array<std::byte, N> buffer_;
  alignas(nova::kCacheLineSize) std::atomic<uint64_t> write_pos_{0};
  alignas(nova::kCacheLineSize) std::atomic<uint64_t> last_valid_pos_{0};
};
};  // namespace static_impl

}  // namespace nova
