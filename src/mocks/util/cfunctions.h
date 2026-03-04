// Shadow header for util/cfunctions.h
// The real implementations are in cfunctions.c which has ARM timer/delay deps.
// We provide the same declarations and implement stubs in cfunctions_stubs.cpp.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t getNumDecimalDigits(uint32_t number);
void intToString(int32_t number, char* buffer, int32_t minNumDigits);
void floatToString(float number, char* __restrict__ buffer, int32_t minNumDecimalPlaces, int32_t maxNumDecimalPlaces);
void slotToString(int32_t slot, int32_t subSlot, char* __restrict__ buffer, int32_t minNumDigits);

uint32_t fastTimerCountToUS(uint32_t timerCount);
uint32_t usToFastTimerCount(uint32_t us);
uint32_t msToSlowTimerCount(uint32_t ms);
uint32_t superfastTimerCountToUS(uint32_t timerCount);
uint32_t superfastTimerCountToNS(uint32_t timerCount);

void delayMS(uint32_t ms);
void delayUS(uint32_t us);

#ifdef __cplusplus
}
#endif
