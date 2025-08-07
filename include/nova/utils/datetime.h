//
// Created by liuxiang on 2025/5/6.
//

#pragma once

#include <cmath>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <tuple>

#include <fmt/format.h>

// Check architecture to ensure emmintrin.h is only used on x86/x64
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
#define NOVA_HAS_X86_INTRINSICS
#include <emmintrin.h>
#endif

namespace nova {

static constexpr int64_t kMilliPerSecond = 1000;
static constexpr int64_t kMicroPerSecond = 1000000;
static constexpr int64_t kNanoPerSecond = 1000000000;

// format "2025-05-13 15:45:08.973"
static constexpr auto kMilliDatetimeFormatSize = 23;
// format "2025-05-13 15:45:08.973000"
static constexpr auto kMicroDatetimeFormatSize = 26;
// format "2025-05-13 15:45:08.973000000"
static constexpr auto kNanoDatetimeFormatSize = 29;

/**
 * @brief Introduces a CPU delay using pause instruction
 * @param delay Number of pause iterations
 */
inline void CpuDelay(std::size_t delay) {
#ifdef NOVA_HAS_X86_INTRINSICS
  for (std::size_t i = 0; i < delay; ++i) _mm_pause();
#else
  // Alternative implementation for non-x86 architectures
  // Use a non-incrementing approach to avoid volatile increment warning
  volatile std::size_t i = 0;
  const std::size_t target = delay * 50;
  while (i < target) {
    // Empty block to prevent compiler optimization
    i = i + 1;  // Assign instead of increment
  }
#endif
}

#ifdef NOVA_HAS_X86_INTRINSICS
/**
 * @brief Read the timestamp counter
 * @return Current timestamp counter value
 * @note Delay is approximately 23-25 cycles
 */
inline uint64_t rdtscp() {
  unsigned int aux;
  unsigned long long tsc;

#if defined(_MSC_VER)
  // MSVC implementation
  unsigned __int64 val = __rdtscp(&aux);
  return val;
#else
  // GCC/Clang implementation
  asm volatile("rdtscp" : "=A"(tsc), "=c"(aux)::"memory");
  return tsc;
#endif
}

/**
 * @brief Read the timestamp counter with chip and core information
 * @param chip Output parameter for chip ID
 * @param core Output parameter for core ID
 * @return Current timestamp counter value
 */
inline uint64_t rdtscp(uint64_t &chip, uint64_t &core) {
  unsigned int aux;
  unsigned long long tsc;

#if defined(_MSC_VER)
  // MSVC implementation
  unsigned __int64 val = __rdtscp(&aux);
  chip = (aux & 0xFFF000) >> 12;
  core = aux & 0xFFF;
  return val;
#else
  // GCC/Clang implementation
  asm volatile("rdtscp" : "=A"(tsc), "=c"(aux)::"memory");
  chip = (aux & 0xFFF000) >> 12;
  core = aux & 0xFFF;
  return tsc;
#endif
}

/**
 * @brief Execute CPUID instruction
 */
inline void CpuId() {
#if defined(_MSC_VER)
  // MSVC implementation
  int cpuInfo[4];
  __cpuid(cpuInfo, 0);
#else
  // GCC/Clang implementation
  unsigned int eax, ebx, ecx, edx;
  asm volatile("cpuid"
               : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
               : "0"(0)
               : "memory");
#endif
}
#endif  // NOVA_HAS_X86_INTRINSICS

/**
 * @brief Convert CPU cycles to nanoseconds
 * @param cycle CPU cycle count
 * @param ghz CPU frequency in GHz
 * @return Equivalent time in nanoseconds
 */
inline int64_t cycles2ns(int64_t cycle, double ghz) {
  return std::llround(static_cast<double>(cycle) / ghz);
}

/**
 * @brief Convert nanoseconds to CPU cycles
 * @param ns Time in nanoseconds
 * @param ghz CPU frequency in GHz
 * @return Equivalent CPU cycle count
 */
inline int64_t ns2cycles(int64_t ns, double ghz) {
  return static_cast<int64_t>(static_cast<double>(ns) * ghz);
}

/**
 * @brief Get current time in nanoseconds
 * @return Current time in nanoseconds
 * @note Function delay is approximately 22-25ns
 */
inline int64_t GetNanoseconds() {
  struct timespec now{};
  clock_gettime(CLOCK_REALTIME, &now);
  return now.tv_sec * kNanoPerSecond + now.tv_nsec;
}

/**
 * @brief Get current time in microseconds
 * @return Current time in microseconds
 */
inline int64_t GetMicroseconds() {
  return GetNanoseconds() / 1000;
}

/**
 * @brief Get current time in milliseconds
 * @return Current time in milliseconds
 */
inline int64_t GetMilliseconds() {
  return GetNanoseconds() / 1000000;
}

/**
 * @brief Get current time in seconds
 * @return Current time in seconds
 */
inline int64_t GetSeconds() {
  struct timespec now{};
  clock_gettime(CLOCK_REALTIME, &now);
  return now.tv_sec * kNanoPerSecond + now.tv_nsec;
  return now.tv_sec;
}

/**
 * @brief Convert seconds to formatted time string
 * @param str Output buffer for the formatted string
 * @param second Time in seconds
 * @param format Time format string
 * @note Ensure str buffer is large enough (at least 20 bytes)
 */
inline void SecondToStr(char *str, const time_t second,
                        const char *format = "%Y-%m-%d %H:%M:%S") {
  if (str == nullptr) return;

  struct tm tp{};
  localtime_r(&second, &tp);
  tp.tm_isdst = 0;
  strftime(str, 20, format, &tp);
}

/**
 * @brief Convert microseconds to formatted time string
 * @param str Output buffer for the formatted string
 * @param us Time in microseconds
 * @param format Time format string
 * @note Ensure str buffer is large enough
 */
inline void MicrosecondToStr(char *str, const int64_t us,
                             const char *format = "%Y-%m-%d %H:%M:%S") {
  if (str == nullptr) return;

  struct tm tp{};
  time_t second = us / 1000000;
  time_t decimal = us % 1000000;
  localtime_r(&second, &tp);
  tp.tm_isdst = 0;

  char tmp[32];
  strftime(tmp, sizeof(tmp), format, &tp);

  auto [out, size] = fmt::format_to_n(str, kMicroDatetimeFormatSize,
                                      "{}.{:06d}", tmp, decimal);
  str[size] = '\0';
}

/**
 * @brief Convert nanoseconds to formatted time string with milliseconds
 * @param str Output buffer for the formatted string
 * @param ns Time in nanoseconds
 * @param format Time format string
 * @note Ensure str buffer is large enough
 */
inline void NanosecondToMilliStr(char *str, const int64_t ns,
                                 const char *format = "%Y-%m-%d %H:%M:%S") {
  if (str == nullptr) return;

  struct tm tp{};
  time_t second = ns / 1000000000;
  time_t decimal = ns % 1000000000 / 1000000;
  localtime_r(&second, &tp);
  tp.tm_isdst = 0;

  char tmp[32];
  strftime(tmp, sizeof(tmp), format, &tp);

  auto [out, size] = fmt::format_to_n(str, kMilliDatetimeFormatSize,
                                      "{}.{:03d}", tmp, decimal);
  str[size] = '\0';
}

/**
 * @brief Convert nanoseconds to formatted time string with milliseconds
 * @param str Output buffer for the formatted string
 * @param ns Time in nanoseconds
 * @param format Time format string
 * @note Ensure str buffer is large enough
 */
inline void NanosecondToDatetime(char *str, const int64_t ns,
                                 const char *format = "%Y-%m-%d %H:%M:%S") {
  if (str == nullptr) return;

  struct tm tp{};
  time_t second = ns / 1000000000;
  time_t decimal = ns % 1000000000;

  char tmp[32];
  localtime_r(&second, &tp);
  tp.tm_isdst = 0;
  strftime(tmp, sizeof(tmp), format, &tp);

  auto [out, size] =
      fmt::format_to_n(str, kNanoDatetimeFormatSize, "{}.{:09d}", tmp, decimal);
  str[size] = '\0';
}

/**
 * @brief Create a timestamp for a specific time point
 * @param year Year
 * @param mon Month (1-12)
 * @param day Day (1-31)
 * @param hour Hour (0-23)
 * @param min Minute (0-59)
 * @param sec Second (0-59)
 * @return Time in seconds
 */
inline int64_t MakeTimePoint(int year, int mon, int day, int hour = 0,
                             int min = 0, int sec = 0) {
  struct tm t{};
  t.tm_sec = sec;
  t.tm_min = min;
  t.tm_hour = hour;
  t.tm_mday = day;
  t.tm_mon = mon - 1;
  t.tm_year = year - 1900;
  t.tm_isdst = 0;
  time_t timepoint = mktime(&t);

  return timepoint;
}

/**
 * @brief Convert time string to seconds
 * @param str Time string
 * @param fmt Format string
 * @return Time in seconds
 */
inline int64_t StringToSecond(const char *str,
                              const char *fmt = "%Y%m%d %H:%M:%S") {
  if (str == nullptr) return 0;

  struct tm t{};
  if (strptime(str, fmt, &t) == nullptr) {
    return 0;  // Parsing failed
  }
  t.tm_isdst = 0;
  time_t second = mktime(&t);

  return second;
}

/**
 * @brief Convert time string to nanoseconds
 * @param str Time string
 * @param fmt Format string
 * @return Time in nanoseconds
 */
inline int64_t StringToNanosecond(const char *str,
                                  const char *fmt = "%Y%m%d %H:%M:%S") {
  return StringToSecond(str, fmt) * 1000000000;
}

/**
 * @brief Convert time string to seconds
 * @param str Time string
 * @param fmt Format string
 * @return Time in seconds
 */
inline int64_t StringToSecond(const std::string &str,
                              const char *fmt = "%Y%m%d %H:%M:%S") {
  struct tm t{};
  if (strptime(str.c_str(), fmt, &t) == nullptr) {
    return 0;  // Parsing failed
  }
  t.tm_isdst = 0;
  time_t second = mktime(&t);
  return second;
}

/**
 * @brief Convert time string to nanoseconds
 * @param str Time string
 * @param fmt Format string
 * @return Time in nanoseconds
 */
inline int64_t StringToNanosecond(const std::string &str,
                                  const char *fmt = "%Y%m%d %H:%M:%S") {
  return StringToSecond(str, fmt) * 1000000000;
}

/**
 * @brief Parse date in YYYYMMDD format
 * @param date Date in YYYYMMDD format
 * @return Tuple containing year, month, day
 */
inline std::tuple<int64_t, int64_t, int64_t> ParseDate(int64_t date) {
  return {date / 10000, (date % 10000) / 100, date % 100};
}

/**
 * @brief Parse time in HHMMSS format
 * @param time Time in HHMMSS format
 * @return Tuple containing hour, minute, second
 */
inline std::tuple<int64_t, int64_t, int64_t> ParseTime(int64_t time) {
  return {time / 10000, (time % 10000) / 100, time % 100};
}

/**
 * @brief Convert time in HHMMSS format to nanoseconds
 * @param time Time in HHMMSS format
 * @return Time in nanoseconds
 */
inline int64_t ToNanosecond(int64_t time) {
  auto [hour, minute, second] = ParseTime(time);
  return (hour * 3600 + minute * 60 + second) * kNanoPerSecond;
}

/**
 * @brief Get nanosecond timestamp for the start of a specific date
 * @param date Date in YYYYMMDD format
 * @return Nanosecond timestamp for the start of the date
 */
inline int64_t GetDateStartNanosecond(const int64_t date) {
  auto [year, month, day] = ParseDate(date);
  return MakeTimePoint(static_cast<int>(year), static_cast<int>(month),
                       static_cast<int>(day)) *
         kNanoPerSecond;
}

/**
 * @brief Helper union for parsing time strings
 * @note Depends on specific memory layout, may have portability issues
 */
union TimeFmt {
  int64_t val;
  // Use a named struct to avoid GNU anonymous struct extension warning
  struct TimeComponents {
    int8_t h1;
    int8_t h0;
    int8_t c1;
    int8_t m1;
    int8_t m0;
    int8_t c2;
    int8_t s1;
    int8_t s0;
  } components;
};

/**
 * @brief Parse time string in "HH:MM:SS" format
 * @param time Time string view
 * @return Tuple containing hour, minute, second
 * @note Assumes the time format strictly conforms to "HH:MM:SS"
 */
inline std::tuple<int64_t, int64_t, int64_t> ParseTime(
    const std::string_view &time) {
  // Safety check
  if (time.size() < 8) {
    return {0, 0, 0};  // String too short
  }

  auto ptr = reinterpret_cast<const TimeFmt *>(time.data());
  auto hour = (ptr->components.h1 - '0') * 10 + (ptr->components.h0 - '0');
  auto minute = (ptr->components.m1 - '0') * 10 + (ptr->components.m0 - '0');
  auto second = (ptr->components.s1 - '0') * 10 + (ptr->components.s0 - '0');
  return {hour, minute, second};
}

/**
 * @brief Get today's date in YYYYMMDD format
 * @return Today's date in YYYYMMDD format
 */
inline int32_t GetToday() {
  const auto sec = static_cast<time_t>(nova::GetSeconds());
  struct tm tp{};
  localtime_r(&sec, &tp);
  tp.tm_isdst = 0;
  return (tp.tm_year + 1900) * 10000 + (tp.tm_mon + 1) * 100 + tp.tm_mday;
}

}  // namespace nova
