#pragma once

#include <cstdint>
#include <vector>

// Mock button state — tracks which buttons are pressed and records action history.
namespace MockButtons {

struct ButtonEvent {
	uint8_t button;
	bool pressed;
};

inline bool buttonStates[256] = {};
inline std::vector<ButtonEvent> eventLog;

void reset();
void press(uint8_t button);
void release(uint8_t button);
bool isPressed(uint8_t button);

} // namespace MockButtons
