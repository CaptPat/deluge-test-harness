#pragma once

#include "audio_engine_mock.h"
#include "button_mock.h"
#include "display_mock.h"
#include "encoder_mock.h"
#include "matrix_driver_mock.h"
#include "midi_engine_mock.h"
#include "song_mock.h"
#include "timer_mocks.h"
#include "ui_mock.h"

#include <cstdint>
#include <string>

/// High-level test fixture providing a DSL for integration tests.
///
/// Usage:
///   TestFixture f;
///   f.pressButton(PLAY);
///   f.advance(0.1);  // 100ms
///   CHECK(f.isPlaying());
///
/// All mock subsystems are reset in the constructor.
class TestFixture {
public:
	TestFixture();
	~TestFixture() = default;

	// ── Time ────────────────────────────────────────────────────────
	/// Advance simulated time by the given number of seconds.
	void advance(double seconds);

	// ── Button input ────────────────────────────────────────────────
	void pressButton(uint8_t button);
	void releaseButton(uint8_t button);
	/// Press and release a button (with a small time advance between).
	void tapButton(uint8_t button);
	bool isButtonPressed(uint8_t button) const;

	// ── Pad input ───────────────────────────────────────────────────
	void pressPad(int32_t x, int32_t y, int32_t velocity = 127);
	void releasePad(int32_t x, int32_t y);
	bool isPadPressed(int32_t x, int32_t y) const;

	// ── Encoder input ───────────────────────────────────────────────
	void turnEncoder(int32_t encoderIndex, int32_t delta);

	// ── MIDI input ──────────────────────────────────────────────────
	void sendMIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
	void sendMIDINoteOff(uint8_t channel, uint8_t note);
	void sendMIDICC(uint8_t channel, uint8_t cc, uint8_t value);

	// ── State queries ───────────────────────────────────────────────
	std::string getDisplayText() const;
	bool hasMIDIOutput() const;
	MockMIDIMessage getLastMIDIOutput() const;
	int32_t getMIDIOutputCount() const;
	MockUI::ViewType getCurrentView() const;

	// ── Song state ──────────────────────────────────────────────────
	Song& getSong();

private:
	void resetAll();
};
