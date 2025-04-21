//
// Created by liuxiang on 2025/4/15.
//

#pragma once

#include <atomic>
#include <type_traits>
#include <utility>

#include "nova/common/hardware.h"

namespace nova {

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
class alignas(nova::kCacheLineSize) StaticSPBroadcastQueue {
 public:
  static_assert(Capacity >= 2, "Capacity must be at least 2");
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");

  /**
   * Default constructor initializes the queue with empty state
   */
  StaticSPBroadcastQueue() : current_(0) {}

  /**
   * Disallow copy and move operations
   */
  StaticSPBroadcastQueue(const StaticSPBroadcastQueue&) = delete;
  StaticSPBroadcastQueue(StaticSPBroadcastQueue&&) = delete;
  StaticSPBroadcastQueue& operator=(const StaticSPBroadcastQueue&) = delete;
  StaticSPBroadcastQueue& operator=(StaticSPBroadcastQueue&&) = delete;

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
    const auto current = current_.load(std::memory_order_acquire);
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

  // Padding to avoid false sharing between producer and consumer data
  char pad_[nova::kCacheLineSize - sizeof(std::atomic<uint64_t>)] = {0};

  // Data storage
  alignas(nova::kCacheLineSize) T data_[Capacity];
};

}  // namespace nova
