#include "button_mock.h"
#include <cstring>

namespace MockButtons {

void reset() {
	memset(buttonStates, 0, sizeof(buttonStates));
	eventLog.clear();
}

void press(uint8_t button) {
	buttonStates[button] = true;
	eventLog.push_back({button, true});
}

void release(uint8_t button) {
	buttonStates[button] = false;
	eventLog.push_back({button, false});
}

bool isPressed(uint8_t button) { return buttonStates[button]; }

} // namespace MockButtons
