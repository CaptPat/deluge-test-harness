#include "matrix_driver_mock.h"
#include <cstring>

namespace MockMatrixDriver {

void reset() {
	memset(padStates, 0, sizeof(padStates));
	eventLog.clear();
}

void pressPad(int32_t x, int32_t y, int32_t velocity) {
	padStates[x][y] = true;
	eventLog.push_back({x, y, velocity});
}

void releasePad(int32_t x, int32_t y) {
	padStates[x][y] = false;
	eventLog.push_back({x, y, 0});
}

bool isPadPressed(int32_t x, int32_t y) { return padStates[x][y]; }

} // namespace MockMatrixDriver
