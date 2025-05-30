//
// Created by liuxiang on 2025/4/9.
//

#pragma once

// OS macros
// Operating System Detection
#define NOVA_OS_WINDOWS 1
#define NOVA_OS_MACOS 2
#define NOVA_OS_LINUX 3

#if defined(_WIN32) || defined(_WIN64)
#define NOVA_OS NOVA_OS_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
#define NOVA_OS NOVA_OS_MACOS
#elif defined(__linux__)
#define NOVA_OS NOVA_OS_LINUX
#else
#error "Unsupported operating system"
#endif

// Common debug mode definition for the entire project
#if !defined(NOVA_DEBUG_MODE)
#if !defined(NDEBUG)
#define NOVA_DEBUG_MODE 1
#else
#define NOVA_DEBUG_MODE 0
#endif
#endif

// Inline macros
#ifdef NDEBUG
#define NOVA_DEBUG_NOINLINE
#else
#define NOVA_DEBUG_NOINLINE __attribute__((noinline))
#endif

#define NOVA_FORCE_INLINE __attribute__((always_inline))
#define NOVA_FORCE_NOINLINE __attribute__((noinline))

// Platform-specific macros
// MAP_POPULATE may not be available on all platforms
#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

// Define property macros
// Pass by value
#define DEFINE_BASIC_PROPERTY(type, name, ...) \
 private:                                      \
  type name##_{__VA_ARGS__};                   \
                                               \
 public:                                       \
  void set_##name(type value) {                \
    name##_ = value;                           \
  }                                            \
  [[nodiscard]] type name() const {            \
    return name##_;                            \
  }

// Pass by reference
#define DEFINE_PROPERTY(type, name, ...)   \
 private:                                  \
  type name##_{__VA_ARGS__};               \
                                           \
 public:                                   \
  void set_##name(const type& value) {     \
    name##_ = value;                       \
  }                                        \
  void set_##name(type&& value) {          \
    name##_ = std::move(value);            \
  }                                        \
  [[nodiscard]] const type& name() const { \
    return name##_;                        \
  }
