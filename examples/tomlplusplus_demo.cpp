//
// Created by liuxiang on 2025/3/19.
//

#include <string>
#include <filesystem>

#include <toml++/toml.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::print("Usage: tomlplusplus_demo filename\n");
  }

  const auto filename = argv[1];
  const auto toml = toml::parse(filename);

  return 0;
}