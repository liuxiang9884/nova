//
// Created by liuxiang on 2025/4/9.
//

#pragma once

#include <new>

namespace nova {
constexpr std::size_t kCacheLineSize =
    std::hardware_destructive_interference_size;
}  // namespace nova