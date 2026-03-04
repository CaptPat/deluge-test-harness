// Shadow header replacing firmware's drivers/mtu/mtu.h for x86 compilation.
// Provides stub declarations for hardware timer registers used in TEST_VECTOR code.

#pragma once

#include <cstdint>

#define TIMER_SYSTEM_FAST 0

// Stub: TCNT is an array of pointers to volatile timer registers on ARM.
// In tests, this is never dereferenced (guarded by #if TEST_VECTOR).
inline volatile uint32_t timer_stub_value = 0;
inline volatile uint32_t* TCNT[] = {&timer_stub_value};
