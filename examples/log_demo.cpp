//
// Created by liuxiang on 2025/3/31.
//

#include <unistd.h>

#include <fmt/format.h>
#include <CLI/CLI.hpp>

#include "nova/utils/log.h"

int main(int argc, const char** argv) {
  nova::LogConfig config;
  nova::LogManager log_manager(config);
  auto logger = log_manager.logger();
  LOG_INFO(logger, "Hello World!");
  return 0;
}
