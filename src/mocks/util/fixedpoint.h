// Shadow header for util/fixedpoint.h
// The firmware uses ARM VFP inline asm (vcvt instructions) that Clang rejects
// on x86. The asm is in dead constexpr branches — never executed on x86.
// We neutralize it by temporarily redefining asm() to a no-op.
#pragma once

#ifdef __clang__
#define asm(...)
#include "deluge/util/fixedpoint.h"
#undef asm
#else
#include "deluge/util/fixedpoint.h"
#endif
