#pragma once

#include <cstdint>
#include <vector>

// Mock pad matrix (18 x 8 grid: 16 main + 2 sidebar columns, 8 rows).
static constexpr int32_t kMockDisplayWidth = 16;
static constexpr int32_t kMockSideBarWidth = 2;
static constexpr int32_t kMockDisplayHeight = 8;

namespace MockMatrixDriver {

struct PadEvent {
	int32_t x;
	int32_t y;
	int32_t velocity; // 0 = release, >0 = press
};

inline bool padStates[kMockDisplayWidth + kMockSideBarWidth][kMockDisplayHeight] = {};
inline std::vector<PadEvent> eventLog;

void reset();
void pressPad(int32_t x, int32_t y, int32_t velocity = 127);
void releasePad(int32_t x, int32_t y);
bool isPadPressed(int32_t x, int32_t y);

} // namespace MockMatrixDriver
