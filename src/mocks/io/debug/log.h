// Shadow header replacing firmware's io/debug/log.h for x86 compilation.
// Provides D_PRINTLN / D_PRINT macros as no-ops.

#pragma once

#include <cstddef>

// Debug macros — no-op in test harness
#define D_PRINTLN(...)
#define D_PRINT(...)
#define D_PRINT_RAW(...)

extern "C" void logDebug(const char* fmt, ...);
