#pragma once

#include "util/d_string.h"
#include <cstdint>

// Stubs for functions.h utilities that reference hardware or pull in too many
// dependencies. Real implementations live in util/functions.cpp (58KB).

inline int32_t getNoteMagnitudeFfromNoteLength(uint32_t noteLength, int32_t tickMagnitude) {
	(void)noteLength;
	(void)tickMagnitude;
	return 1;
}

inline void getNoteLengthNameFromMagnitude(StringBuf& buf, int32_t magnitude,
                                           char const* durationSuffix = "-notes",
                                           bool clarifyPerColumn = false) {
	(void)buf;
	(void)magnitude;
	(void)durationSuffix;
	(void)clarifyPerColumn;
}
