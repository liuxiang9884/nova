//
// Created by liuxiang on 2025/3/31.
//

#ifndef LOG_H
#define LOG_H

#include <string>
#include <string_view>
#include <unordered_map>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

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

  void set_log_file_name(std::string_view filename) {
    log_filename_ = filename;
  }

  void set_log_level(std::string_view level) {
    log_level_ = LogLevelMap[level];
  }

  void set_to_console(bool value) {
    to_console_ = value;
  }

  void set_to_file(bool value) {
    to_file_ = value;
  }

  [[nodiscard]] const std::string& log_file_name() const {
    return log_filename_;
  }

  [[nodiscard]] LogLevel log_level() const noexcept {
    return log_level_;
  }

  [[nodiscard]] bool to_file() const noexcept {
    return to_file_;
  }

  [[nodiscard]] bool to_console() const noexcept {
    return to_console_;
  }

 private:
  std::string log_filename_{"/tmp/test.log"};
  LogLevel log_level_{LogLevel::kLogTrace};
  bool to_console_{true};
  bool to_file_{true};
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
        quill::PatternFormatterOptions{
            "%(time) [%(thread_id)] [%(log_level)] %(message)",
            "%Y-%m-%d %H:%M:%S.%Qns", quill::Timezone::LocalTime});
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
