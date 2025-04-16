//
// Created by liuxiang on 2025/4/9.
//

#pragma once

#include <atomic>
#include <type_traits>

#include "nova/common/hardware.h"
#include "nova/common/macros.h"

namespace nova {

template <typename T>
class alignas(kCacheLineSize) SeqLock {
 public:
  static_assert(std::is_nothrow_copy_assignable_v<T>,
                "T must satisfy is_nothrow_copy_assignable");
  static_assert(std::is_trivially_copy_assignable_v<T>,
                "T must satisfy is_trivially_copy_assignable");

  SeqLock() = default;

  NOVA_DEBUG_NOINLINE T Load() const noexcept {
    T copy;
    uint64_t seq0, seq1;
    do {
      seq0 = seq_.load(std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_acquire);
      copy = value_;
      std::atomic_thread_fence(std::memory_order_release);
      seq1 = seq_.load(std::memory_order_relaxed);
    } while (seq0 != seq1 || seq0 & 1);
    return copy;
  }

  NOVA_DEBUG_NOINLINE void Store(const T& desired) noexcept {
    uint64_t seq0 = seq_.load(std::memory_order_relaxed);
    seq_.store(seq0 + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    value_ = desired;
    seq_.store(seq0 + 2, std::memory_order_release);
  }

  const T& value() const noexcept {
    return value_;
  }

  const std::atomic<uint64_t>& seq() const noexcept {
    return seq_;
  }

 private:
  alignas(kCacheLineSize) T value_;
  alignas(kCacheLineSize) std::atomic<uint64_t> seq_ = 0;
};

template <typename T>
class alignas(kCacheLineSize) MRSWSeqLock {
 public:
  static_assert(std::is_nothrow_copy_assignable_v<T>,
                "T must satisfy is_nothrow_copy_assignable");
  static_assert(std::is_trivially_copy_assignable_v<T>,
                "T must satisfy is_trivially_copy_assignable");

  MRSWSeqLock() = default;

  NOVA_DEBUG_NOINLINE T Load() const noexcept {
    T copy;
    uint64_t seq0, seq1;
    do {
      seq0 = seq_.load(std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_acquire);
      copy = value_;
      std::atomic_thread_fence(std::memory_order_release);
      seq1 = seq_.load(std::memory_order_relaxed);
    } while (seq0 != seq1 || seq0 & 1);
    return copy;
  }

  NOVA_DEBUG_NOINLINE void Store(const T& desired) noexcept {
    uint64_t seq0 = seq_.load(std::memory_order_relaxed);
    seq_.store(seq0 + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    value_ = desired;
    seq_.store(seq0 + 2, std::memory_order_release);
  }

  template <typename F, typename R = std::invoke_result_t<F, const T&>>
  NOVA_DEBUG_NOINLINE R Visit(F&& visitor) const noexcept {
    static_assert(noexcept(visitor(std::declval<const T&>())),
                  "Visitor function passed to Visit must be noexcept");

    uint64_t seq0, seq1;
    if constexpr (std::is_void_v<R>) {
      do {
        seq0 = seq_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        visitor(value_);
        std::atomic_thread_fence(std::memory_order_release);
        seq1 = seq_.load(std::memory_order_relaxed);
      } while (seq0 != seq1 || seq0 & 1);
    } else {
      R result;
      do {
        seq0 = seq_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        result = visitor(value_);
        std::atomic_thread_fence(std::memory_order_release);
        seq1 = seq_.load(std::memory_order_relaxed);
      } while (seq0 != seq1 || seq0 & 1);
      return result;
    }
  }

  template <typename F, typename R = std::invoke_result_t<F, T&>>
  NOVA_DEBUG_NOINLINE R Update(F&& updater) noexcept {
    static_assert(noexcept(updater(std::declval<T&>())),
                  "Updater function passed to Update must be noexcept");

    uint64_t seq0 = seq_.load(std::memory_order_relaxed);
    seq_.store(seq0 + 1, std::memory_order_relaxed);

    std::atomic_thread_fence(std::memory_order_release);
    if constexpr (std::is_void_v<R>) {
      updater(value_);
      seq_.store(seq0 + 2, std::memory_order_release);
    } else {
      R result = updater(value_);
      seq_.store(seq0 + 2, std::memory_order_release);
      return result;
    }
  }

  const T& value() const noexcept {
    return value_;
  }

  const std::atomic<uint64_t>& seq() const noexcept {
    return seq_;
  }

 private:
  alignas(kCacheLineSize) T value_;
  alignas(kCacheLineSize) std::atomic<uint64_t> seq_ = 0;
};

}  // namespace nova