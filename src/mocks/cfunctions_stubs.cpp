// Stub implementations for util/cfunctions.h declarations.
// These replace the ARM-specific implementations in cfunctions.c.

#include "util/cfunctions.h"
#include <cstdio>
#include <cstring>

extern "C" {

int32_t getNumDecimalDigits(uint32_t number) {
	if (number == 0) {
		return 1;
	}
	int32_t count = 0;
	while (number) {
		number /= 10;
		count++;
	}
	return count;
}

void intToString(int32_t number, char* buffer, int32_t minNumDigits) {
	if (minNumDigits < 1) {
		minNumDigits = 1;
	}
	char fmt[16];
	std::snprintf(fmt, sizeof(fmt), "%%0%dd", minNumDigits);
	std::sprintf(buffer, fmt, number);
}

void floatToString(float number, char* __restrict__ buffer, int32_t minNumDecimalPlaces,
                   int32_t maxNumDecimalPlaces) {
	std::sprintf(buffer, "%.*f", maxNumDecimalPlaces, (double)number);
}

void slotToString(int32_t slot, int32_t subSlot, char* __restrict__ buffer, int32_t minNumDigits) {
	intToString(slot, buffer, minNumDigits);
}

uint32_t fastTimerCountToUS(uint32_t timerCount) { return timerCount; }
uint32_t usToFastTimerCount(uint32_t us) { return us; }
uint32_t msToSlowTimerCount(uint32_t ms) { return ms; }
uint32_t superfastTimerCountToUS(uint32_t timerCount) { return timerCount; }
uint32_t superfastTimerCountToNS(uint32_t timerCount) { return timerCount; }

void delayMS(uint32_t ms) {}
void delayUS(uint32_t us) {}

} // extern "C"
