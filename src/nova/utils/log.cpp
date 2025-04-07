//
// Created by liuxiang on 2025/4/7.
//

#include "nova/utils/log.h"

#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

#include "nova/utils/enum.h"

namespace nova {

#ifdef NDEBUG
constexpr LogLevel kDefaultLogLevel = LogLevel::kLogInfo;
constexpr std::string_view kDefaultLogLevelString = "info";
#else
const LogLevel kDefaultLogLevel = LogLevel::kLogTrace;
const std::string_view kDefaultLogLevelString = "trace";
#endif
constexpr std::string_view kDefaultLogConsoleSinkName = "nova_console";
constexpr std::string_view kDefaultLogFile = "/tmp/nova.log";
constexpr std::string_view kDefaultLogBackendThreadName = "nova_log";
constexpr uint16_t kDefaultLogBackendCpuAffinity =
    std::numeric_limits<uint16_t>::max();

constexpr std::string_view kDefaultLogFormatPattern =
    "%(log_level_short_code)%(time) %(process_id):%(thread_id) "
    "%(file_name):%(caller_function):%(line_number)] %(message)";
constexpr std::string_view kDefaultLogTimestampPattern = "%Y%m%d %H:%M:%S.%Qns";

const EnumArray<LogLevel, quill::LogLevel> LogLevelArray{
    quill::LogLevel::TraceL1, quill::LogLevel::Debug,
    quill::LogLevel::Info,    quill::LogLevel::Warning,
    quill::LogLevel::Error,   quill::LogLevel::Critical};

LogConfig::LogConfig()
    : log_level_{kDefaultLogLevel},
      console_sink_name_{kDefaultLogConsoleSinkName},
      log_file_{kDefaultLogFile},
      backend_thread_name_{kDefaultLogBackendThreadName},
      backend_cpu_affinity_{kDefaultLogBackendCpuAffinity},
      format_pattern_{kDefaultLogFormatPattern},
      timestamp_pattern_{kDefaultLogTimestampPattern} {}

void LogConfig::FromToml(const toml::node_view<const toml::node>& log_node) {
  const auto log_level = log_node["log_level"].value_or(kDefaultLogLevelString);
  log_level_ = LogLevelMap[log_level];
  console_sink_name_ =
      log_node["console_sink_name"].value_or(kDefaultLogConsoleSinkName);
  log_file_ = log_node["log_file"].value_or(kDefaultLogFile);
  backend_thread_name_ =
      log_node["backend_thread_name"].value_or(kDefaultLogBackendThreadName);
  backend_cpu_affinity_ =
      log_node["backend_cpu_affinity"].value_or(kDefaultLogBackendCpuAffinity);
  format_pattern_ =
      log_node["format_pattern"].value_or(kDefaultLogFormatPattern);
  timestamp_pattern_ =
      log_node["timestamp_pattern"].value_or(kDefaultLogTimestampPattern);
}

std::vector<std::shared_ptr<quill::Sink>> LogManager::CreateSinks() {
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

void LogManager::InitializeBackend() {
  quill::BackendOptions backend_options;
  backend_options.thread_name = config_.backend_thread_name();
  backend_options.cpu_affinity = config_.backend_cpu_affinity();
  quill::Backend::start(backend_options);
}

void LogManager::InitializeFrontend() {
  auto sinks = CreateSinks();
  quill::PatternFormatterOptions format_options;
  format_options.format_pattern = config_.format_pattern();
  format_options.timestamp_pattern = config_.timestamp_pattern();
  format_options.timestamp_timezone = quill::Timezone::LocalTime;
  logger_ = NovaFrontend::create_or_get_logger("logger", sinks, format_options);
}

void LogManager::Initialize() {
  InitializeBackend();
  InitializeFrontend();
}

}  // namespace nova