#pragma once
// Shadow: on x86 we use the standard library's memcpy/memset/etc.
// The firmware's mem_functions.h declares these without noexcept,
// which conflicts with Linux GCC's <string.h>.
