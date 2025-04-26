//
// Created by liuxiang on 2025/4/9.
//

#pragma once

#include <new>

namespace nova {
#if defined(__cpp_lib_hardware_interference_size)
constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
constexpr size_t kCacheLineSize = 64;
#endif
}  // namespace nova