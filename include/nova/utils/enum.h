//
// Created by liuxiang on 2025/4/1.
//

#ifndef ENUM_H
#define ENUM_H

#include <array>

#include <magic_enum/magic_enum.hpp>

namespace nova {

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType Enum>
constexpr auto MaxEnumValue() -> int32_t {
  constexpr auto values = magic_enum::enum_values<Enum>();
  return static_cast<int32_t>(*std::ranges::max_element(values));
}

template <EnumType Enum, typename ValueType>
class EnumMap {
 public:
  static constexpr auto kMaxEnumValue = MaxEnumValue<Enum>();
  static constexpr auto kSize = kMaxEnumValue + 1;

  constexpr EnumMap() = default;

  constexpr EnumMap(std::initializer_list<ValueType> values) {
    static_assert(
        values.size() <= kSize,
        "initializer list size must be no larger than enum array size");
    std::copy(values.begin(), values.end(), values_.begin());
  }

  ValueType& operator[](Enum index) {
    return values_[static_cast<int32_t>(index)];
  }

  void SetValue(Enum key, ValueType value) {
    values_[static_cast<int32_t>(key)] = value;
  }

 private:
  std::array<ValueType, kMaxEnumValue> values_;
};

}  // namespace nova

#endif  // ENUM_H
