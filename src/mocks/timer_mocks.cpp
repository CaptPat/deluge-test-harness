#include <chrono>
#include <cstdint>

extern "C" {

#include "OSLikeStuff/timers_interrupts/clock_type.h"
#include "RZA1/ostm/ostm.h"

static std::chrono::time_point<std::chrono::steady_clock> timers[2];
static uint32_t mockTimers[2]{0};

void enableTimer(int timerNo) {
	timers[timerNo] = std::chrono::steady_clock::now();
	mockTimers[timerNo] = 0;
}

void disableTimer(int timerNo) {
	(void)timerNo;
}

bool isTimerEnabled(int timerNo) {
	(void)timerNo;
	return true;
}

void setOperatingMode(int timerNo, enum OSTimerOperatingMode mode, bool enable_interrupt) {
	(void)timerNo;
	(void)mode;
	(void)enable_interrupt;
}

void setTimerValue(int timerNo, uint32_t timerValue) {
	timers[timerNo] = std::chrono::steady_clock::now();
	mockTimers[timerNo] = timerValue;
}

void passMockTime(double seconds) {
	uint32_t ticks = static_cast<uint32_t>(seconds * DELUGE_CLOCKS_PER);
	mockTimers[0] += ticks;
	mockTimers[1] += ticks;
}

uint32_t getTimerValue(int timerNo) {
	passMockTime(0.0000001);
	return mockTimers[timerNo];
}

} // extern "C"
