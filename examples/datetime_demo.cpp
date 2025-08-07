#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

// Include fmt library headers
#include <fmt/core.h>
#include <fmt/format.h>

// Include core Nova header
#include "nova/utils/datetime.h"

// Function to format time display
std::string format_time(int64_t time_value, bool is_nano = false) {
  char buffer[64] = {0};
  if (is_nano) {
    // Manual implementation of NanosecondToMilliStr functionality
    struct tm tp{};
    time_t second = time_value / 1000000000;
    time_t decimal = time_value % 1000000000 / 1000000;
    localtime_r(&second, &tp);
    tp.tm_isdst = 0;

    char tmp[32];
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", &tp);

    // Format using fmt
    return fmt::format("{}.{:03d}", tmp, decimal);
  } else {
    time_t t = static_cast<time_t>(time_value);
    nova::SecondToStr(buffer, t);
    return std::string(buffer);
  }
}

// Implement a standalone function to format microseconds
std::string format_microsecond(int64_t us) {
  struct tm tp{};
  time_t second = us / 1000000;
  time_t decimal = us % 1000000;
  localtime_r(&second, &tp);
  tp.tm_isdst = 0;

  char tmp[32];
  strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", &tp);

  // Format using fmt
  return fmt::format("{}.{:06d}", tmp, decimal);
}

int main() {
  fmt::println("Nova Datetime Utilities Demo ");
  fmt::println("---------------------------  ");

  // Get current time in different precision
  int64_t current_ns = nova::GetNanoseconds();
  int64_t current_us = nova::GetMicroseconds();
  int64_t current_ms = nova::GetMilliseconds();
  int64_t current_s = nova::GetSeconds();

  fmt::println("Current time: ");
  fmt::println("  Nanoseconds: {} ", current_ns);
  fmt::println("  Microseconds: {} ", current_us);
  fmt::println("  Milliseconds: {} ", current_ms);
  fmt::println("  Seconds: {} ", current_s);
  fmt::println("  Formatted: {} ", format_time(current_s));
  fmt::println("  Formatted (ns): {} ",
                           format_time(current_ns, true));
  fmt::println("  Formatted (us): {}  ",
                           format_microsecond(current_us));

  // Measure function execution time
  auto start = nova::GetNanoseconds();
  // Sleep for a short period to simulate an operation
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto end = nova::GetNanoseconds();

  fmt::println("Time measurement: ");
  fmt::println(
      "  Operation took {:.6f} seconds ({} ns)  ",
      static_cast<double>(end - start) / nova::kNanoPerSecond, end - start);

  // Time point creation and conversion
  int64_t specific_time = nova::MakeTimePoint(2025, 5, 6, 12, 30, 45);
  fmt::println("Time point manipulation: ");
  fmt::println("  Created time point: {} seconds ", specific_time);
  fmt::println("  Formatted: {}  ", format_time(specific_time));

  // String parsing
  std::string time_str = "20250506 12:30:45";
  int64_t parsed_time = nova::StringToSecond(time_str);
  fmt::println("String parsing: ");
  fmt::println("  Parsed string '{}' to: {} seconds ", time_str,
                           parsed_time);
  fmt::println("  Formatted back: {}  ",
                           format_time(parsed_time));

  // Parse date and time
  int64_t date_int = 20250506;  // YYYYMMDD
  int64_t time_int = 123045;    // HHMMSS

  auto [year, month, day] = nova::ParseDate(date_int);
  auto [hour, minute, second] = nova::ParseTime(time_int);

  fmt::println("Date and time parsing: ");
  fmt::println("  Date {} parsed as: year={}, month={}, day={} ",
                           date_int, year, month, day);
  fmt::println(
      "  Time {} parsed as: hour={}, minute={}, second={} ", time_int, hour,
      minute, second);

  // Get today's date
  int32_t today = nova::GetToday();
  fmt::println("  Today's date (YYYYMMDD): {}  ", today);

  // CPU-related functions demonstration
  fmt::println("CPU-related functions: ");

  // Demonstrate CPU delay
  fmt::println("  Demonstrating CpuDelay... ");
  start = nova::GetNanoseconds();
  nova::CpuDelay(1000);  // Execute 1000 pause instructions
  end = nova::GetNanoseconds();
  fmt::println("  CpuDelay(1000) took {} ns ", end - start);

#ifdef NOVA_HAS_X86_INTRINSICS
  // Execute only on x86 architecture
  fmt::println(
      "  X86-specific functions available on this platform ");

  // Read timestamp counter
  uint64_t tsc = nova::rdtscp();
  fmt::println("  Current TSC: {} ", tsc);

  // Read timestamp counter with chip and core info
  uint64_t chip, core;
  tsc = nova::rdtscp(chip, core);
  fmt::println("  Current TSC with chip/core info: {} ", tsc);
  fmt::println("  Running on chip: {}, core: {} ", chip, core);

  // Show cycles and nanoseconds conversion
  double cpu_ghz = 3.0;           // Assume 3GHz CPU
  int64_t cycles = 3000000000;    // 1 billion cycles
  int64_t ns_value = 1000000000;  // 1 billion nanoseconds (1 second)

  fmt::println(
      "  Cycles-nanoseconds conversion (assuming {}GHz CPU): ", cpu_ghz);
  fmt::println("    {} cycles = {} ns ", cycles,
                           nova::cycles2ns(cycles, cpu_ghz));
  fmt::println("    {} ns = {} cycles ", ns_value,
                           nova::ns2cycles(ns_value, cpu_ghz));
#else
  fmt::println(
      "  X86-specific functions not available on this platform ");
#endif

  return 0;
}