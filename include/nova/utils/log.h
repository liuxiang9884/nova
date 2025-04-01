//
// Created by liuxiang on 2025/3/31.
//

#ifndef LOG_H
#define LOG_H

#include <string>

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

struct LogConfig {
  std::string log_file;
  LogLevel log_level;
  bool enable_stdout;
};

class LogManager {
 public:
  LogManager(const LogConfig& config) {
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
  LogConfig config_;
  quill::Logger* logger_ = nullptr;
};

}  // namespace nova

#endif  // LOG_H
