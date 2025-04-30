//
// Created by liuxiang on 2025/4/9.
//

#pragma once

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

#ifdef NDEBUG
#define NOVA_DEBUG_NOINLINE
#else
#define NOVA_DEBUG_NOINLINE __attribute__((noinline))
#endif

#define NOVA_FORCE_INLINE __attribute__((always_inline))
#define NOVA_FORCE_NOINLINE __attribute__((noinline))