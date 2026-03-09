// Phase 9C: HID tests — Button coordinate conversion, Pad encoding, ClipMinder

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "hid/button.h"
#include "hid/matrix/pad.h"
#include "model/clip/clip_minder.h"

using namespace deluge::hid::button;

// ── Button ──────────────────────────────────────────────────────────────

TEST_GROUP(ButtonTest){};

TEST(ButtonTest, fromXYtoXYRoundTrip) {
	// Test several coordinate pairs round-trip through encoding/decoding
	for (int32_t y = 0; y < 4; y++) {
		for (int32_t x = 0; x < 9; x++) {
			uint8_t encoded = fromXY(x, y);
			Cartesian decoded = toXY(encoded);
			CHECK_EQUAL(x, decoded.x);
			CHECK_EQUAL(y, decoded.y);
		}
	}
}

TEST(ButtonTest, fromCartesianMatchesFromXY) {
	Cartesian c = {3, 2};
	CHECK_EQUAL(fromXY(3, 2), fromCartesian(c));
}

TEST(ButtonTest, knownButtonShift) {
	// Shift button is at {8, 0}
	uint8_t shift = fromXY(8, 0);
	Cartesian decoded = toXY(shift);
	CHECK_EQUAL(8, decoded.x);
	CHECK_EQUAL(0, decoded.y);
	CHECK_EQUAL(shift, SHIFT);
}

TEST(ButtonTest, knownButtonPlay) {
	// Play button is at {8, 3}
	uint8_t play = fromXY(8, 3);
	Cartesian decoded = toXY(play);
	CHECK_EQUAL(8, decoded.x);
	CHECK_EQUAL(3, decoded.y);
	CHECK_EQUAL(play, PLAY);
}

TEST(ButtonTest, knownButtonRecord) {
	// Record button is at {8, 2}
	uint8_t record = fromXY(8, 2);
	CHECK_EQUAL(record, RECORD);
}

TEST(ButtonTest, knownButtonBack) {
	// Back button is at {7, 1}
	uint8_t back = fromXY(7, 1);
	CHECK_EQUAL(back, BACK);
}

TEST(ButtonTest, knownButtonSynth) {
	// Synth button is at {5, 0}
	uint8_t synth = fromXY(5, 0);
	CHECK_EQUAL(synth, SYNTH);
}

// ── Pad ─────────────────────────────────────────────────────────────────

TEST_GROUP(PadTest){};

TEST(PadTest, constructFromXY) {
	Pad p(4, 3);
	CHECK_EQUAL(4, p.x);
	CHECK_EQUAL(3, p.y);
}

TEST(PadTest, toCharRoundTripEvenX) {
	// Even x coordinates
	Pad p(4, 3);
	uint8_t encoded = p.toChar();
	Pad decoded(encoded);
	CHECK_EQUAL(4, decoded.x);
	CHECK_EQUAL(3, decoded.y);
}

TEST(PadTest, toCharRoundTripOddX) {
	// Odd x coordinates use the upper half of the encoding
	Pad p(5, 3);
	uint8_t encoded = p.toChar();
	Pad decoded(encoded);
	CHECK_EQUAL(5, decoded.x);
	CHECK_EQUAL(3, decoded.y);
}

TEST(PadTest, isPadValidCoord) {
	Pad p(0, 0);
	CHECK(p.isPad());
}

TEST(PadTest, isPadSidebarCoord) {
	// Sidebar pads (x = kDisplayWidth to kDisplayWidth + kSideBarWidth - 1)
	Pad p(kDisplayWidth, 0);
	CHECK(p.isPad());
}

TEST(PadTest, staticIsPadValidLow) {
	CHECK(Pad::isPad(0));
}

TEST(PadTest, staticIsPadMaxValid) {
	// kDisplayHeight * 2 * 9 - 1 = 143
	CHECK(Pad::isPad(static_cast<uint8_t>(kDisplayHeight * 2 * 9 - 1)));
}

TEST(PadTest, staticIsPadInvalidAtBoundary) {
	// kDisplayHeight * 2 * 9 = 144 — first invalid value
	CHECK(!Pad::isPad(static_cast<uint8_t>(kDisplayHeight * 2 * 9)));
}

TEST(PadTest, staticIsPadInvalidHigh) {
	CHECK(!Pad::isPad(255));
}

TEST(PadTest, isPadInvalidY) {
	Pad p(0, kDisplayHeight);
	CHECK(!p.isPad());
}

TEST(PadTest, isPadInvalidX) {
	Pad p(kDisplayWidth + kSideBarWidth, 0);
	CHECK(!p.isPad());
}

TEST(PadTest, multipleRoundTrips) {
	// Test a range of valid pad coordinates
	for (int y = 0; y < kDisplayHeight; y++) {
		for (int x = 0; x < kDisplayWidth + kSideBarWidth; x++) {
			Pad original(x, y);
			uint8_t encoded = original.toChar();
			Pad decoded(encoded);
			CHECK_EQUAL(x, decoded.x);
			CHECK_EQUAL(y, decoded.y);
		}
	}
}

// ── ClipMinder ──────────────────────────────────────────────────────────

TEST_GROUP(ClipMinderTest){};

TEST(ClipMinderTest, buttonActionReturnsNotDealtWith) {
	ClipMinder minder;
	ActionResult result = minder.buttonAction(PLAY, true);
	CHECK(result == ActionResult::NOT_DEALT_WITH);
}

TEST(ClipMinderTest, buttonActionReleaseAlsoNotDealtWith) {
	ClipMinder minder;
	ActionResult result = minder.buttonAction(SHIFT, false);
	CHECK(result == ActionResult::NOT_DEALT_WITH);
}
