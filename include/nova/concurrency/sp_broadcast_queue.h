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

namespace static_impl {

/**
 * A single producer, multiple consumer broadcast queue with static memory
 * allocation. This queue allows one producer to publish messages to multiple
 * consumers. Each consumer maintains its own read position and can read at its
 * own pace.
 *
 * @tparam T The type of elements stored in the queue
 * @tparam Capacity The fixed capacity of the queue (must be a power of 2)
 */
template <typename T, std::size_t Capacity>
  requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&
           std::is_trivially_copyable_v<T> &&
           std::is_default_constructible_v<T> &&
           std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>
class alignas(nova::kCacheLineSize) SPBroadcastQueue {
 public:
  static_assert(Capacity >= 2, "Capacity must be at least 2");
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");

  /**
   * Default constructor initializes the queue with empty state
   */
  SPBroadcastQueue() : current_(0) {}

  /**
   * Disallow copy and move operations
   */
  SPBroadcastQueue(const SPBroadcastQueue&) = default;
  SPBroadcastQueue(SPBroadcastQueue&&) = default;
  SPBroadcastQueue& operator=(const SPBroadcastQueue&) = default;
  SPBroadcastQueue& operator=(SPBroadcastQueue&&) = default;

  /**
   * Emplace a new element into the queue by constructing it in-place
   * @tparam Args Constructor argument types
   * @param args Constructor arguments
   */
  template <typename... Args>
  void Emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args&&...>) {
    static_assert(std::is_constructible_v<T, Args&&...>,
                  "T must be constructible with Args&&...");

    // Get current index and calculate next index
    const auto current = current_.load(std::memory_order_relaxed);
    const auto next = current + 1;

    // Construct the object at the current slot
    new (&data_[Idx(current)]) T(std::forward<Args>(args)...);

    // Update current_ to make the new element visible to consumers
    current_.store(next, std::memory_order_release);
  }

  /**
   * Push a copy of an element to the queue
   * @param value The value to push
   */
  void Push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    Emplace(value);
  }

  /**
   * Push a moved element to the queue
   * @param value The value to move
   */
  void Push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
    Emplace(std::move(value));
  }

  /**
   * Get the current producer position (number of elements ever produced)
   * @return The current producer position
   */
  [[nodiscard]] uint64_t Current() const noexcept {
    return current_.load(std::memory_order_acquire);
  }

  /**
   * Get a copy of the value at the specified position
   * @param pos The position to read from
   * @return A copy of the value
   */
  T Value(uint64_t pos) const noexcept {
    return data_[Idx(pos)];
  }

  /**
   * Get a reference to the value at the specified position
   * @param pos The position to read from
   * @return A reference to the value
   */
  const T& Ref(uint64_t pos) const noexcept {
    return data_[Idx(pos)];
  }

  /**
   * Retrieve a value and advance the given position
   * @param pos Position to read from (will be incremented after read)
   * @return The value at the position
   */
  T Pop(uint64_t& pos) noexcept {
    T value = data_[Idx(pos)];
    ++pos;
    return value;
  }

  /**
   * Try to retrieve a value if available
   * @param pos Position to read from (will be incremented if successful)
   * @param value Reference to store the retrieved value
   * @return true if a value was retrieved, false if no new value was available
   */
  bool TryPop(uint64_t& pos, T& value) noexcept {
    if (Current() <= pos) {
      return false;
    }

    value = data_[Idx(pos)];
    ++pos;
    return true;
  }

  /**
   * Get the number of elements available for reading from the given position
   * @param pos The consumer's current position
   * @return Number of elements available to read
   */
  [[nodiscard]] uint64_t Available(uint64_t pos) const noexcept {
    return Current() - pos;
  }

  /**
   * Check if there are elements available for reading
   * @param pos The consumer's current position
   * @return true if there are elements available, false otherwise
   */
  [[nodiscard]] bool HasAvailable(uint64_t pos) const noexcept {
    return Available(pos) > 0;
  }

  /**
   * Get the index in the circular buffer for a given sequence number
   * @param pos Sequence number
   * @return Index in the circular buffer
   */
  [[nodiscard]] constexpr std::size_t Idx(uint64_t pos) const noexcept {
    return pos & kMask;
  }

  /**
   * Get the number of times the circular buffer has wrapped for a given
   * position
   * @param pos Sequence number
   * @return Number of wraps
   */
  [[nodiscard]] constexpr std::size_t Turn(uint64_t pos) const noexcept {
    return pos / Capacity;
  }

  /**
   * Get the capacity of the queue
   * @return Queue capacity
   */
  [[nodiscard]] constexpr std::size_t capacity() const noexcept {
    return Capacity;
  }

 private:
  static constexpr std::size_t kMask = Capacity - 1;

  // Ensure proper cache line alignment
  alignas(nova::kCacheLineSize) std::atomic<uint64_t> current_;

  // Data storage
  alignas(nova::kCacheLineSize) T data_[Capacity];
};

}  // namespace static_impl

/**
 * A single producer, multiple consumer broadcast queue with dynamic memory
 * allocation. This queue allows one producer to publish messages to multiple
 * consumers. Each consumer maintains its own read position and can read at its
 * own pace.
 *
 * @tparam T The type of elements stored in the queue
 * @tparam Allocator The allocator type used for memory management
 */
template <typename T, typename Allocator = std::allocator<T>>
class alignas(nova::kCacheLineSize) SPBroadcastQueue {
 public:
  /**
   * Constructor with capacity and optional allocator
   * @param n Desired capacity (will be rounded up to next power of 2)
   * @param allocator Allocator instance
   */
  explicit SPBroadcastQueue(std::size_t n, Allocator allocator = Allocator{})
      : allocator_{allocator},
        mask_{std::bit_ceil(n) - 1},
        data_{std::allocator_traits<Allocator>::allocate(allocator_,
                                                         capacity())} {}

  /**
   * Destructor deallocates memory
   */
  ~SPBroadcastQueue() {
    std::allocator_traits<Allocator>::deallocate(allocator_, data_, capacity());
  }

  /**
   * Disallow copy and move operations
   */
  SPBroadcastQueue(const SPBroadcastQueue&) = delete;
  SPBroadcastQueue(SPBroadcastQueue&&) = delete;
  SPBroadcastQueue& operator=(const SPBroadcastQueue&) = delete;
  SPBroadcastQueue& operator=(SPBroadcastQueue&&) = delete;

  /**
   * Emplace a new element into the queue by constructing it in-place
   * @tparam Args Constructor argument types
   * @param args Constructor arguments
   */
  template <typename... Args>
  void Emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args&&...>) {
    static_assert(std::is_constructible_v<T, Args&&...>,
                  "T must be constructible with Args&&...");

    // Get current index and calculate next index
    const auto current = current_.load(std::memory_order_relaxed);
    const auto next = current + 1;

    // Construct the object at the current slot
    new (&data_[Idx(current)]) T(std::forward<Args>(args)...);

    // Update current_ to make the new element visible to consumers
    current_.store(next, std::memory_order_release);
  }

  /**
   * Push a copy of an element to the queue
   * @param value The value to push
   */
  void Push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    Emplace(value);
  }

  /**
   * Push a moved element to the queue
   * @param value The value to move
   */
  void Push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
    Emplace(std::move(value));
  }

  /**
   * Get the current producer position (number of elements ever produced)
   * @return The current producer position
   */
  [[nodiscard]] uint64_t Current() const noexcept {
    return current_.load(std::memory_order_acquire);
  }

  /**
   * Get a copy of the value at the specified position
   * @param pos The position to read from
   * @return A copy of the value
   */
  T Value(uint64_t pos) const noexcept {
    return data_[Idx(pos)];
  }

  /**
   * Get a reference to the value at the specified position
   * @param pos The position to read from
   * @return A reference to the value
   */
  const T& Ref(uint64_t pos) const noexcept {
    return data_[Idx(pos)];
  }

  /**
   * Retrieve a value and advance the given position
   * @param pos Position to read from (will be incremented after read)
   * @return The value at the position
   */
  T Pop(uint64_t& pos) noexcept {
    T value = data_[Idx(pos)];
    ++pos;
    return value;
  }

  /**
   * Try to retrieve a value if available
   * @param pos Position to read from (will be incremented if successful)
   * @param value Reference to store the retrieved value
   * @return true if a value was retrieved, false if no new value was available
   */
  bool TryPop(uint64_t& pos, T& value) noexcept {
    if (Current() <= pos) {
      return false;
    }

    value = data_[Idx(pos)];
    ++pos;
    return true;
  }

  /**
   * Get the number of elements available for reading from the given position
   * @param pos The consumer's current position
   * @return Number of elements available to read
   */
  [[nodiscard]] uint64_t Available(uint64_t pos) const noexcept {
    return Current() - pos;
  }

  /**
   * Check if there are elements available for reading
   * @param pos The consumer's current position
   * @return true if there are elements available, false otherwise
   */
  [[nodiscard]] bool HasAvailable(uint64_t pos) const noexcept {
    return Available(pos) > 0;
  }

  /**
   * Get the index in the circular buffer for a given sequence number
   * @param pos Sequence number
   * @return Index in the circular buffer
   */
  [[nodiscard]] std::size_t Idx(uint64_t pos) const noexcept {
    return pos & mask_;
  }

  /**
   * Get the number of times the circular buffer has wrapped for a given
   * position
   * @param pos Sequence number
   * @return Number of wraps
   */
  [[nodiscard]] std::size_t Turn(uint64_t pos) const noexcept {
    return pos / capacity();
  }

  /**
   * Get the capacity of the queue
   * @return Queue capacity
   */
  [[nodiscard]] std::size_t capacity() const noexcept {
    return mask_ + 1;
  }

  /**
   * Check if a consumer at the given position might lose data
   * @param pos Consumer position
   * @return true if consumer might lose data due to buffer wrap
   */
  [[nodiscard]] bool WillOverflow(uint64_t pos) const noexcept {
    return Current() - pos >= capacity() - 1;
  }

  /**
   * Check if a consumer at the given position has no data to read
   * @param pos Consumer position
   * @return true if empty for the given consumer
   */
  [[nodiscard]] bool Empty(uint64_t pos) const noexcept {
    return Available(pos) == 0;
  }

 private:
  Allocator allocator_ [[no_unique_address]];
  std::size_t mask_{0};
  T* data_{nullptr};
  alignas(nova::kCacheLineSize) std::atomic<uint64_t> current_{0};
};

}  // namespace nova
