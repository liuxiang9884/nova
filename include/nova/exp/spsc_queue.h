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

namespace exp {

namespace detail {

/**
 * Base class for SPSC (Single Producer Single Consumer) queue implementations.
 * Provides common functionality shared between different SPSC queue variants.
 *
 * @tparam Derived The derived class (CRTP pattern)
 * @tparam T Type of the elements stored in the queue
 */
template <typename Derived, typename T>
class SPSCQueueBase {
 protected:
  using SizeType = std::size_t;

  SPSCQueueBase() = default;
  ~SPSCQueueBase() = default;

  SPSCQueueBase(const SPSCQueueBase &) = delete;
  SPSCQueueBase(SPSCQueueBase &&) = delete;
  SPSCQueueBase &operator=(const SPSCQueueBase &) = delete;
  SPSCQueueBase &operator=(SPSCQueueBase &&) = delete;

 public:
  /**
   * Get the number of elements in the queue.
   */
  SizeType size() const noexcept {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_relaxed);
    return head > tail
               ? head - tail
               : head + static_cast<const Derived *>(this)->capacity() - tail;
  }

  /**
   * Check if the queue is empty.
   */
  bool Empty() const noexcept {
    return size() == 0;
  }

  /**
   * Check if the queue is full.
   */
  bool Full() const noexcept {
    return size() == static_cast<const Derived *>(this)->capacity();
  }

  /**
   * Push an element using copy semantics.
   */
  void Push(const T &value) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    static_cast<Derived *>(this)->template Emplace<const T &>(value);
  }

  /**
   * Push an element using perfect forwarding.
   */
  template <typename P,
            typename = std::enable_if_t<std::is_constructible_v<T, P &&>>>
  void Push(P &&v) noexcept(std::is_nothrow_constructible_v<T, P &&>) {
    static_cast<Derived *>(this)->template Emplace<P &&>(std::forward<P>(v));
  }

  /**
   * Try to push an element using copy semantics.
   * @return true if the element was pushed, false if the queue was full
   */
  bool TryPush(const T &value) noexcept(
      std::is_nothrow_copy_constructible_v<T>) {
    return static_cast<Derived *>(this)->template TryEmplace<const T &>(value);
  }

  /**
   * Try to push an element using perfect forwarding.
   * @return true if the element was pushed, false if the queue was full
   */
  template <typename P,
            typename = std::enable_if_t<std::is_constructible_v<T, P &&>>>
  bool TryPush(P &&v) noexcept(std::is_nothrow_constructible_v<T, P &&>) {
    return static_cast<Derived *>(this)->template TryEmplace<P &&>(
        std::forward<P>(v));
  }

  /**
   * Get a pointer to the front element without removing it.
   * @return A pointer to the front element, or nullptr if the queue is empty
   */
  [[nodiscard]] T *Front() noexcept {
    const auto current = tail_.load(std::memory_order_relaxed);
    if (current == cached_head_) {
      cached_head_ = head_.load(std::memory_order_acquire);
      if (current == cached_head_) {
        return nullptr;
      }
    }
    return static_cast<Derived *>(this)->element(current);
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

    static_cast<Derived *>(this)->element(current)->~T();
    const auto next = (current + 1) & static_cast<Derived *>(this)->mask();
    tail_.store(next, std::memory_order_release);
  }

  /**
   * Try to pop an element from the queue.
   * @param value Reference to store the popped element
   * @return true if an element was popped, false if the queue was empty
   */
  bool TryPop(T &value) noexcept {
    auto current = tail_.load(std::memory_order_relaxed);
    if (current == cached_head_) {
      cached_head_ = head_.load(std::memory_order_acquire);
      if (current == cached_head_) {
        return false;
      }
    }

    value = std::move(*static_cast<Derived *>(this)->element(current));
    static_cast<Derived *>(this)->element(current)->~T();
    const auto next = (current + 1) & static_cast<Derived *>(this)->mask();
    tail_.store(next, std::memory_order_release);
    return true;
  }

 protected:
  template <typename... Args>
  void emplace_impl(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    static_assert(std::is_constructible_v<T, Args &&...>,
                  "T must be constructible with Args&&...");

    const auto current = head_.load(std::memory_order_relaxed);
    const auto next = (current + 1) & static_cast<Derived *>(this)->mask();

    while (next == cached_tail_) {
      cached_tail_ = tail_.load(std::memory_order_acquire);
    }

    new (static_cast<Derived *>(this)->element(current))
        T(std::forward<Args>(args)...);
    head_.store(next, std::memory_order_release);
  }

  template <typename... Args>
  bool try_emplace_impl(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    static_assert(std::is_constructible_v<T, Args &&...>,
                  "T must be constructible with Args&&...");

    const auto current = head_.load(std::memory_order_relaxed);
    const auto next = (current + 1) & static_cast<Derived *>(this)->mask();

    if (next == cached_tail_) {
      cached_tail_ = tail_.load(std::memory_order_acquire);
      if (next == cached_tail_) {
        return false;
      }
    }

    new (static_cast<Derived *>(this)->element(current))
        T(std::forward<Args>(args)...);
    head_.store(next, std::memory_order_release);
    return true;
  }

  // Atomic variables and cache padded fields
  alignas(nova::kCacheLineSize) std::atomic<SizeType> head_{0};
  alignas(nova::kCacheLineSize) SizeType cached_tail_{0};
  alignas(nova::kCacheLineSize) std::atomic<SizeType> tail_{0};
  alignas(nova::kCacheLineSize) SizeType cached_head_{0};
};

}  // namespace detail

/**
 * A fixed-size, compile-time capacity SPSC queue implementation.
 * This version requires T to be a trivial type and uses a fixed-size array.
 */
template <typename T, std::size_t Capacity>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
           std::is_trivially_copyable_v<T> &&
           std::is_default_constructible_v<T> &&
           std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
class alignas(nova::kCacheLineSize) StaticSPSCQueue
    : public detail::SPSCQueueBase<StaticSPSCQueue<T, Capacity>, T> {
 private:
  using Base = detail::SPSCQueueBase<StaticSPSCQueue<T, Capacity>, T>;
  friend Base;

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

  /**
   * Get the capacity of the queue.
   */
  static constexpr std::size_t capacity() noexcept {
    return Capacity;
  }

  /**
   * Emplace a new element in the queue.
   * Blocks (spins) if the queue is full.
   */
  template <typename... Args>
  void Emplace(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    this->emplace_impl(std::forward<Args>(args)...);
  }

  /**
   * Try to emplace a new element in the queue.
   * @return true if the element was emplaced, false if the queue was full
   */
  template <typename... Args>
  bool TryEmplace(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    return this->try_emplace_impl(std::forward<Args>(args)...);
  }

 private:
  // Return mask for index calculations
  static constexpr std::size_t mask() noexcept {
    return kMask;
  }

  // Get pointer to the element at the given index
  T *element(std::size_t index) noexcept {
    return &data_[index & kMask];
  }

  static constexpr std::size_t kMask = Capacity - 1;
  T data_[Capacity];
};

/**
 * A dynamic-size SPSC queue implementation using an allocator.
 */
template <typename T, typename Allocator = std::allocator<T>>
class alignas(nova::kCacheLineSize) SPSCQueue
    : private Allocator,
      public detail::SPSCQueueBase<SPSCQueue<T, Allocator>, T> {
 private:
  using Base = detail::SPSCQueueBase<SPSCQueue<T, Allocator>, T>;
  using AllocatorTraits = std::allocator_traits<Allocator>;
  friend Base;

 public:
  /**
   * Construct a queue with the given capacity.
   * Capacity will be rounded up to the next power of 2.
   */
  explicit SPSCQueue(std::size_t n, Allocator allocator = Allocator{})
      : Allocator{allocator},
        mask_{std::bit_ceil(n) - 1},
        data_{AllocatorTraits::allocate(*this, capacity())} {}

  ~SPSCQueue() {
    while (this->Front()) {
      this->Pop();
    }
    AllocatorTraits::deallocate(*this, data_, capacity());
  }

  SPSCQueue(const SPSCQueue &) = delete;
  SPSCQueue(SPSCQueue &&) = delete;
  SPSCQueue &operator=(const SPSCQueue &) = delete;
  SPSCQueue &operator=(SPSCQueue &&) = delete;

  /**
   * Get the capacity of the queue.
   */
  std::size_t capacity() const noexcept {
    return mask_ + 1;
  }

  /**
   * Emplace a new element in the queue.
   * Blocks (spins) if the queue is full.
   */
  template <typename... Args>
  void Emplace(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    this->emplace_impl(std::forward<Args>(args)...);
  }

  /**
   * Try to emplace a new element in the queue.
   * @return true if the element was emplaced, false if the queue was full
   */
  template <typename... Args>
  bool TryEmplace(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>) {
    return this->try_emplace_impl(std::forward<Args>(args)...);
  }

 private:
  // Return mask for index calculations
  std::size_t mask() const noexcept {
    return mask_;
  }

  // Get pointer to the element at the given index
  T *element(std::size_t index) noexcept {
    return &data_[index & mask_];
  }

  std::size_t mask_{0};
  T *data_{nullptr};
};
}  // namespace exp
}  // namespace nova