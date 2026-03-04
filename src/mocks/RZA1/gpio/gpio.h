// Shadow header replacing RZA1/gpio/gpio.h for x86 compilation.
// Provides no-op stubs for GPIO functions used by hid/encoder.cpp.

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

inline void setPinMux(uint8_t p, uint8_t q, uint8_t mux) { (void)p; (void)q; (void)mux; }
inline void setPinAsOutput(uint8_t p, uint8_t q) { (void)p; (void)q; }
inline void setPinAsInput(uint8_t p, uint8_t q) { (void)p; (void)q; }
inline uint16_t getOutputState(uint8_t p, uint8_t q) { (void)p; (void)q; return 0; }
inline void setOutputState(uint8_t p, uint8_t q, uint16_t state) { (void)p; (void)q; (void)state; }
inline uint16_t readInput(uint8_t p, uint8_t q) { (void)p; (void)q; return 1; }

#ifdef __cplusplus
}
#endif
