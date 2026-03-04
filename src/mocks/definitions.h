// Shadow header replacing firmware's definitions.h
// The real header includes ARM-specific headers (fault_handler.h, RZA1/cpu_specific.h, etc.)
// We provide only the defines and declarations needed by compiled firmware code.

#pragma once
#include <cstdint>

#ifndef SSI_TX_BUFFER_NUM_SAMPLES
#define SSI_TX_BUFFER_NUM_SAMPLES 128
#endif
#define SSI_RX_BUFFER_NUM_SAMPLES 2048
#define CACHE_LINE_SIZE 32
#define ALPHA_OR_BETA_VERSION 0

// ARM section attributes — empty on x86
#define PLACE_SDRAM_DATA
#define PLACE_SDRAM_BSS

#ifdef __cplusplus
extern "C" {
#endif
extern void freezeWithError(char const* errmsg);
#ifdef __cplusplus
}
#endif

#define FREEZE_WITH_ERROR(error) ({ freezeWithError(error); })
