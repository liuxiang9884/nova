//
// Created by liuxiang on 2025/4/9.
//

#pragma once

#ifdef NDEBUG
#define NOVA_DEBUG_NOINLINE
#else
#define NOVA_DEBUG_NOINLINE __attribute__((noinline))
#endif

#define NOVA_FORCE_INLINE __attribute__((always_inline))
#define NOVA_FORCE_NOINLINE __attribute__((noinline))