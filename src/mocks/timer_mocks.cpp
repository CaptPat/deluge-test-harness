#include "timer_mocks.h"
#include <chrono>

extern "C" {

static std::chrono::time_point<std::chrono::steady_clock> timers[2];
static uint32_t mockTimers[2]{0};

// Approximate Deluge clock rate (400MHz Cortex-A9)
static constexpr uint32_t MOCK_CLOCKS_PER_SEC = 33333333; // OSTM rate

void enableTimer(int timerNo) {
	timers[timerNo] = std::chrono::steady_clock::now();
	mockTimers[timerNo] = 0;
}

void disableTimer(int timerNo) {}

bool isTimerEnabled(int timerNo) { return true; }

void setTimerValue(int timerNo, uint32_t timerValue) {
	timers[timerNo] = std::chrono::steady_clock::now();
	mockTimers[timerNo] = timerValue;
}

void passMockTime(double seconds) {
	uint32_t ticks = static_cast<uint32_t>(seconds * MOCK_CLOCKS_PER_SEC);
	mockTimers[0] += ticks;
	mockTimers[1] += ticks;
}

uint32_t getTimerValue(int timerNo) {
	passMockTime(0.0000001);
	return mockTimers[timerNo];
}

} // extern "C"
