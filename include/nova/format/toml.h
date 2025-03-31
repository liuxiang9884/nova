//
// Created by liuxiang on 2025/3/31.
//

#ifndef FORMAT_TOML_H
#define FORMAT_TOML_H

#include <fmt/format.h>
#include <toml++/toml.h>

#include <iostream>
#include <string>

template <>
struct fmt::formatter<toml::date_time> {
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.end(); }

  template <typename FormatContext>
  auto format(const toml::date_time& dt, FormatContext& ctx) const {
    const auto& date = dt.date;
    const auto& time = dt.time;

    std::string result =
        fmt::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}", date.year,
                    static_cast<int>(date.month), static_cast<int>(date.day),
                    static_cast<int>(time.hour), static_cast<int>(time.minute),
                    static_cast<int>(time.second));

    if (time.nanosecond > 0) {
      result += fmt::format(".{:09d}", time.nanosecond);
    }

    return fmt::format_to(ctx.out(), "{}", result);
  }
};

#endif //FORMAT_TOML_H
