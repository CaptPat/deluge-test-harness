#pragma once

#include <cstdint>
#include <vector>

// Mock encoder inputs (SELECT, TEMPO, MOD_0, MOD_1, SCROLL_X, SCROLL_Y).
namespace MockEncoders {

struct EncoderEvent {
	int32_t encoderIndex;
	int32_t delta;
};

inline std::vector<EncoderEvent> eventLog;

void reset();
void turn(int32_t encoderIndex, int32_t delta);

} // namespace MockEncoders
