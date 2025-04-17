//
// Created by liuxiang on 2025/4/11.
//

#pragma once

#include <atomic>
#include <cassert>
#include <type_traits>

#include "nova/common/hardware.h"

namespace nova {

template <typename T>
using StaticMappedType = T;

template <typename T, std::size_t Capacity>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
           std::is_trivially_copyable_v<T> &&
           std::is_default_constructible_v<T> &&
           std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
class alignas(nova::kCacheLineSize) StaticSPSCQueue {
 public:
  static_assert(Capacity >= 2, "Capacity must be at least 2");
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");

  StaticSPSCQueue() = default;

  ~StaticSPSCQueue() = default;

  StaticSPSCQueue(const StaticSPSCQueue &) = delete;

  StaticSPSCQueue(StaticSPSCQueue &&) = delete;

  StaticSPSCQueue &operator=(const StaticSPSCQueue &) = delete;

  StaticSPSCQueue &operator=(StaticSPSCQueue &&) = delete;

  std::size_t size() const noexcept {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_relaxed);
    return head > tail ? head - tail : head + (Capacity - tail);
  }

  template <typename... Args>
  void Emplace(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    static_assert(std::is_constructible_v<T, Args &&...>,
                  "T must be constructible with Args&&...");
    const auto current = head_.load(std::memory_order_relaxed);
    const auto next = (current + 1) & kMask;
    while (next == cached_tail_) {
      cached_tail_ = tail_.load(std::memory_order_acquire);
    }
    new (&data_[current]) T(std::forward<Args>(args)...);
    head_.store(next, std::memory_order_release);
  }

  template <typename... Args>
  bool TryEmplace(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    static_assert(std::is_constructible_v<T, Args &&...>,
                  "T must be constructible with Args&&...");
    const auto current = head_.load(std::memory_order_relaxed);
    const auto next = (current + 1) & kMask;
    if (next == cached_tail_) {
      cached_tail_ = tail_.load(std::memory_order_acquire);
      if (next == cached_tail_) {
        return false;
      }
    }
    new (&data_[current]) T(std::forward<Args>(args)...);
    head_.store(next, std::memory_order_release);
    return true;
  }

  [[nodiscard]] T *Front() noexcept {
    const auto current = tail_.load(std::memory_order_relaxed);
    if (current == cached_head_) {
      cached_head_ = head_.load(std::memory_order_acquire);
      if (current == cached_head_) {
        return nullptr;
      }
    }
    return &data_[current];
  }

  /**
   * Removes the front element from the queue.
   * PRECONDITION: Front() must have been called and returned non-nullptr.
   * Using this method when the queue is empty leads to undefined behavior.
   */
  void Pop() noexcept {
    const auto current = tail_.load(std::memory_order_relaxed);
    assert(head_.load(std::memory_order_acquire) != current &&
           "pop can only be called when front() is non-nullptr(queue is not "
           "empty)");
    data_[current].~T();
    const auto next = (current + 1) & kMask;
    tail_.store(next, std::memory_order_release);
  }

  bool TryPop(T &value) noexcept {
    auto current = tail_.load(std::memory_order_relaxed);
    if (current == cached_head_) {
      cached_head_ = head_.load(std::memory_order_acquire);
      if (current == cached_head_) {
        return false;
      }
    }

    value = data_[current];
    data_[current].~T();
    const auto next = (current + 1) & kMask;
    tail_.store(next, std::memory_order_release);
    return true;
  }

  bool empty() const noexcept {
    return size() == 0;
  }

  bool full() const noexcept {
    return size() == Capacity;
  }

 private:
  static constexpr std::size_t kMask = Capacity - 1;
  T data_[Capacity];
  alignas(nova::kCacheLineSize) std::atomic<std::size_t> head_{0};
  alignas(nova::kCacheLineSize) std::size_t cached_tail_{0};
  alignas(nova::kCacheLineSize) std::atomic<std::size_t> tail_{0};
  alignas(nova::kCacheLineSize) std::size_t cached_head_{0};
};

template <typename T>
class alignas(nova::kCacheLineSize) SPSCQueue {
 public:
  // SPSCQueue 实现...
 private:
  // SPSCQueue 私有成员...
};

}  // namespace nova
