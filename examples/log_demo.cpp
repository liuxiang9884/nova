//
// Created by liuxiang on 2025/3/31.
//

#include <unistd.h>

#include <filesystem>

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <toml++/toml.h>

#include "nova/utils/enum.h"
#include "nova/utils/log.h"

namespace fs = std::filesystem;

int main([[maybe_unused]] int argc, [[maybe_unused]] const char** argv) {
  CLI::App app{"log_demo"};
  std::string config_file;
  // -c ../../data/examples/log_demo.toml
  app.add_option("-c,--config", config_file, "file name");
  CLI11_PARSE(app, argc, argv);

  const auto path = fs::canonical(config_file);
  fmt::println("path = {}", path.c_str());
  const auto toml = toml::parse_file(path.string());
  const auto log_node = toml["log"];

  nova::LogConfig log_config;
  log_config.FromToml(log_node);
  fmt::println("log_level = {}", static_cast<int32_t>(log_config.log_level()));
  fmt::println("log_file = {}", log_config.log_file());
  fmt::println("console_sink_name = {}", log_config.console_sink_name());
  fmt::println("backend_thread_name = {}", log_config.backend_thread_name());
  fmt::println("backend_cpu_affinity = {}", log_config.backend_cpu_affinity());

  nova::LogManager log_manager(log_config);
  auto logger = log_manager.logger();
  LOG_INFO(logger, "Hello World!");
  LOG_WARNING(logger, "Hello World!");
  std::cout << nova::MaxEnumValue<nova::LogLevel>() << std::endl;

  std::cout << static_cast<int32_t>(
                   nova::LogLevelArray[nova::LogLevel::kLogCritical])
            << std::endl;

  std::cout << log_config.log_file() << std::endl;
  std::cout << log_config.backend_thread_name() << std::endl;

#include <CLI/CLI.hpp>
  return 0;
}
