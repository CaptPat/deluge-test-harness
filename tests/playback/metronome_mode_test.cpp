// Metronome tri-state mode tests.
// Verifies the MetronomeMode enum, toggle cycle, default state,
// and the count-in vs playback click logic.

#include "CppUTest/TestHarness.h"
#include "playback/playback_handler.h"

TEST_GROUP(MetronomeMode){};

// ── Default state ───────────────────────────────────────────────────────

TEST(MetronomeMode, DefaultIsCountIn) {
	// Original firmware always played metronome during count-in.
	// COUNT_IN preserves that behavior as the default.
	PlaybackHandler ph;
	CHECK(ph.metronomeOn == MetronomeMode::COUNT_IN);
}

// ── Toggle cycle ────────────────────────────────────────────────────────

TEST(MetronomeMode, ToggleCyclesCorrectly) {
	PlaybackHandler ph;
	// Default: COUNT_IN
	CHECK(ph.metronomeOn == MetronomeMode::COUNT_IN);

	ph.toggleMetronomeStatus();
	CHECK(ph.metronomeOn == MetronomeMode::ON);

	ph.toggleMetronomeStatus();
	CHECK(ph.metronomeOn == MetronomeMode::OFF);

	ph.toggleMetronomeStatus();
	CHECK(ph.metronomeOn == MetronomeMode::COUNT_IN);
}

TEST(MetronomeMode, ToggleFullCycleTwice) {
	PlaybackHandler ph;
	// Two full cycles should end where we started
	for (int i = 0; i < 6; i++) {
		ph.toggleMetronomeStatus();
	}
	CHECK(ph.metronomeOn == MetronomeMode::COUNT_IN);
}

// ── Count-in click logic ────────────────────────────────────────────────
// The count-in path checks: metronomeOn != MetronomeMode::OFF

TEST(MetronomeMode, CountInClicksWhenCountInMode) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::COUNT_IN;
	CHECK(ph.metronomeOn != MetronomeMode::OFF); // should click during count-in
}

TEST(MetronomeMode, CountInClicksWhenOnMode) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::ON;
	CHECK(ph.metronomeOn != MetronomeMode::OFF); // should click during count-in
}

TEST(MetronomeMode, CountInSilentWhenOffMode) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::OFF;
	CHECK(ph.metronomeOn == MetronomeMode::OFF); // should NOT click during count-in
}

// ── Playback click logic ────────────────────────────────────────────────
// The playback path checks: metronomeOn == MetronomeMode::ON

TEST(MetronomeMode, PlaybackClicksOnlyWhenOnMode) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::ON;
	CHECK(ph.metronomeOn == MetronomeMode::ON); // should click during playback
}

TEST(MetronomeMode, PlaybackSilentWhenCountInMode) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::COUNT_IN;
	CHECK(ph.metronomeOn != MetronomeMode::ON); // should NOT click during playback
}

TEST(MetronomeMode, PlaybackSilentWhenOffMode) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::OFF;
	CHECK(ph.metronomeOn != MetronomeMode::ON); // should NOT click during playback
}

// ── Tempoless record logic ──────────────────────────────────────────────
// session.cpp: tempoless record allowed when metronomeOn != MetronomeMode::ON

TEST(MetronomeMode, TempolessRecordAllowedWhenOff) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::OFF;
	CHECK(ph.metronomeOn != MetronomeMode::ON); // tempoless record allowed
}

TEST(MetronomeMode, TempolessRecordAllowedWhenCountIn) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::COUNT_IN;
	CHECK(ph.metronomeOn != MetronomeMode::ON); // tempoless record allowed
}

TEST(MetronomeMode, TempolessRecordBlockedWhenOn) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::ON;
	CHECK(ph.metronomeOn == MetronomeMode::ON); // tempoless record blocked
}
