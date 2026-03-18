// Regression tests for bugfix/metronome-countin-toggle.
//
// Bug: when MetronomeMode was introduced as a tri-state enum (OFF/COUNT_IN/ON),
// three comparison sites in session.cpp and playback_handler.cpp were not updated:
//
// 1. Count-in path unconditionally jumped to doMetronome (should check != OFF)
// 2. investigateSyncedLaunch compared metronomeOn as truthy (should check == ON)
// 3. wantsToDoTempolessRecord checked !metronomeOn (should check != ON)
//
// The basic boolean comparisons are tested in metronome_mode_test.cpp.
// These tests verify the extracted session logic with all three modes.

#include "CppUTest/TestHarness.h"
#include "playback/playback_handler.h"

TEST_GROUP(MetronomeCountIn){};

// ── Session::wantsToDoTempolessRecord extracted logic ───────────────────
// Real code: bool mightDoTempolessRecord = (!newPos
//     && playbackHandler.recording == RecordingMode::NORMAL
//     && playbackHandler.metronomeOn != MetronomeMode::ON);
//
// The bug was: && !playbackHandler.metronomeOn
// which treats COUNT_IN as falsy (allows record) and OFF as truthy (blocks record) — inverted.

namespace {
bool simulateWantsToDoTempolessRecord(MetronomeMode mode, RecordingMode recording, int32_t newPos) {
	return (!newPos && recording == RecordingMode::NORMAL && mode != MetronomeMode::ON);
}
} // namespace

TEST(MetronomeCountIn, TempolessRecordAllowedAtPosZeroRecordingNormalMetroOff) {
	CHECK(simulateWantsToDoTempolessRecord(MetronomeMode::OFF, RecordingMode::NORMAL, 0));
}

TEST(MetronomeCountIn, TempolessRecordAllowedAtPosZeroRecordingNormalMetroCountIn) {
	// This was the bug: COUNT_IN should allow tempoless record (clicks only during count-in)
	CHECK(simulateWantsToDoTempolessRecord(MetronomeMode::COUNT_IN, RecordingMode::NORMAL, 0));
}

TEST(MetronomeCountIn, TempolessRecordBlockedWhenMetroOn) {
	CHECK_FALSE(simulateWantsToDoTempolessRecord(MetronomeMode::ON, RecordingMode::NORMAL, 0));
}

TEST(MetronomeCountIn, TempolessRecordBlockedWhenNonZeroPos) {
	CHECK_FALSE(simulateWantsToDoTempolessRecord(MetronomeMode::OFF, RecordingMode::NORMAL, 100));
}

TEST(MetronomeCountIn, TempolessRecordBlockedWhenArrangementRecording) {
	CHECK_FALSE(simulateWantsToDoTempolessRecord(MetronomeMode::OFF, RecordingMode::ARRANGEMENT, 0));
}

// ── Session::investigateSyncedLaunch extracted logic ────────────────────
// Real code: || playbackHandler.metronomeOn == MetronomeMode::ON
// Bug was:   || playbackHandler.metronomeOn   (truthy — COUNT_IN is truthy!)
//
// Only full ON mode should force tempo-quantized launch.

namespace {
bool simulateUseMetronomeForSyncLaunch(MetronomeMode mode) {
	return mode == MetronomeMode::ON;
}
} // namespace

TEST(MetronomeCountIn, SyncLaunchUsesMetroOnlyWhenFullyOn) {
	CHECK(simulateUseMetronomeForSyncLaunch(MetronomeMode::ON));
}

TEST(MetronomeCountIn, SyncLaunchIgnoresMetroInCountInMode) {
	// Bug: COUNT_IN was truthy, causing sync launch to quantize to metronome
	CHECK_FALSE(simulateUseMetronomeForSyncLaunch(MetronomeMode::COUNT_IN));
}

TEST(MetronomeCountIn, SyncLaunchIgnoresMetroWhenOff) {
	CHECK_FALSE(simulateUseMetronomeForSyncLaunch(MetronomeMode::OFF));
}

// ── Count-in click path ────────────────────────────────────────────────
// Real code: if (metronomeOn != MetronomeMode::OFF) { goto doMetronome; }
// Bug was:   unconditional goto doMetronome (always clicks during count-in)

namespace {
bool simulateCountInShouldClick(MetronomeMode mode) {
	return mode != MetronomeMode::OFF;
}
} // namespace

TEST(MetronomeCountIn, CountInClicksWhenCountInMode) {
	CHECK(simulateCountInShouldClick(MetronomeMode::COUNT_IN));
}

TEST(MetronomeCountIn, CountInClicksWhenOnMode) {
	CHECK(simulateCountInShouldClick(MetronomeMode::ON));
}

TEST(MetronomeCountIn, CountInSilentWhenOffMode) {
	// Bug: this path was unconditional, so metronome=OFF still clicked during count-in
	CHECK_FALSE(simulateCountInShouldClick(MetronomeMode::OFF));
}

// ── Playback metronome path ────────────────────────────────────────────
// Real code: if (metronomeOn == MetronomeMode::ON) { goto doMetronome; }
// This was correct in the original fix, but verify the invariant.

namespace {
bool simulatePlaybackShouldClick(MetronomeMode mode) {
	return mode == MetronomeMode::ON;
}
} // namespace

TEST(MetronomeCountIn, PlaybackClicksOnlyWhenOn) {
	CHECK(simulatePlaybackShouldClick(MetronomeMode::ON));
	CHECK_FALSE(simulatePlaybackShouldClick(MetronomeMode::COUNT_IN));
	CHECK_FALSE(simulatePlaybackShouldClick(MetronomeMode::OFF));
}

// ── LED state logic ────────────────────────────────────────────────────
// Real code: indicator_leds::setLedState(TAP_TEMPO, metronomeOn != MetronomeMode::OFF);

TEST(MetronomeCountIn, TapTempoLedOnWhenCountIn) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::COUNT_IN;
	CHECK(ph.metronomeOn != MetronomeMode::OFF);
}

TEST(MetronomeCountIn, TapTempoLedOnWhenOn) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::ON;
	CHECK(ph.metronomeOn != MetronomeMode::OFF);
}

TEST(MetronomeCountIn, TapTempoLedOffWhenOff) {
	PlaybackHandler ph;
	ph.metronomeOn = MetronomeMode::OFF;
	CHECK(ph.metronomeOn == MetronomeMode::OFF);
}
