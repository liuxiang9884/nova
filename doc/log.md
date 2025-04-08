# Nova 日志系统使用指南

Nova提供了异步日志记录、多种输出格式（包括普通文本和JSON）、灵活的配置选项等功能。

## 快速开始

只需包含头文件即可使用 Nova 日志系统：

```cpp
#include "nova/utils/logger.h"
```

### 初始化日志系统

在使用日志系统前，需要进行初始化：

```cpp
// 使用默认配置
nova::InitializeLogging();

// 或使用自定义配置
nova::LogConfig config;
config.set_log_level("debug");
config.set_file_sink_name("/path/to/your.log");
nova::InitializeLogging(config);
```

### 记录日志

Nova 日志系统提供了不同级别的日志宏：

```cpp
NOVA_TRACE("这是一条跟踪日志");
NOVA_DEBUG("这是一条调试日志");
NOVA_INFO("这是一条信息日志，包含参数: {}", 42);
NOVA_WARNING("这是一条警告日志，多个参数: {} {}", "字符串", 100);
NOVA_ERROR("这是一条错误日志");
NOVA_CRITICAL("这是一条严重错误日志");
```

## 配置选项

Nova 日志系统提供了多种配置选项：

### 日志级别

```cpp
config.set_log_level("trace");   // 所有日志
config.set_log_level("debug");   // 调试及以上
config.set_log_level("info");    // 信息及以上
config.set_log_level("warning"); // 警告及以上
config.set_log_level("error");   // 错误及以上
config.set_log_level("critical"); // 只有严重错误
```

### 日志输出

```cpp
// 控制台输出
config.set_console_sink_name("console");

// 文件输出
config.set_file_sink_name("/path/to/your.log");

// JSON 格式输出（控制台）
config.set_json_console_sink_name("json_console");

// JSON 格式输出（文件）
config.set_json_file_sink_name("/path/to/your.json.log");
```
sink_name设置为空时，对应sink不启动，默认json_sink均不启动

### 其他设置

```cpp
// 设置日志格式模式
config.set_format_pattern("%(time) [%(log_level)] %(message)");

// 设置时间戳格式
config.set_timestamp_pattern("%Y-%m-%d %H:%M:%S.%Qns");

// 设置后端线程名称
config.set_backend_thread_name("logger_thread");

// 设置后端线程 CPU 亲和性
config.set_backend_cpu_affinity(1);
```

## 高级用法

### 从 TOML 配置文件加载

```cpp
toml::table config_table = toml::parse_file("config.toml");
nova::LogConfig log_config;
log_config.FromToml(config_table["log"]);
nova::InitializeLogging(log_config);
```

### 预分配内存

在高性能场景下，可以预先分配日志系统所需的内存：

```cpp
nova::PreallocateLogging();
```

## 内部架构

Nova 日志系统使用了以下组件：

1. `LogConfig`：日志配置类，存储和管理日志系统的各种参数
2. `LogManager`：日志管理类，负责初始化和管理日志系统
3. 多种日志宏：为不同日志级别提供简便的接口
