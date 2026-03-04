#include "encoder_mock.h"

namespace MockEncoders {

void reset() { eventLog.clear(); }

void turn(int32_t encoderIndex, int32_t delta) {
	eventLog.push_back({encoderIndex, delta});
}

} // namespace MockEncoders
