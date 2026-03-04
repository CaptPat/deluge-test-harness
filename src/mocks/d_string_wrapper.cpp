// Wrapper: compile d_string.cpp with two 64-bit portability fixes.
//
// 1. ALPHA_OR_BETA_VERSION=0: skip ARM SDRAM pointer range checks that
//    cast pointers to uint32_t (truncates on x86-64).
//
// 2. intptr_t → int32_t: d_string.cpp uses intptr_t for the 4-byte
//    reference-count slot at (stringMemory - 4). On ARM32, intptr_t is
//    4 bytes; on x86-64 it's 8 bytes. Writing 8 bytes at offset -4
//    overwrites the first 4 bytes of string data with zeros. Redefining
//    intptr_t to int32_t for the function bodies restores correct behavior.

// Include ALL headers d_string.cpp needs (they use #pragma once so the
// second include from the .cpp will be a no-op).
#include "definitions_cxx.hpp"
#undef ALPHA_OR_BETA_VERSION
#define ALPHA_OR_BETA_VERSION 0

#include "util/d_string.h"
#include "memory/general_memory_allocator.h"
#include "util/cfunctions.h"
#include <bit>
#include <cstring>
#include <io/debug/log.h>

// Now redefine intptr_t so the function bodies in d_string.cpp use 32-bit
// reason counters, matching the firmware's ARM32 memory layout.
#define intptr_t int32_t

#include "util/d_string.cpp"

#undef intptr_t
