// Shadow header for io/debug/print.h
// Replaces the firmware's version which contains ARM inline assembly.
#pragma once

#include <cstdint>

class MIDICable;

namespace Debug {
const uint32_t sec = 400000000;
const uint32_t mS = 400000;
const uint32_t uS = 400;

inline uint32_t readCycleCounter() { return 0; }
inline void readCycleCounter(uint32_t& time) { time = 0; }

void init();
void print(char const* output);
void println(char const* output);
void println(int32_t number);
void printlnfloat(float number);
void printfloat(float number);
void print(int32_t number);
void ResetClock();

extern MIDICable* midiDebugCable;
} // namespace Debug
