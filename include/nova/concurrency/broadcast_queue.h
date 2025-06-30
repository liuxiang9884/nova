//
// Created by liuxiang on 2025/6/29.
//

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <optional>

#include "nova/common/hardware.h"

namespace nova {

namespace static_impl {

template <typename Type, std::size_t N>
class alignas(nova::kCacheLineSize) FlexibleSPBroadcastQueue {
 public:
  using size_type = std::size_t;
  static constexpr size_type kAlignment = alignof(std::max_align_t);

  template <std::size_t Alignment = kAlignment>
  static constexpr size_type AlignUp(size_type value) noexcept {
    return (value + Alignment - 1) & ~(Alignment - 1);
  }

  struct alignas(kAlignment) Header {
    uint32_t length;
    Type type;
  };

  template <typename T, typename... Args>
  T& Emplace(Type type, Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>) {
    // Calculate current position's write_size
    size_type object_start_pos =
        AlignUp<alignof(T)>(cached_write_pos_ + sizeof(Header));
    size_type write_size = object_start_pos - cached_write_pos_ + sizeof(T);

    // Check if we need to wrap around
    if (cached_write_pos_ + write_size > buffer_.size() - sizeof(Header))
        [[unlikely]] {
      HandleWriteWrapAround<T>(object_start_pos, write_size);
    }

    // Write size field first (store total write_size for reader navigation)
    auto header = reinterpret_cast<Header*>(buffer_.data() + cached_write_pos_);
    header->type = type;
    header->length = write_size;

    // Write T object at properly aligned absolute position
    auto object_ptr = buffer_.data() + object_start_pos;
    T* ptr = new (object_ptr) T(std::forward<Args>(args)...);

    // Update cached position
    cached_write_pos_ += write_size;

    // Update atomic position for readers
    write_pos_.store(cached_write_pos_, std::memory_order_release);

    return *ptr;
  }

  // Read entry info and return header with entry position
  struct EntryInfo {
    const Header* header;
    size_type entry_pos;
  };

  std::optional<EntryInfo> TryRead(size_type& read_pos) noexcept {
    // Get current write position
    size_type current_write_pos = write_pos_.load(std::memory_order_acquire);

    // Check if there's new data available
    if (read_pos == current_write_pos) {
      return std::nullopt;
    }

    // Store original entry position
    size_type entry_pos = read_pos;

    // Read the header at current position
    const auto* header =
        reinterpret_cast<const Header*>(buffer_.data() + read_pos);

    // Check for wrap-around marker (length = 0)
    if (header->length == 0) [[unlikely]] {
      read_pos = 0;
      entry_pos = 0;
      header = reinterpret_cast<const Header*>(buffer_.data() + read_pos);
    }

    // Advance read position
    read_pos += header->length;

    return EntryInfo{header, entry_pos};
  }

  // Get typed object pointer based on entry info
  template <typename T>
  T* Get(const EntryInfo& entry_info) noexcept {
    // Calculate object position with same logic as Emplace
    size_type object_start_pos =
        AlignUp<alignof(T)>(entry_info.entry_pos + sizeof(Header));
    return reinterpret_cast<T*>(buffer_.data() + object_start_pos);
  }

  // Get current write position for new readers
  size_type GetCurrentWritePos() const noexcept {
    return write_pos_.load(std::memory_order_acquire);
  }

  // Alternative reading method: direct header access with wrap-around handling
  const Header* GetHeader(size_type& pos) const noexcept {
    const auto* header = reinterpret_cast<const Header*>(buffer_.data() + pos);

    // Handle wrap-around marker (length = 0)
    if (header->length == 0) [[unlikely]] {
      pos = 0;
      header = reinterpret_cast<const Header*>(buffer_.data() + pos);
    }
    pos += header->length;
    return header;
  }

  // Get object pointer at specific position with manual alignment calculation
  template <typename T>
  const T* Get(size_type entry_pos) const noexcept {
    size_type object_start_pos =
        AlignUp<alignof(T)>(entry_pos + sizeof(Header));
    return reinterpret_cast<const T*>(buffer_.data() + object_start_pos);
  }

 private:
  // Handle write wrap-around logic
  template <typename T>
  void HandleWriteWrapAround(size_type& object_start_pos,
                             size_type& write_size) noexcept {
    // Set wrap-around marker at current position
    memset(buffer_.data() + cached_write_pos_, 0, sizeof(Header));

    // Reset to beginning of buffer
    cached_write_pos_ = 0;

    // Recalculate positions for the new location
    object_start_pos = AlignUp<alignof(T)>(sizeof(Header));
    write_size = object_start_pos + sizeof(T);
  }

 private:
  alignas(nova::kCacheLineSize) std::array<std::byte, N> buffer_;
  alignas(nova::kCacheLineSize) std::atomic<uint64_t> write_pos_{0};
  uint64_t cached_write_pos_{0};
};
};  // namespace static_impl

}  // namespace nova
