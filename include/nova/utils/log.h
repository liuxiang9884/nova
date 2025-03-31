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

namespace nova {

enum LogLevel : uint8_t {
  kLogTrace = quill::LogLevel::TraceL1,
  kLogDebug = quill::LogLevel::Debug,
  kLogInfo = quill::LogLevel::Info,
  kLogWarning = quill::LogLevel::Warning,
  kLogError = quill::LogLevel::Error,
  kLogCritical = quill::LogLevel::Critical
};

struct LogConfig {
  std::string log_file;
  LogLevel log_level;
  bool enable_stdout;
};

class LogManager {
 public:
 private:
};

}  // namespace nova

#endif  // LOG_H
