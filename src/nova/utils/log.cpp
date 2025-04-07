//
// Created by liuxiang on 2025/4/7.
//

#include "nova/utils/log.h"

namespace nova {

void LogConfig::FromToml(const toml::node_view<const toml::node>& log_node) {
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
  timestamp_pattern_ =
      log_node["timestamp_pattern"].value_or(kDefaultLogTimestampPattern);
}

} // namespace nova