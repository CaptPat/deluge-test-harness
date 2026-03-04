#pragma once

#include "hid/display/display.h"
#include <string>

// Concrete Display mock that captures state for test assertions.
// Inherits from the firmware's abstract Display class.
class MockDisplay : public deluge::hid::Display {
public:
	MockDisplay() : Display(deluge::hid::DisplayType::OLED) {}

	// Captured state for assertions
	std::string lastText;

	void setText(std::string_view newText, bool alignRight = false, uint8_t drawDot = 255, bool doBlink = false,
	             uint8_t* newBlinkMask = nullptr, bool blinkImmediately = false, bool shouldBlinkFast = false,
	             int32_t scrollPos = 0, uint8_t* blinkAddition = nullptr,
	             bool justReplaceBottomLayer = false) override {
		lastText = std::string(newText);
	}

	void displayPopup(char const* newText, int8_t numFlashes = 3, bool alignRight = false, uint8_t drawDot = 255,
	                   int32_t blinkSpeed = 1, PopupType type = PopupType::GENERAL) override {
		lastText = newText ? newText : "";
	}
};

extern MockDisplay testDisplay;
