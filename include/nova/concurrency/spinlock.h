//
// Created by liuxiang on 2025/4/9.
//

#pragma once

#include <atomic>

namespace nova {

class SpinLock {
 public:
  SpinLock() : flag_{ATOMIC_FLAG_INIT} {}

  void lock() {
    while (flag_.test_and_set(std::memory_order_acquire));
  }

  void unlock() {
    flag_.clear(std::memory_order_release);
  }

 private:
  std::atomic_flag flag_;
};

}  // namespace nova
