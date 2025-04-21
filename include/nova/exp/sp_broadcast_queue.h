//
// Created by liuxiang on 2025/4/15.
//

#pragma once

#include <atomic>
#include <bit>
#include <memory>
#include <type_traits>
#include <utility>

#include "nova/common/hardware.h"

namespace nova {

namespace exp {

namespace detail {

/**
 * Base class for SP Broadcast (Single Producer, Multiple Consumer) queue
 * implementations. This queue allows one producer to broadcast messages to
 * multiple consumers. Each consumer maintains its own reading position.
 *
 * @tparam Derived The derived class (CRTP pattern)
 * @tparam T Type of the elements stored in the queue
 */
template <typename Derived, typename T>
class SPBroadcastQueueBase {
 protected:
  using SizeType = std::size_t;
  using PositionType = uint64_t;

  SPBroadcastQueueBase() = default;
  ~SPBroadcastQueueBase() = default;

  SPBroadcastQueueBase(const SPBroadcastQueueBase&) = delete;
  SPBroadcastQueueBase(SPBroadcastQueueBase&&) = delete;
  SPBroadcastQueueBase& operator=(const SPBroadcastQueueBase&) = delete;
  SPBroadcastQueueBase& operator=(SPBroadcastQueueBase&&) = delete;

 public:
  /**
   * Get the current producer position (number of elements ever produced).
   */
  [[nodiscard]] PositionType Current() const noexcept {
    return current_.load(std::memory_order_acquire);
  }

  /**
   * Get the number of elements available for reading from the given position.
   */
  [[nodiscard]] PositionType Available(PositionType pos) const noexcept {
    return Current() - pos;
  }

  /**
   * Check if there are elements available for reading from the given position.
   */
  [[nodiscard]] bool HasAvailable(PositionType pos) const noexcept {
    return Available(pos) > 0;
  }

  /**
   * Check if the queue is empty for a consumer at the given position.
   */
  [[nodiscard]] bool Empty(PositionType pos) const noexcept {
    return Available(pos) == 0;
  }

  /**
   * Check if a consumer at given position might lose data due to buffer
   * overflow.
   */
  [[nodiscard]] bool WillOverflow(PositionType pos) const noexcept {
    return Current() - pos >= static_cast<const Derived*>(this)->capacity() - 1;
  }

  /**
   * Get the index in the circular buffer for a given sequence number.
   */
  [[nodiscard]] SizeType Idx(PositionType pos) const noexcept {
    return pos & static_cast<const Derived*>(this)->mask();
  }

  /**
   * Get the number of times the circular buffer has wrapped for a given
   * position.
   */
  [[nodiscard]] SizeType Turn(PositionType pos) const noexcept {
    return pos / static_cast<const Derived*>(this)->capacity();
  }

  /**
   * Get a copy of the value at the specified position.
   */
  [[nodiscard]] T Value(PositionType pos) const noexcept {
    return *static_cast<const Derived*>(this)->element(pos);
  }

  /**
   * Get a reference to the value at the specified position.
   */
  [[nodiscard]] const T& Ref(PositionType pos) const noexcept {
    return *static_cast<const Derived*>(this)->element(pos);
  }

  /**
   * Retrieve a value and advance the given position.
   * @param pos Consumer position (will be incremented after read)
   * @return The value at the position
   */
  T Pop(PositionType& pos) noexcept {
    T value = Value(pos);
    ++pos;
    return value;
  }

  /**
   * Try to retrieve a value if available.
   * @param pos Consumer position (will be incremented if successful)
   * @param value Reference to store the retrieved value
   * @return true if a value was retrieved, false if no new value was available
   */
  bool TryPop(PositionType& pos, T& value) noexcept {
    if (Empty(pos)) {
      return false;
    }

    value = Value(pos);
    ++pos;
    return true;
  }

  /**
   * Push a copy of an element to the queue.
   */
  void Push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    static_cast<Derived*>(this)->template Emplace<const T&>(value);
  }

  /**
   * Push a moved element to the queue.
   */
  void Push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
    static_cast<Derived*>(this)->template Emplace<T&&>(std::move(value));
  }

 protected:
  template <typename... Args>
  void EmplaceImpl(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args&&...>) {
    static_assert(std::is_constructible_v<T, Args&&...>,
                  "T must be constructible with Args&&...");

    // Get current index and calculate next index
    const auto current = current_.load(std::memory_order_relaxed);
    const auto next = current + 1;

    // Construct the object at the current slot
    new (static_cast<Derived*>(this)->element(current))
        T(std::forward<Args>(args)...);

    // Update current_ to make the new element visible to consumers
    current_.store(next, std::memory_order_release);
  }

  // Atomic position and cache line padding
  alignas(nova::kCacheLineSize) std::atomic<PositionType> current_{0};
};

}  // namespace detail

/**
 * A fixed-size, compile-time capacity single-producer broadcast queue
 * implementation. This version requires T to be a trivial type and uses a
 * fixed-size array.
 *
 * @tparam T Type of elements stored in the queue
 * @tparam Capacity Fixed capacity of the queue (must be a power of 2)
 */
template <typename T, std::size_t Capacity>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
           std::is_trivially_copyable_v<T> &&
           std::is_default_constructible_v<T> &&
           std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
class alignas(nova::kCacheLineSize) StaticSPBroadcastQueue
    : public detail::SPBroadcastQueueBase<StaticSPBroadcastQueue<T, Capacity>,
                                          T> {
 private:
  using Base =
      detail::SPBroadcastQueueBase<StaticSPBroadcastQueue<T, Capacity>, T>;
  friend Base;

 public:
  static_assert(Capacity >= 2, "Capacity must be at least 2");
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");

  StaticSPBroadcastQueue() = default;
  ~StaticSPBroadcastQueue() = default;

  StaticSPBroadcastQueue(const StaticSPBroadcastQueue&) = delete;
  StaticSPBroadcastQueue(StaticSPBroadcastQueue&&) = delete;
  StaticSPBroadcastQueue& operator=(const StaticSPBroadcastQueue&) = delete;
  StaticSPBroadcastQueue& operator=(StaticSPBroadcastQueue&&) = delete;

  /**
   * Get the capacity of the queue.
   */
  static constexpr std::size_t capacity() noexcept {
    return Capacity;
  }

  /**
   * Emplace a new element in the queue.
   */
  template <typename... Args>
  void Emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args&&...>) {
    this->EmplaceImpl(std::forward<Args>(args)...);
  }

 private:
  // Return mask for index calculations
  static constexpr std::size_t mask() noexcept {
    return kMask;
  }

  // Get pointer to the element at the given index
  const T* element(uint64_t pos) const noexcept {
    return &data_[this->Idx(pos)];
  }

  // Get pointer to the element at the given index (non-const)
  T* element(uint64_t pos) noexcept {
    return &data_[this->Idx(pos)];
  }

  static constexpr std::size_t kMask = Capacity - 1;
  alignas(nova::kCacheLineSize) T data_[Capacity];
};

/**
 * A dynamic-size single-producer broadcast queue implementation using an
 * allocator.
 *
 * @tparam T Type of elements stored in the queue
 * @tparam Allocator Allocator type for memory management
 */
template <typename T, typename Allocator = std::allocator<T>>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
               std::is_trivially_copyable_v<T> &&
               std::is_default_constructible_v<T> &&
               std::is_copy_constructible_v<T> &&
               std::is_move_constructible_v<T>
class alignas(nova::kCacheLineSize) SPBroadcastQueue
    : private Allocator,
      public detail::SPBroadcastQueueBase<SPBroadcastQueue<T, Allocator>, T> {
 private:
  using Base = detail::SPBroadcastQueueBase<SPBroadcastQueue<T, Allocator>, T>;
  using AllocatorTraits = std::allocator_traits<Allocator>;
  friend Base;

 public:
  /**
   * Construct a queue with the given capacity.
   * Capacity will be rounded up to the next power of 2.
   */
  explicit SPBroadcastQueue(std::size_t n, Allocator allocator = Allocator{})
      : Allocator{allocator},
        mask_{std::bit_ceil(n) - 1},
        data_{AllocatorTraits::allocate(*this, capacity())} {}

  ~SPBroadcastQueue() {
    // No need to destruct elements - they're trivial types
    AllocatorTraits::deallocate(*this, data_, capacity());
  }

  SPBroadcastQueue(const SPBroadcastQueue&) = delete;
  SPBroadcastQueue(SPBroadcastQueue&&) = delete;
  SPBroadcastQueue& operator=(const SPBroadcastQueue&) = delete;
  SPBroadcastQueue& operator=(SPBroadcastQueue&&) = delete;

  /**
   * Get the capacity of the queue.
   */
  [[nodiscard]] std::size_t capacity() const noexcept {
    return mask_ + 1;
  }

  /**
   * Emplace a new element in the queue.
   */
  template <typename... Args>
  void Emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args&&...>) {
    this->EmplaceImpl(std::forward<Args>(args)...);
  }

 private:
  // Return mask for index calculations
  [[nodiscard]] std::size_t mask() const noexcept {
    return mask_;
  }

  // Get pointer to the element at the given index
  const T* element(uint64_t pos) const noexcept {
    return &data_[this->Idx(pos)];
  }

  // Get pointer to the element at the given index (non-const)
  T* element(uint64_t pos) noexcept {
    return &data_[this->Idx(pos)];
  }

  std::size_t mask_{0};
  alignas(nova::kCacheLineSize) T* data_{nullptr};
};

}  // namespace exp
}  // namespace nova