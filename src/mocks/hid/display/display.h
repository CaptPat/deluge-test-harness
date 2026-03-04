// Shadow header replacing firmware's hid/display/display.h for x86 compilation.
// Provides the deluge::hid::Display abstract class with all pure virtuals
// given default no-op implementations. Avoids pulling in util/cfunctions.h.

#pragma once

#include "definitions_cxx.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

class NumericLayer;
class NumericLayerScrollingText;

enum class PopupType {
	NONE,
	GENERAL,
	LOADING,
	PROBABILITY,
	ITERANCE,
	SWING,
	TEMPO,
	QUANTIZE,
	THRESHOLD_RECORDING_MODE,
	NOTIFICATION,
};

namespace deluge::hid {

enum struct DisplayType { OLED, SEVENSEG };

class Display {
public:
	Display(DisplayType displayType) : displayType(displayType) {}
	virtual ~Display() = default;

	constexpr virtual size_t getNumBrowserAndMenuLines() { return 4; }

	virtual void setText(std::string_view newText, bool alignRight = false, uint8_t drawDot = 255,
	                     bool doBlink = false, uint8_t* newBlinkMask = nullptr, bool blinkImmediately = false,
	                     bool shouldBlinkFast = false, int32_t scrollPos = 0, uint8_t* blinkAddition = nullptr,
	                     bool justReplaceBottomLayer = false) {}

	virtual void displayPopup(char const* newText, int8_t numFlashes = 3, bool alignRight = false,
	                           uint8_t drawDot = 255, int32_t blinkSpeed = 1, PopupType type = PopupType::GENERAL) {}

	virtual void displayPopup(uint8_t val, int8_t numFlashes = 3, bool alignRight = false, uint8_t drawDot = 255,
	                           int32_t blinkSpeed = 1, PopupType type = PopupType::GENERAL) {}

	virtual void displayPopup(char const* shortLong[2], int8_t numFlashes = 3, bool alignRight = false,
	                           uint8_t drawDot = 255, int32_t blinkSpeed = 1, PopupType type = PopupType::GENERAL) {}

	virtual void popupText(char const* text, PopupType type = PopupType::GENERAL) {}
	virtual void popupTextTemporary(char const* text, PopupType type = PopupType::GENERAL) {}

	virtual void setNextTransitionDirection(int8_t thisDirection) {}

	virtual void cancelPopup() {}
	virtual void freezeWithError(char const* text) {}
	virtual bool isLayerCurrentlyOnTop(NumericLayer* layer) { return false; }
	virtual void displayError(Error error) {}

	virtual void removeWorkingAnimation() {}

	virtual void displayLoadingAnimation() {}
	virtual void displayLoadingAnimationText(char const* text, bool delayed = false, bool transparent = false) {}
	virtual void removeLoadingAnimation() {}

	virtual void displayNotification(std::string_view paramTitle, std::optional<std::string_view> paramValue) {}

	virtual bool hasPopup() { return false; }
	virtual bool hasPopupOfType(PopupType type) { return false; }

	virtual void consoleText(char const* text) {}

	virtual void timerRoutine() {}

	virtual void setTextAsNumber(int16_t number, uint8_t drawDot = 255, bool doBlink = false) {}
	virtual int32_t getEncodedPosFromLeft(int32_t textPos, char const* text, bool* andAHalf) { return 0; }
	virtual void setTextAsSlot(int16_t currentSlot, int8_t currentSubSlot, bool currentSlotExists,
	                            bool doBlink = false, int32_t blinkPos = -1, bool blinkImmediately = false) {}
	virtual void setTextWithMultipleDots(std::string_view newText, std::vector<uint8_t> dotPositions,
	                                     bool alignRight = false, bool doBlink = false,
	                                     uint8_t* newBlinkMask = nullptr, bool blinkImmediately = false) {}
	virtual NumericLayerScrollingText* setScrollingText(char const* newText, int32_t startAtPos = 0,
	                                                    int32_t initialDelay = 600, int count = -1,
	                                                    uint8_t fixedDot = 255) {
		return nullptr;
	}

	virtual std::array<uint8_t, kNumericDisplayLength> getLast() { return {0}; }

	bool haveOLED() { return displayType == DisplayType::OLED; }
	bool have7SEG() { return displayType == DisplayType::SEVENSEG; }

private:
	DisplayType displayType;
};

} // namespace deluge::hid

extern deluge::hid::Display* display;

namespace deluge::hid::display {
void swapDisplayType();
extern bool have_oled_screen;
} // namespace deluge::hid::display

extern "C" void consoleTextIfAllBootedUp(char const* text);
