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
#include <toml++/toml.h>

namespace nova {

enum LogLevel : uint8_t {
  kLogTrace,
  kLogDebug,
  kLogInfo,
  kLogWarning,
  kLogError,
  kLogCritical
};

constexpr quill::QueueType kDefaultLogQueueType =
    quill::QueueType::BoundedDropping;
constexpr uint32_t kDefaultLogInitialQueueCapacity = 1024 * 1024;
constexpr uint32_t kDefaultLogBlockingQueueRetryIntervalNs = 800;
#ifdef WIN32
constexpr bool kLogEnableHugePages = false;
#else
constexpr bool kLogEnableHugePages = true;
#endif

class LogConfig {
 public:
  std::unordered_map<std::string_view, LogLevel> LogLevelMap{
      {"trace", LogLevel::kLogTrace}, {"debug", LogLevel::kLogDebug},
      {"info", LogLevel::kLogInfo},   {"warning", LogLevel::kLogWarning},
      {"error", LogLevel::kLogError}, {"critical", LogLevel::kLogCritical}};

  LogConfig();

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
  LogLevel log_level_;
  std::string console_sink_name_;
  std::string log_file_;
  std::string backend_thread_name_;
  uint16_t backend_cpu_affinity_;
  std::string format_pattern_;
  std::string timestamp_pattern_;
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
    static constexpr uint32_t blocking_queue_retry_interval_ns =
        kDefaultLogBlockingQueueRetryIntervalNs;
    static constexpr bool huge_pages_enabled = kLogEnableHugePages;
  };

  using NovaFrontend = quill::FrontendImpl<NovaFrontendOptions>;
  using NovaLogger = quill::LoggerImpl<NovaFrontendOptions>;

  [[nodiscard]] NovaLogger* logger() const {
    return logger_;
  }

 private:
  std::vector<std::shared_ptr<quill::Sink>> CreateSinks();

  void InitializeBackend();

  void InitializeFrontend();

  void Initialize();

 private:
  LogConfig config_{};
  NovaLogger* logger_{nullptr};
};

}  // namespace nova

#endif  // LOG_H
