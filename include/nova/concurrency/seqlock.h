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

  NOVA_DEBUG_NOINLINE T load() const noexcept {
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

  NOVA_DEBUG_NOINLINE void store(const T &desired) noexcept {
    std::size_t seq0 = seq_.load(std::memory_order_relaxed);
    seq_.store(seq0 + 1, std::memory_order_release);
    std::atomic_signal_fence(std::memory_order_acq_rel);
    value_ = desired;
    std::atomic_signal_fence(std::memory_order_acq_rel);
    seq_.store(seq0 + 2, std::memory_order_release);
  }

 private:
  alignas(kCacheLineSize) T value_;
  alignas(kCacheLineSize) std::atomic<std::size_t> seq_ = 0;
};

}  // namespace nova