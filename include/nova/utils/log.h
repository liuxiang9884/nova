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

constexpr uint32_t kDefaultLogInitialQueueCapacity = 1024 * 1024;
constexpr auto kDefaultLogQueueType = quill::QueueType::BoundedDropping;
#ifdef WIN32
constexpr auto kLogEnableHugePages = false;
#else
constexpr auto kLogEnableHugePages = true;
#endif
constexpr std::string_view kDefaultLogFormatPattern =
    "%(log_level_short_code)%(time) %(process_id):%(thread_id) "
    "%(file_name):%(caller_function):%(line_number)] %(message)";
constexpr std::string_view kDefaultLogTimestampPattern = "%Y%m%d %H:%M:%S.%Qns";

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

  void set_timestamp_pattern(std::string_view value) {
    timestamp_pattern_ = value;
  }

  void FromToml(const toml::node_view<const toml::node>& log_node);

  [[nodiscard]] LogLevel log_level() const noexcept {
    return log_level_;
  }

  [[nodiscard]] const std::string& log_file() const noexcept {
    return log_file_;
  }

  [[nodiscard]] const std::string& console_sink_name() const noexcept {
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

  [[nodiscard]] const std::string& timestamp_pattern() const noexcept {
    return timestamp_pattern_;
  }

 private:
  LogLevel log_level_{kDefaultLogLevel};
  std::string console_sink_name_{kDefaultLogConsoleSinkName};
  std::string log_file_{kDefaultLogFile};
  std::string backend_thread_name_{kDefaultLogBackendThreadName};
  uint16_t backend_cpu_affinity_{kDefaultLogBackendCpuAffinity};
  std::string format_pattern_{kDefaultLogFormatPattern};
  std::string timestamp_pattern_{kDefaultLogTimestampPattern};
};

class LogManager {
 public:
  explicit LogManager([[maybe_unused]] const LogConfig& config)
      : config_(config) {
    Initialize();
  }

  struct NovaFrontendOptions {
    static constexpr quill::QueueType queue_type = kDefaultLogQueueType;
    static constexpr uint32_t initial_queue_capacity =
        kDefaultLogInitialQueueCapacity;
    static constexpr uint32_t blocking_queue_retry_interval_ns = 800;
    static constexpr bool huge_pages_enabled = kLogEnableHugePages;
  };

  using NovaFrontend = quill::FrontendImpl<NovaFrontendOptions>;
  using NovaLogger = quill::LoggerImpl<NovaFrontendOptions>;

  std::vector<std::shared_ptr<quill::Sink>> CreateSinks() {
    std::vector<std::shared_ptr<quill::Sink>> sinks;
    if (!config_.console_sink_name().empty()) {
      auto console_sink = NovaFrontend::create_or_get_sink<quill::ConsoleSink>(
          config_.console_sink_name());
      sinks.emplace_back(std::move(console_sink));
    }

    if (!config_.log_file().empty()) {
      quill::FileSinkConfig file_sink_config;
      file_sink_config.set_open_mode('w');
      file_sink_config.set_filename_append_option(
          quill::FilenameAppendOption::StartDateTime);
      auto file_sink = NovaFrontend::create_or_get_sink<quill::FileSink>(
          config_.log_file(), file_sink_config, quill::FileEventNotifier{});
      sinks.emplace_back(std::move(file_sink));
    }

    if (sinks.empty()) {
      throw std::logic_error("Must have at least one sink");
    }
    return sinks;
  }

  void InitializeBackend() {
    quill::BackendOptions backend_options;
    backend_options.thread_name = config_.backend_thread_name();
    backend_options.cpu_affinity = config_.backend_cpu_affinity();
    quill::Backend::start(backend_options);
  }

  void InitializeFrontend() {
    auto sinks = CreateSinks();
    quill::PatternFormatterOptions format_options;
    format_options.format_pattern = config_.format_pattern();
    format_options.timestamp_pattern = config_.timestamp_pattern();
    format_options.timestamp_timezone = quill::Timezone::LocalTime;
    logger_ =
        NovaFrontend::create_or_get_logger("logger", sinks, format_options);
  }

  void Initialize() {
    InitializeBackend();
    InitializeFrontend();
  }

  [[nodiscard]] NovaLogger* logger() const {
    return logger_;
  }

 private:
  LogConfig config_{};
  NovaLogger* logger_{nullptr};
};

}  // namespace nova

#endif  // LOG_H
