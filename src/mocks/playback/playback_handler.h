// Shadow header replacing firmware's playback/playback_handler.h
// The real header (276 lines) pulls in Song, Clip, Kit, MIDICable, d_string.
// We provide a minimal stub with only what compiled firmware code needs.

#pragma once
#include <cstdint>

class Song;

enum class RecordingMode {
	OFF,
	NORMAL,
	ARRANGEMENT,
};

enum class MetronomeMode : uint8_t {
	OFF,      // Silent always
	COUNT_IN, // Clicks during count-in only
	ON,       // Clicks always
};

class PlaybackHandler {
public:
	PlaybackHandler() : metronomeOn(MetronomeMode::COUNT_IN) {}

	// Returns inverse of time-per-tick; ~42949672 ≈ 120 BPM at 44.1kHz
	uint32_t getTimePerInternalTickInverse(bool getStickyValue = false) { return 42949672; }

	// Phase 10: consequence system stubs
	RecordingMode recording = RecordingMode::OFF;
	uint8_t playbackState = 0;
	MetronomeMode metronomeOn;

	// Session launch scheduling fields (real firmware fields)
	int64_t lastSwungTickActioned = 0;
	int32_t swungTicksTilNextEvent = 2147483647; // INT32_MAX = no event pending
	bool stopOutputRecordingAtLoopEnd = false;

	void toggleMetronomeStatus();
	void scheduleSwungTick() {} // No-op in mock — no timer hardware

	float calculateBPM(float timePerTimerTick) {
		(void)timePerTimerTick;
		return 120.0f;
	}
	void forceResetPlayPos(Song* song) { (void)song; }
	bool isInternalClockActive() { return playbackState != 0; }
	bool isEitherClockActive() { return playbackState != 0; }
	void endPlayback() { playbackState = 0; }
	void setupPlaybackUsingInternalClock(int32_t buttonPressLatency = 0) {
		(void)buttonPressLatency;
		playbackState = 1;
	}
};

extern PlaybackHandler playbackHandler;
