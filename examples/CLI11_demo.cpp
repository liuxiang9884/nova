//
// Created by liuxiang on 2025/3/31.
//

// cmd:
// ./CLI11_demo [OPTIONS]
// OPTIONS:
//  -h,     --help              Print this help message and exit
//  -f,     --file TEXT         file name
//  -d,     --date INT          date to be loaded

#include <fmt/format.h>

#include <CLI/CLI.hpp>

int main(int argc, char *argv[]) {
  CLI::App app{"CLI11 demo"};
  argv = app.ensure_utf8(argv);

  int32_t date = 2;
  std::string filename = "default";

  app.add_option("-f,--file", filename, "file name");
  app.add_option("-d, --date", date, "date to be loaded");

  CLI11_PARSE(app, argc, argv);

  fmt::println("filename = {}, date = {}", filename, date);
  return 0;
}