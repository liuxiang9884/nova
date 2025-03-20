//
// Created by liuxiang on 2025/3/19.
//

#include <filesystem>
#include <iostream>
#include <string>

#include <toml++/toml.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::println("Usage: tomlplusplus_demo filename");
  }

  const auto filename = argv[1];
  std::println("input_file = {}", filename);
  const auto toml = toml::parse_file(filename);

  const std::string_view title = toml["title"].value_or("default");
  const int32_t version = toml["value"].value_or(0);
  const float pi = toml["pi"].value_or(3.14);
  const bool debug = toml["debug"].value_or(true);
  std::println("title={}, version={}, pi={}, debug={}",
    title, version, pi, debug);

  auto created_at = toml["created_at"].value<toml::date_time>().value();
  // std::println("created_at={}", created_at.date.year, created_at.date.month,);

  return 0;
}