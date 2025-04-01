//
// Created by liuxiang on 2025/3/31.
//

#include <unistd.h>

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <nova/utils/enum.h>

#include "nova/utils/log.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] const char** argv) {
  nova::LogConfig config;
  nova::LogManager log_manager(config);
  auto logger = log_manager.logger();
  LOG_INFO(logger, "Hello World!");
  LOG_WARNING(logger, "Hello World!");
  std::cout << nova::MaxEnumValue<nova::LogLevel>() << std::endl;

  std::cout << static_cast<int32_t>(
                   nova::LogLevelArray[nova::LogLevel::kLogCritical])
            << std::endl;
  return 0;
}
