#include "io/debug/print.h"
#include <cstdio>
#include <iostream>

namespace Debug {

MIDICable* midiDebugCable = nullptr;

void println(char const* output) {
	std::cout << output << std::endl;
}

void println(int32_t number) {
	std::cout << number << std::endl;
}

void print(char const* output) {
	std::cout << output;
}

void print(int32_t number) {
	std::cout << number;
}

void printlnfloat(float number) {
	std::cout << number << std::endl;
}

void printfloat(float number) {
	std::cout << number;
}

void init() {}
void ResetClock() {}

} // namespace Debug

extern "C" void putchar_(char c) {
	putchar(c);
}

// Stub for io/debug/log.h
extern "C" void logDebug(const char* fmt, ...) {
	// No-op in tests
}
