//
// Created by liuxiang on 2025/5/28.
//

#pragma once

#include <type_traits>

namespace nova {

// Type trait to check if types are shared memory compatible
template <typename T>
constexpr bool is_shm_compatible_v =
    std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T> &&
    !std::is_pointer_v<T>;

} // namespace nova