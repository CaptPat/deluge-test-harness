#pragma once

#include <cstdint>
#include <string>

// Minimal Display mock that captures display state for test assertions.
// Extends the firmware's test mock with state capture.
class Display {
public:
	bool have7SEG() { return false; }
	bool haveOLED() { return true; }

	// Capture display text for assertions
	std::string lastText;
	void setText(const char* text) { lastText = text ? text : ""; }

	void clearMainImage() {}
	void sendMainImage() {}
};

extern Display* display;
extern Display testDisplay;
