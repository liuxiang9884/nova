//
// Created by liuxiang on 2025/4/1.
//

#ifndef ENUM_H
#define ENUM_H

#include <ranges>

#include <magic_enum/magic_enum.hpp>

namespace nova {

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType Enum>
constexpr auto MaxEnumValue() {
  constexpr auto values = magic_enum::enum_values<Enum>();
  return static_cast<int32_t>(*std::ranges::max_element(values));
}

class EnumMap {
public:
private:
};

}

#endif //ENUM_H
