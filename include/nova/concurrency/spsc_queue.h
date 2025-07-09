//
// Created by liuxiang on 2025/4/11.
//

#pragma once

#include <atomic>
#include <bit>
#include <cassert>
#include <memory>
#include <type_traits>

#include "nova/common/hardware.h"

namespace nova {

template <typename T>
using MappedType = T;

namespace static_impl {

template <typename T, std::size_t Capacity>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T>
class SPSCQueue {
 public:
  static_assert(Capacity >= 2, "Capacity must be at least 2");
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");

  SPSCQueue() = default;

  ~SPSCQueue() = default;

  SPSCQueue(const SPSCQueue &) = delete;

  SPSCQueue(SPSCQueue &&) = delete;

  SPSCQueue &operator=(const SPSCQueue &) = delete;

  SPSCQueue &operator=(SPSCQueue &&) = delete;

  std::size_t size() const noexcept {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_relaxed);
    return head > tail ? head - tail : head + Capacity - tail;
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

    value = std::move(data_[current]);
    data_[current].~T();
    const auto next = (current + 1) & kMask;
    tail_.store(next, std::memory_order_release);
    return true;
  }

  void Push(const T &value) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    Emplace(value);
  }

  template <typename P, typename = typename std::enable_if_t<
                            std::is_constructible_v<T, P &&>>>
  void Push(P &&v) noexcept(std::is_nothrow_constructible<T, P &&>::value) {
    Emplace(std::forward<P>(v));
  }

  bool TryPush(const T &value) noexcept(
      std::is_nothrow_copy_constructible_v<T>) {
    return TryEmplace(value);
  }

  template <typename P, typename = typename std::enable_if_t<
                            std::is_constructible_v<T, P &&>>>
  bool TryPush(P &&v) noexcept(std::is_nothrow_constructible<T, P &&>::value) {
    return TryEmplace(std::forward<P>(v));
  }

  bool Empty() const noexcept {
    return size() == 0;
  }

  bool Full() const noexcept {
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

}  // namespace static_impl

template <typename T, typename Allocator = std::allocator<T>>
class SPSCQueue {
 public:
  explicit SPSCQueue(std::size_t n, Allocator allocator = Allocator{})
      : allocator_{allocator},
        mask_{std::bit_ceil(n) - 1},
        data_{std::allocator_traits<Allocator>::allocate(allocator_,
                                                         capacity())} {}

  ~SPSCQueue() {
    while (Front()) {
      Pop();
    }
    std::allocator_traits<Allocator>::deallocate(allocator_, data_, capacity());
  }

  SPSCQueue(const SPSCQueue &) = delete;

  SPSCQueue(SPSCQueue &&) = delete;

  SPSCQueue &operator=(const SPSCQueue &) = delete;

  SPSCQueue &operator=(SPSCQueue &&) = delete;

  template <typename... Args>
  void Emplace(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    static_assert(std::is_constructible_v<T, Args &&...>,
                  "T must be constructible with Args&&...");
    const auto current = head_.load(std::memory_order_relaxed);
    const auto next = (current + 1) & mask_;
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
    const auto next = (current + 1) & mask_;
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
    const auto next = (current + 1) & mask_;
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

    value = std::move(data_[current]);
    data_[current].~T();
    const auto next = (current + 1) & mask_;
    tail_.store(next, std::memory_order_release);
    return true;
  }

  void Push(const T &value) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    Emplace(value);
  }

  template <typename P, typename = typename std::enable_if_t<
                            std::is_constructible_v<T, P &&>>>
  void Push(P &&v) noexcept(std::is_nothrow_constructible<T, P &&>::value) {
    Emplace(std::forward<P>(v));
  }

  bool TryPush(const T &value) noexcept(
      std::is_nothrow_copy_constructible_v<T>) {
    return TryEmplace(value);
  }

  template <typename P, typename = typename std::enable_if_t<
                            std::is_constructible_v<T, P &&>>>
  bool TryPush(P &&v) noexcept(std::is_nothrow_constructible<T, P &&>::value) {
    return TryEmplace(std::forward<P>(v));
  }

  std::size_t size() const noexcept {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_relaxed);
    return head > tail ? head - tail : head + capacity() - tail;
  }

  bool Empty() const noexcept {
    return size() == 0;
  }

  bool Full() const noexcept {
    return size() == capacity();
  }

  std::size_t capacity() const noexcept {
    return mask_ + 1;
  }

 private:
  Allocator allocator_ [[no_unique_address]];
  std::size_t mask_{0};
  T *data_{nullptr};
  alignas(nova::kCacheLineSize) std::atomic<std::size_t> head_{0};
  alignas(nova::kCacheLineSize) std::size_t cached_tail_{0};
  alignas(nova::kCacheLineSize) std::atomic<std::size_t> tail_{0};
  alignas(nova::kCacheLineSize) std::size_t cached_head_{0};
};

}  // namespace nova
