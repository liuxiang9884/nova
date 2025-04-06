//
// Created by liuxiang on 2025/3/31.
//

#ifndef LOG_H
#define LOG_H

#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>
#include <toml++/toml.h>

#include "nova/utils/enum.h"

namespace nova {

enum LogLevel : uint8_t {
  kLogTrace,
  kLogDebug,
  kLogInfo,
  kLogWarning,
  kLogError,
  kLogCritical
};

#ifdef NDEBUG
constexpr LogLevel kDefaultLogLevel = LogLevel::kLogInfo;
constexpr std::string_view kDefaultLogLevelString = "info";
#else
constexpr LogLevel kDefaultLogLevel = LogLevel::kLogTrace;
constexpr std::string_view kDefaultLogLevelString = "trace";
#endif
constexpr std::string_view kDefaultLogConsoleSinkName = "nova_console";
constexpr std::string_view kDefaultLogFile = "/tmp/nova.log";
constexpr std::string_view kDefaultLogBackendThreadName = "nova_log";
constexpr auto kDefaultLogBackendCpuAffinity =
    std::numeric_limits<uint16_t>::max();

constexpr uint32_t kDefaultLogInitialQueueSize = 1024 * 1024;
constexpr auto kDefaultLogQueueType = quill::QueueType::BoundedDropping;
#ifdef WIN32
constexpr auto enableHugePages = false;
#else
constexpr auto enableHugePages = true;
#endif
constexpr std::string_view kDefaultLogFormatPattern =
    "%(log_level_short_code)%(time) %(process_id):%(thread_id) "
    "%(file_name):%(caller_function):%(line_number)] %(message)";

const EnumArray<LogLevel, quill::LogLevel> LogLevelArray{
    quill::LogLevel::TraceL1, quill::LogLevel::Debug,
    quill::LogLevel::Info,    quill::LogLevel::Warning,
    quill::LogLevel::Error,   quill::LogLevel::Critical};

class LogConfig {
 public:
  std::unordered_map<std::string_view, LogLevel> LogLevelMap{
      {"trace", LogLevel::kLogTrace}, {"debug", LogLevel::kLogDebug},
      {"info", LogLevel::kLogInfo},   {"warning", LogLevel::kLogWarning},
      {"error", LogLevel::kLogError}, {"critical", LogLevel::kLogCritical}};

  LogConfig() = default;

  void set_log_file(std::string_view file) {
    log_file_ = file;
  }

  void set_console_sink_name(std::string_view name) {
    console_sink_name_ = name;
  }

  void set_log_level(std::string_view level) {
    if (const auto iter = LogLevelMap.find(level); iter != LogLevelMap.end()) {
      log_level_ = iter->second;
    }
  }

  void set_backend_thread_name(std::string_view value) {
    backend_thread_name_ = value;
  }

  void set_backend_cpu_affinity(uint16_t value) {
    backend_cpu_affinity_ = value;
  }

  void set_format_pattern(std::string_view format_pattern) {
    format_pattern_ = format_pattern;
  }

  void FromToml(const toml::node_view<const toml::node>& log_node) {
    const auto log_level =
        log_node["log_level"].value_or(kDefaultLogLevelString);
    log_level_ = LogLevelMap[log_level];
    console_sink_name_ =
        log_node["console_sink_name"].value_or(kDefaultLogConsoleSinkName);
    log_file_ = log_node["log_file"].value_or(kDefaultLogFile);
    backend_thread_name_ =
        log_node["backend_thread_name"].value_or(kDefaultLogBackendThreadName);
    backend_cpu_affinity_ = log_node["backend_cpu_affinity"].value_or(
        kDefaultLogBackendCpuAffinity);
    format_pattern_ =
        log_node["format_pattern"].value_or(kDefaultLogFormatPattern);
  }

  [[nodiscard]] LogLevel log_level() const noexcept {
    return log_level_;
  }

  [[nodiscard]] const std::string& log_file() const {
    return log_file_;
  }

  [[nodiscard]] const std::string& console_sink_name() const {
    return console_sink_name_;
  }

  [[nodiscard]] const std::string& backend_thread_name() const noexcept {
    return backend_thread_name_;
  }

  [[nodiscard]] uint16_t backend_cpu_affinity() const noexcept {
    return backend_cpu_affinity_;
  }

  [[nodiscard]] const std::string& format_pattern() const noexcept {
    return format_pattern_;
  }

 private:
  LogLevel log_level_{kDefaultLogLevel};
  std::string console_sink_name_{kDefaultLogConsoleSinkName};
  std::string log_file_{kDefaultLogFile};
  std::string backend_thread_name_{kDefaultLogBackendThreadName};
  std::string format_pattern_{kDefaultLogFormatPattern};
  uint16_t backend_cpu_affinity_{kDefaultLogBackendCpuAffinity};
};

class LogManager {
 public:
  explicit LogManager([[maybe_unused]] const LogConfig& config) {
    quill::Backend::start();
    auto console_sink =
        quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink");

    auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        "/tmp/test.log",
        []() {
          quill::FileSinkConfig cfg;
          cfg.set_open_mode('w');
          cfg.set_filename_append_option(
              quill::FilenameAppendOption::StartDateTime);
          return cfg;
        }(),
        quill::FileEventNotifier{});

    logger_ = quill::Frontend::create_or_get_logger(
        "logger", {console_sink, file_sink},
        quill::PatternFormatterOptions{std::string{kDefaultLogFormatPattern},
                                       "%Y-%m-%d %H:%M:%S.%Qns",
                                       quill::Timezone::LocalTime});
  }

  std::vector<std::shared_ptr<quill::Sink>> CreateSinks() {
    std::vector<std::shared_ptr<quill::Sink>> sinks;
    if (!config_.console_sink_name().empty()) {
      auto console_sink =
          quill::Frontend::create_or_get_sink<quill::ConsoleSink>(
              config_.console_sink_name());
      sinks.emplace_back(std::move(console_sink));
    }

    if (!config_.log_file().empty()) {
      auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
          config_.log_file(),
          []() {
            quill::FileSinkConfig cfg;
            cfg.set_open_mode('w');
            cfg.set_filename_append_option(
                quill::FilenameAppendOption::StartDateTime);
            return cfg;
          }(),
          quill::FileEventNotifier{});
      sinks.emplace_back(std::move(file_sink));
    }

    if (sinks.empty()) {
      throw std::logic_error("Must have at least one sink");
    }
    return sinks;
  }

  void Initialize() {
    auto sinks = CreateSinks();
    std::string format_pattern{
        "%(log_level_short_code)%(time) [%(thread_id)] "
        "%(file_name):%(full_path) %(message)"};
  }

  [[nodiscard]] quill::Logger* logger() const {
    return logger_;
  }

 private:
  LogConfig config_{};
  quill::Logger* logger_ = nullptr;
};

}  // namespace nova

#endif  // LOG_H
