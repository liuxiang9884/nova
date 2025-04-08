//
// Created by liuxiang on 2025/3/31.
//

#include <unistd.h>

#include <filesystem>
#include <thread>
#include <vector>

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
  fmt::println("file_sink_name = {}", log_config.file_sink_name());
  fmt::println("console_sink_name = {}", log_config.console_sink_name());
  fmt::println("json_file_sink_name = {}", log_config.json_file_sink_name());
  fmt::println("json_console_sink_name = {}",
               log_config.json_console_sink_name());
  fmt::println("backend_thread_name = {}", log_config.backend_thread_name());
  fmt::println("format_pattern = {}", log_config.format_pattern());
  fmt::println("backend_cpu_affinity = {}", log_config.backend_cpu_affinity());
  fmt::println("timestamp_pattern = {}", log_config.timestamp_pattern());

  nova::InitializeLogging(log_config);
  for (auto i = 0; i < 10; i++) {
    NOVA_INFO("Hello World!");
    NOVA_INFO_TAGS(TAG_PERFORMANCE, "let's go!");
  }

  const auto thread_count = std::thread::hardware_concurrency() / 2;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (auto i = 0u; i < thread_count; i++) {
    threads.emplace_back([&]() {
      nova::PreallocateLogging();
      for (auto j = 0; j < 10; j++) {
        NOVA_INFO("Hello World!");
        NOVA_INFO_TAGS(TAG_PERFORMANCE, "let's go!");
      }
    });
  }

  std::ranges::for_each(threads, [](std::thread& t) { t.join(); });

  return 0;
}
