#pragma once

#include <cstdint>

// Mock timer system — advances simulated time for test scenarios.
extern "C" {
void passMockTime(double seconds);
void enableTimer(int timerNo);
void disableTimer(int timerNo);
bool isTimerEnabled(int timerNo);
void setTimerValue(int timerNo, uint32_t timerValue);
uint32_t getTimerValue(int timerNo);
}
