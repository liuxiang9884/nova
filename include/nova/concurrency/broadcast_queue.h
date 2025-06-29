//
// Created by liuxiang on 2025/6/29.
//

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <new>

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

 private:
  alignas(nova::kCacheLineSize) std::array<std::byte, N> buffer_;
  alignas(nova::kCacheLineSize) std::atomic<uint64_t> write_pos_{0};
  uint64_t cached_write_pos_{0};
};
};  // namespace static_impl

}  // namespace nova
