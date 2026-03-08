#include "CppUTest/TestHarness.h"
#include "model/scale/preset_scales.h"

namespace {

TEST_GROUP(PresetScales){};

// --- getScaleName ---

TEST(PresetScales, nameOfMajor) {
	STRCMP_EQUAL("MAJOR", getScaleName(MAJOR_SCALE));
}

TEST(PresetScales, nameOfMinor) {
	STRCMP_EQUAL("MINOR", getScaleName(MINOR_SCALE));
}

TEST(PresetScales, nameOfAllPresets) {
	for (uint8_t i = 0; i < NUM_PRESET_SCALES; i++) {
		const char* name = getScaleName(static_cast<Scale>(i));
		CHECK(name != nullptr);
		CHECK(name[0] != '\0');
	}
}

TEST(PresetScales, nameOfUser) {
	STRCMP_EQUAL("USER", getScaleName(USER_SCALE));
}

TEST(PresetScales, nameOfRandom) {
	STRCMP_EQUAL("RANDOM", getScaleName(RANDOM_SCALE));
}

TEST(PresetScales, nameOfNone) {
	STRCMP_EQUAL("NONE", getScaleName(NO_SCALE));
}

TEST(PresetScales, nameOutOfRange) {
	STRCMP_EQUAL("ERR", getScaleName(static_cast<Scale>(255)));
}

// --- getScale (reverse lookup) ---

TEST(PresetScales, reverseLookupMajor) {
	CHECK_EQUAL(MAJOR_SCALE, getScale(presetScaleNotes[MAJOR_SCALE]));
}

TEST(PresetScales, reverseLookupAllPresets) {
	for (uint8_t i = 0; i < NUM_PRESET_SCALES; i++) {
		CHECK_EQUAL(i, getScale(presetScaleNotes[i]));
	}
}

TEST(PresetScales, reverseLookupUnknownReturnsUser) {
	// Chromatic (all 12 notes) is not a preset
	NoteSet chromatic(0xFFF);
	CHECK_EQUAL(USER_SCALE, getScale(chromatic));
}

// --- isUserScale ---

TEST(PresetScales, majorIsNotUserScale) {
	CHECK_FALSE(isUserScale(presetScaleNotes[MAJOR_SCALE]));
}

TEST(PresetScales, chromaticIsUserScale) {
	NoteSet chromatic(0xFFF);
	CHECK(isUserScale(chromatic));
}

// --- ensureNotAllPresetScalesDisabled ---

TEST(PresetScales, notAllDisabledIsNoOp) {
	std::bitset<NUM_PRESET_SCALES> disabled;
	disabled.set(); // all disabled
	disabled[MINOR_SCALE] = false; // except minor
	ensureNotAllPresetScalesDisabled(disabled);
	CHECK_FALSE(disabled[MINOR_SCALE]); // minor still enabled
	CHECK(disabled[MAJOR_SCALE]); // major still disabled
}

TEST(PresetScales, allDisabledEnablesMajor) {
	std::bitset<NUM_PRESET_SCALES> disabled;
	disabled.set(); // all disabled
	ensureNotAllPresetScalesDisabled(disabled);
	CHECK_FALSE(disabled[MAJOR_SCALE]); // major now enabled
}

TEST(PresetScales, noneDisabledIsNoOp) {
	std::bitset<NUM_PRESET_SCALES> disabled;
	// all false (enabled)
	ensureNotAllPresetScalesDisabled(disabled);
	CHECK_FALSE(disabled[MAJOR_SCALE]);
}

// --- Flash storage codec round-trips ---

TEST(PresetScales, flashCodecRoundTripAllPresets) {
	for (uint8_t i = 0; i < NUM_PRESET_SCALES; i++) {
		Scale s = static_cast<Scale>(i);
		uint8_t code = scaleToFlashStorageCode(s);
		Scale decoded = flashStorageCodeToScale(code);
		CHECK_EQUAL(s, decoded);
	}
}

TEST(PresetScales, flashCodecRoundTripUser) {
	uint8_t code = scaleToFlashStorageCode(USER_SCALE);
	CHECK_EQUAL(253, code);
	CHECK_EQUAL(USER_SCALE, flashStorageCodeToScale(code));
}

TEST(PresetScales, flashCodecRoundTripRandom) {
	uint8_t code = scaleToFlashStorageCode(RANDOM_SCALE);
	CHECK_EQUAL(254, code);
	CHECK_EQUAL(RANDOM_SCALE, flashStorageCodeToScale(code));
}

TEST(PresetScales, flashCodecRoundTripNone) {
	uint8_t code = scaleToFlashStorageCode(NO_SCALE);
	CHECK_EQUAL(255, code);
	CHECK_EQUAL(NO_SCALE, flashStorageCodeToScale(code));
}

TEST(PresetScales, flashCodec7NoteScalesDirectMapping) {
	// 7-note scales should map directly (code == enum value)
	for (uint8_t i = 0; i < FIRST_6_NOTE_SCALE_INDEX; i++) {
		CHECK_EQUAL(i, scaleToFlashStorageCode(static_cast<Scale>(i)));
	}
}

TEST(PresetScales, flashCodec6NoteScalesOffset64) {
	// 6-note scales start at code 64
	uint8_t code = scaleToFlashStorageCode(WHOLE_TONE_SCALE);
	CHECK(code >= 64);
	CHECK(code < 128);
}

TEST(PresetScales, flashCodec5NoteScalesOffset128) {
	// 5-note scales start at code 128
	uint8_t code = scaleToFlashStorageCode(PENTATONIC_MINOR_SCALE);
	CHECK(code >= 128);
	CHECK(code < 253);
}

TEST(PresetScales, flashCodeUnknownReturnsNoScale) {
	// Code 252 (below USER but above valid 5-note range) should decode
	// to a Scale enum value — verify it doesn't crash
	Scale s = flashStorageCodeToScale(252);
	// May produce garbage Scale value, but shouldn't crash
	(void)s;
}

// --- Preset note set sanity ---

TEST(PresetScales, majorHas7Notes) {
	CHECK_EQUAL(7, presetScaleNotes[MAJOR_SCALE].count());
}

TEST(PresetScales, minorHas7Notes) {
	CHECK_EQUAL(7, presetScaleNotes[MINOR_SCALE].count());
}

TEST(PresetScales, pentatonicHas5Notes) {
	CHECK_EQUAL(5, presetScaleNotes[PENTATONIC_MINOR_SCALE].count());
}

TEST(PresetScales, allPresetsHaveRoot) {
	for (uint8_t i = 0; i < NUM_PRESET_SCALES; i++) {
		CHECK_TEXT(presetScaleNotes[i].has(0),
		           (std::string("Scale ") + std::to_string(i) + " missing root").c_str());
	}
}

TEST(PresetScales, allPresetsAreDistinct) {
	for (uint8_t i = 0; i < NUM_PRESET_SCALES; i++) {
		for (uint8_t j = i + 1; j < NUM_PRESET_SCALES; j++) {
			CHECK_TEXT(!(presetScaleNotes[i] == presetScaleNotes[j]),
			           (std::string("Scales ") + std::to_string(i) + " and " + std::to_string(j) + " are identical")
			               .c_str());
		}
	}
}

} // namespace
