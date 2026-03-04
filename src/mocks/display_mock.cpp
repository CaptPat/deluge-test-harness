#include "display_mock.h"

MockDisplay testDisplay;
deluge::hid::Display* display = &testDisplay;

namespace deluge::hid::display {
bool have_oled_screen = true;
void swapDisplayType() {}
} // namespace deluge::hid::display
