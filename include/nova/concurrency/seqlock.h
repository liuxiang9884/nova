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
      seq0 = seq_.load(std::memory_order_acquire);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      copy = value_;
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq1 = seq_.load(std::memory_order_acquire);
    } while (seq0 != seq1 || seq0 & 1);
    return copy;
  }

  NOVA_DEBUG_NOINLINE void Store(const T& desired) noexcept {
    const uint64_t seq0 = seq_.load(std::memory_order_relaxed);
    seq_.store(seq0 + 1, std::memory_order_release);
    std::atomic_signal_fence(std::memory_order_acq_rel);
    value_ = desired;
    std::atomic_signal_fence(std::memory_order_acq_rel);
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
class alignas(kCacheLineSize) DoubleBufferSeqLock {
 public:
  static_assert(std::is_nothrow_copy_assignable_v<T>,
                "T must satisfy is_nothrow_copy_assignable");
  static_assert(std::is_trivially_copy_assignable_v<T>,
                "T must satisfy is_trivially_copy_assignable");

  DoubleBufferSeqLock() = default;

  NOVA_DEBUG_NOINLINE T Load() const noexcept {
    T copy;
    uint64_t seq0, seq1;

    do {
      auto active = active_.load(std::memory_order_acquire);
      seq0 = seq_.load(std::memory_order_acquire);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      copy = active ? buffer_[1] : buffer_[0];
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq1 = seq_.load(std::memory_order_acquire);
    } while (seq0 != seq1 || seq0 & 1);

    return copy;
  }

  NOVA_DEBUG_NOINLINE void Store(const T& desired) noexcept {
    bool active = active_.load(std::memory_order_relaxed);
    const uint64_t seq0 = seq_.load(std::memory_order_relaxed);
    seq_.store(seq0 + 1, std::memory_order_release);

    std::atomic_signal_fence(std::memory_order_acq_rel);
    (active ? buffer_[0] : buffer_[1]) = desired;
    std::atomic_signal_fence(std::memory_order_acq_rel);

    active_.store(!active, std::memory_order_release);
    seq_.store(seq0 + 2, std::memory_order_release);
  }

 private:
  alignas(kCacheLineSize) T buffer_[2];
  alignas(kCacheLineSize) std::atomic<bool> active_{false};
  alignas(kCacheLineSize) std::atomic<uint64_t> seq_{0};
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
    std::size_t seq0, seq1;
    do {
      seq0 = seq_.load(std::memory_order_acquire);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      copy = value_;
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq1 = seq_.load(std::memory_order_acquire);
    } while (seq0 != seq1 || seq0 & 1);
    return copy;
  }

  NOVA_DEBUG_NOINLINE void Store(const T& desired) noexcept {
    std::size_t seq0 = seq_.load(std::memory_order_relaxed);
    seq_.store(seq0 + 1, std::memory_order_release);
    std::atomic_signal_fence(std::memory_order_acq_rel);
    value_ = desired;
    std::atomic_signal_fence(std::memory_order_acq_rel);
    seq_.store(seq0 + 2, std::memory_order_release);
  }

  template <typename F, typename R = std::invoke_result_t<F, const T&>>
  NOVA_DEBUG_NOINLINE R Visit(F&& visitor) const noexcept {
    std::size_t seq0, seq1;
    R result;
    do {
      seq0 = seq_.load(std::memory_order_acquire);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      result = visitor(value_);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq1 = seq_.load(std::memory_order_acquire);
    } while (seq0 != seq1 || seq0 & 1);
    return result;
  }

  template <typename F, typename R = std::invoke_result_t<F, T&>>
  NOVA_DEBUG_NOINLINE R Update(F&& updater) noexcept {
    while (true) {
      std::size_t seq0 = seq_.load(std::memory_order_acquire);
      if (seq0 & 1) continue;

      if (!seq_.compare_exchange_weak(seq0, seq0 + 1,
                                      std::memory_order_acq_rel)) {
        continue;
      }

      std::atomic_signal_fence(std::memory_order_acq_rel);
      R result = updater(value_);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq_.store(seq0 + 2, std::memory_order_release);
      break;
      return result;
    }
  }

  const T& value() const noexcept {
    return value_;
  }

  const std::atomic<std::size_t>& seq() const noexcept {
    return seq_;
  }

 private:
  alignas(kCacheLineSize) T value_;
  alignas(kCacheLineSize) std::atomic<std::size_t> seq_ = 0;
};

template <typename T>
class alignas(kCacheLineSize) MRMWSeqLock {
 public:
  static_assert(std::is_nothrow_copy_assignable_v<T>,
                "T must satisfy is_nothrow_copy_assignable");
  static_assert(std::is_trivially_copy_assignable_v<T>,
                "T must satisfy is_trivially_copy_assignable");

  MRMWSeqLock() = default;

  NOVA_DEBUG_NOINLINE T Load() const noexcept {
    T copy;
    std::size_t seq0, seq1;
    do {
      seq0 = seq_.load(std::memory_order_acquire);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      copy = value_;
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq1 = seq_.load(std::memory_order_acquire);
    } while (seq0 != seq1 || seq0 & 1);
    return copy;
  }

  NOVA_DEBUG_NOINLINE void Store(const T& desired) noexcept {
    while (true) {
      std::size_t seq0 = seq_.load(std::memory_order_acquire);
      if (seq0 & 1) continue;

      if (!seq_.compare_exchange_weak(seq0, seq0 + 1,
                                      std::memory_order_acq_rel)) {
        continue;
      }

      std::atomic_signal_fence(std::memory_order_acq_rel);
      value_ = desired;
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq_.store(seq0 + 2, std::memory_order_release);
      break;
    }
  }

  template <typename F, typename R = std::invoke_result_t<F, const T&>>
  NOVA_DEBUG_NOINLINE R Visit(F&& visitor) const noexcept {
    std::size_t seq0, seq1;
    R result;
    do {
      seq0 = seq_.load(std::memory_order_acquire);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      result = visitor(value_);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq1 = seq_.load(std::memory_order_acquire);
    } while (seq0 != seq1 || seq0 & 1);
    return result;
  }

  template <typename F, typename R = std::invoke_result_t<F, T&>>
  NOVA_DEBUG_NOINLINE R Update(F&& updater) noexcept {
    while (true) {
      std::size_t seq0 = seq_.load(std::memory_order_acquire);
      if (seq0 & 1) continue;

      if (!seq_.compare_exchange_weak(seq0, seq0 + 1,
                                      std::memory_order_acq_rel)) {
        continue;
      }

      std::atomic_signal_fence(std::memory_order_acq_rel);
      R result = updater(value_);
      std::atomic_signal_fence(std::memory_order_acq_rel);
      seq_.store(seq0 + 2, std::memory_order_release);
      break;
      return result;
    }
  }

  const T& value() const noexcept {
    return value_;
  }
  const std::atomic<std::size_t>& seq() const noexcept {
    return seq_;
  }

 private:
  alignas(kCacheLineSize) T value_;
  alignas(kCacheLineSize) std::atomic<std::size_t> seq_ = 0;
};

}  // namespace nova