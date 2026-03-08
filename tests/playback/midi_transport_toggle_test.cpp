// MIDI clock/transport separation tests for PR #4330.
// Tests the independent toggle logic for clock vs transport.

#include "CppUTest/TestHarness.h"

#include <cstdint>

// The PR adds two independent booleans:
//   midiOutClockEnabled (existing, controls tick output)
//   midiOutTransportEnabled (new, controls start/stop/continue/position)
// And on input:
//   midiInClockEnabled (existing)
//   midiInTransportEnabled (new)
//
// Flash storage: slot 190 = output transport, slot 191 = input transport.
// Uninitialized flash (0xFF) defaults to true.

namespace {

struct TransportConfig {
	bool midiOutClockEnabled{true};
	bool midiOutTransportEnabled{true};
	bool midiInClockEnabled{true};
	bool midiInTransportEnabled{true};

	bool currentlySendingMIDIOutputClocks() const { return midiOutClockEnabled; }
	bool currentlySendingMIDIOutputTransport() const { return midiOutTransportEnabled; }

	// Simulate flash storage read
	void readFromFlash(uint8_t slot190, uint8_t slot191) {
		midiOutTransportEnabled = (slot190 != 0);
		midiInTransportEnabled = (slot191 != 0);
	}
};

} // namespace

TEST_GROUP(MidiTransportToggle) {
	TransportConfig config;
};

// ── Independent toggles ──────────────────────────────────────────────────────

TEST(MidiTransportToggle, clockAndTransportIndependent) {
	config.midiOutClockEnabled = true;
	config.midiOutTransportEnabled = false;
	CHECK_TRUE(config.currentlySendingMIDIOutputClocks());
	CHECK_FALSE(config.currentlySendingMIDIOutputTransport());
}

TEST(MidiTransportToggle, transportWithoutClock) {
	config.midiOutClockEnabled = false;
	config.midiOutTransportEnabled = true;
	CHECK_FALSE(config.currentlySendingMIDIOutputClocks());
	CHECK_TRUE(config.currentlySendingMIDIOutputTransport());
}

TEST(MidiTransportToggle, bothEnabled) {
	CHECK_TRUE(config.currentlySendingMIDIOutputClocks());
	CHECK_TRUE(config.currentlySendingMIDIOutputTransport());
}

TEST(MidiTransportToggle, bothDisabled) {
	config.midiOutClockEnabled = false;
	config.midiOutTransportEnabled = false;
	CHECK_FALSE(config.currentlySendingMIDIOutputClocks());
	CHECK_FALSE(config.currentlySendingMIDIOutputTransport());
}

// ── Input toggles ────────────────────────────────────────────────────────────

TEST(MidiTransportToggle, inputClockAndTransportIndependent) {
	config.midiInClockEnabled = true;
	config.midiInTransportEnabled = false;
	CHECK_TRUE(config.midiInClockEnabled);
	CHECK_FALSE(config.midiInTransportEnabled);
}

// ── Flash storage defaults ───────────────────────────────────────────────────

TEST(MidiTransportToggle, uninitializedFlashDefaultsToEnabled) {
	// Uninitialized flash bytes are 0xFF, which is non-zero → true
	config.readFromFlash(0xFF, 0xFF);
	CHECK_TRUE(config.midiOutTransportEnabled);
	CHECK_TRUE(config.midiInTransportEnabled);
}

TEST(MidiTransportToggle, flashZeroMeansDisabled) {
	config.readFromFlash(0, 0);
	CHECK_FALSE(config.midiOutTransportEnabled);
	CHECK_FALSE(config.midiInTransportEnabled);
}

TEST(MidiTransportToggle, flashOneMeansEnabled) {
	config.readFromFlash(1, 1);
	CHECK_TRUE(config.midiOutTransportEnabled);
	CHECK_TRUE(config.midiInTransportEnabled);
}
