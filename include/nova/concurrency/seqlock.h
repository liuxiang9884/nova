//
// Created by liuxiang on 2025/4/9.
//

#pragma once

#include <atomic>
#include <type_traits>

#include "nova/common/hardware.h"

namespace nova {
template <typename T>
class SeqLock {
 public:
  static_assert(std::is_nothrow_copy_assignable_v<T>,
                "T must satisfy is_nothrow_copy_assignable");
  static_assert(std::is_trivially_copy_assignable_v<T>,
                "T must satisfy is_trivially_copy_assignable");



 private:
};
}  // namespace nova