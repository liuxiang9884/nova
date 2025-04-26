//
// Created by liuxiang on 2025/4/7.
//

#include "nova/utils/log.h"

#include <filesystem>
#include <iostream>
#include <limits>

#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>
#include <quill/sinks/JsonSink.h>

#include "nova/utils/enum.h"

namespace fs = std::filesystem;

namespace nova {

LogManager kLogManager = LogManager::Instance();

void InitializeLogging(const LogConfig& config) {
  kLogManager.Initialize(config);
}

void PreallocateLogging() {
  nova::LogManager::Preallocate();
}

#ifdef NDEBUG
constexpr LogLevel kDefaultLogLevel = LogLevel::kLogInfo;
constexpr std::string_view kDefaultLogLevelString = "info";
#else
constexpr LogLevel kDefaultLogLevel = LogLevel::kLogTrace;
constexpr std::string_view kDefaultLogLevelString = "trace";
#endif
constexpr std::string_view kDefaultLogConsoleSinkName = "nova_console";
constexpr std::string_view kDefaultLogFileSinkName = "/tmp/nova.log";
constexpr std::string_view kDefaultLogJsonConsoleSinkName;
constexpr std::string_view kDefaultLogJsonFileSinkName;
constexpr std::string_view kDefaultLogBackendThreadName;
constexpr uint16_t kDefaultLogBackendCpuAffinity =
    std::numeric_limits<uint16_t>::max();

constexpr std::string_view kDefaultLogFormatPattern =
    "%(log_level_short_code)%(time) %(process_id):%(thread_id) "
    "%(file_name):%(caller_function):%(line_number)] %(message) %(tags)";
constexpr std::string_view kDefaultLogTimestampPattern = "%Y%m%d %H:%M:%S.%Qns";

const EnumArray<LogLevel, quill::LogLevel> LogLevelArray{
    quill::LogLevel::TraceL1, quill::LogLevel::Debug,
    quill::LogLevel::Info,    quill::LogLevel::Warning,
    quill::LogLevel::Error,   quill::LogLevel::Critical};

template <typename SinkType>
class NovaJsonSinkBase : public quill::detail::JsonSink<SinkType> {
 public:
  using quill::detail::JsonSink<SinkType>::JsonSink;

  void generate_json_message_impl(
      quill::MacroMetadata const* log_metadata, uint64_t log_timestamp,
      std::string_view thread_id, std::string_view thread_name,
      std::string const& process_id,
      [[maybe_unused]] std::string_view logger_name,
      [[maybe_unused]] quill::LogLevel log_level,
      [[maybe_unused]] std::string_view log_level_description,
      [[maybe_unused]] std::string_view log_level_short_code,
      std::vector<std::pair<std::string, std::string>> const* named_args,
      [[maybe_unused]] std::string_view log_message,
      [[maybe_unused]] std::string_view log_statement,
      char const* message_format) {
    this->_json_message.append(fmtquill::format(
        R"({{"timestamp":"{}","file_name":"{}","function":"{}","line":"{}","process_id":"{}","thread_id":"{}","thread_name":"{}","log_level":"{}","message":"{}")",
        std::to_string(log_timestamp), log_metadata->file_name(),
        log_metadata->caller_function(), log_metadata->line(), process_id,
        thread_id, thread_name, log_level_description, message_format));

    if (named_args) {
      for (auto const& [key, value] : *named_args) {
        this->_json_message.append(
            fmtquill::format(R"(,"{}":"{}")", key, value));
      }
    }
  }
};

class NovaJsonFileSink final : public NovaJsonSinkBase<quill::FileSink> {
 public:
  NovaJsonFileSink(fs::path const& filename,
                   quill::FileSinkConfig const& config,
                   quill::FileEventNotifier notifier = {}, bool do_fopen = true)
      : NovaJsonSinkBase<quill::FileSink>(filename, config,
                                          std ::move(notifier), do_fopen) {}

  void generate_json_message(
      quill::MacroMetadata const* log_metadata, uint64_t log_timestamp,
      std::string_view thread_id, std::string_view thread_name,
      std::string const& process_id,
      [[maybe_unused]] std::string_view logger_name,
      [[maybe_unused]] quill::LogLevel log_level,
      [[maybe_unused]] std::string_view log_level_description,
      [[maybe_unused]] std::string_view log_level_short_code,
      std::vector<std::pair<std::string, std::string>> const* named_args,
      [[maybe_unused]] std::string_view log_message,
      [[maybe_unused]] std::string_view log_statement,
      char const* message_format) override {
    generate_json_message_impl(
        log_metadata, log_timestamp, thread_id, thread_name, process_id,
        logger_name, log_level, log_level_description, log_level_short_code,
        named_args, log_message, log_statement, message_format);
  }
};

class NovaJsonConsoleSink : public NovaJsonSinkBase<quill::StreamSink> {
 public:
  NovaJsonConsoleSink()
      : NovaJsonSinkBase<quill ::StreamSink>("stdout", nullptr) {}

  void generate_json_message(
      quill::MacroMetadata const* log_metadata, uint64_t log_timestamp,
      std::string_view thread_id, std::string_view thread_name,
      std::string const& process_id,
      [[maybe_unused]] std::string_view logger_name,
      [[maybe_unused]] quill::LogLevel log_level,
      [[maybe_unused]] std::string_view log_level_description,
      [[maybe_unused]] std::string_view log_level_short_code,
      std::vector<std::pair<std::string, std::string>> const* named_args,
      [[maybe_unused]] std::string_view log_message,
      [[maybe_unused]] std::string_view log_statement,
      char const* message_format) override {
    generate_json_message_impl(
        log_metadata, log_timestamp, thread_id, thread_name, process_id,
        logger_name, log_level, log_level_description, log_level_short_code,
        named_args, log_message, log_statement, message_format);
  }
};

LogConfig::LogConfig()
    : log_level_{kDefaultLogLevel},
      console_sink_name_{kDefaultLogConsoleSinkName},
      file_sink_name_{kDefaultLogFileSinkName},
      json_console_sink_name_{kDefaultLogJsonConsoleSinkName},
      json_file_sink_name_{kDefaultLogJsonFileSinkName},
      backend_thread_name_{kDefaultLogBackendThreadName},
      backend_cpu_affinity_{kDefaultLogBackendCpuAffinity},
      format_pattern_{kDefaultLogFormatPattern},
      timestamp_pattern_{kDefaultLogTimestampPattern} {}

void LogConfig::FromToml(const toml::node_view<const toml::node>& log_node) {
  const auto log_level = log_node["log_level"].value_or(kDefaultLogLevelString);
  log_level_ = LogLevelMap[log_level];
  console_sink_name_ =
      log_node["console_sink_name"].value_or(kDefaultLogConsoleSinkName);
  file_sink_name_ =
      log_node["file_sink_name"].value_or(kDefaultLogFileSinkName);
  json_console_sink_name_ = log_node["json_console_sink_name"].value_or(
      kDefaultLogJsonConsoleSinkName);
  json_file_sink_name_ =
      log_node["json_file_sink_name"].value_or(kDefaultLogJsonFileSinkName);
  backend_thread_name_ =
      log_node["backend_thread_name"].value_or(kDefaultLogBackendThreadName);
  backend_cpu_affinity_ =
      log_node["backend_cpu_affinity"].value_or(kDefaultLogBackendCpuAffinity);
  format_pattern_ =
      log_node["format_pattern"].value_or(kDefaultLogFormatPattern);
  timestamp_pattern_ =
      log_node["timestamp_pattern"].value_or(kDefaultLogTimestampPattern);
}

std::vector<std::shared_ptr<quill::Sink>> LogManager::CreateSinks() const {
  std::vector<std::shared_ptr<quill::Sink>> sinks;
  if (!config_.console_sink_name().empty()) {
    auto console_sink = NovaFrontend::create_or_get_sink<quill::ConsoleSink>(
        config_.console_sink_name());
    sinks.emplace_back(std::move(console_sink));
  }

  if (!config_.file_sink_name().empty()) {
    quill::FileSinkConfig file_sink_config;
    file_sink_config.set_open_mode('w');
    file_sink_config.set_filename_append_option(
        quill::FilenameAppendOption::StartDateTime);
    auto file_sink = NovaFrontend::create_or_get_sink<quill::FileSink>(
        config_.file_sink_name(), file_sink_config, quill::FileEventNotifier{});
    sinks.emplace_back(std::move(file_sink));
  }

  if (!config_.json_console_sink_name().empty()) {
    auto json_console_sink =
        NovaFrontend::create_or_get_sink<NovaJsonConsoleSink>(
            config_.json_console_sink_name());
    sinks.emplace_back(std::move(json_console_sink));
  }

  if (!config_.json_file_sink_name().empty()) {
    quill::FileSinkConfig json_file_sink_config;
    json_file_sink_config.set_open_mode('w');
    json_file_sink_config.set_filename_append_option(
        quill::FilenameAppendOption::StartDateTime);
    auto json_file_sink = NovaFrontend::create_or_get_sink<NovaJsonFileSink>(
        config_.json_file_sink_name(), json_file_sink_config,
        quill::FileEventNotifier{});
    sinks.emplace_back(std::move(json_file_sink));
  }

  if (sinks.empty()) {
    throw std::logic_error("Log must have at least one sink");
  }
  return sinks;
}

void LogManager::InitializeBackend() const {
  quill::BackendOptions backend_options;
  backend_options.thread_name = config_.backend_thread_name();
  backend_options.cpu_affinity = config_.backend_cpu_affinity();
  quill::Backend::start<NovaFrontendOptions>(backend_options,
                                             quill::SignalHandlerOptions{});
}

void LogManager::InitializeFrontend() {
  const auto sinks = CreateSinks();
  quill::PatternFormatterOptions format_options;
  format_options.format_pattern = config_.format_pattern();
  format_options.timestamp_pattern = config_.timestamp_pattern();
  format_options.timestamp_timezone = quill::Timezone::LocalTime;
  logger_ = NovaFrontend::create_or_get_logger("logger", sinks, format_options);
}

void LogManager::Initialize(const LogConfig& config) {
  config_ = config;
  InitializeBackend();
  InitializeFrontend();
}

}  // namespace nova