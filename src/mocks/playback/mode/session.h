// Shadow header replacing firmware's playback/mode/session.h
// The real header defines the Session class (PlaybackMode subclass).
// We provide a minimal stub with all pure virtuals implemented as no-ops,
// plus real implementations of pure-logic methods for testing.

#pragma once

#include "definitions_cxx.hpp"
#include "gui/colour/colour.h"
#include "playback/mode/playback_mode.h"
#include "playback/playback_handler.h"

class Clip;
class Song;

extern Song* currentSong;
extern Song* preLoadedSong;

extern const RGB defaultClipSectionColours[];

#define SECTION_OUT_OF_RANGE 254
#define REALLY_OUT_OF_RANGE 255

class Session : public PlaybackMode {
public:
	Session() { cancelAllLaunchScheduling(); }

	void setupPlayback() override {}
	bool endPlayback() override { return false; }
	void doTickForward(int32_t posIncrement) override { (void)posIncrement; }
	void resetPlayPos(int32_t newPos, bool doingComplete = true, int32_t buttonPressLatency = 0) override {
		(void)newPos;
		(void)doingComplete;
		(void)buttonPressLatency;
	}
	void resyncToSongTicks(Song* song) override { (void)song; }
	void reversionDone() override {}
	bool isOutputAvailable(Output* output) override {
		(void)output;
		return true;
	}
	void stopOutputRecordingAtLoopEnd() override {}
	int32_t getPosAtWhichClipWillCut(ModelStackWithTimelineCounter const* modelStack) override {
		(void)modelStack;
		return 2147483647;
	}
	bool willClipContinuePlayingAtEnd(ModelStackWithTimelineCounter const* modelStack) override {
		(void)modelStack;
		return false;
	}
	bool willClipLoopAtSomePoint(ModelStackWithTimelineCounter const* modelStack) override {
		(void)modelStack;
		return false;
	}
	void reSyncClip(ModelStackWithTimelineCounter* modelStack, bool mustSetPosToSomething = false,
	                bool mayResumeClip = true) override {
		(void)modelStack;
		(void)mustSetPosToSomething;
		(void)mayResumeClip;
	}

	// --- Real implementations of pure-logic methods (from session.cpp) ---

	// cancelAllLaunchScheduling: resets launch event tick to 0
	void cancelAllLaunchScheduling() { launchEventAtSwungTickCount = 0; }

	// scheduleLaunchTiming: schedules a launch event at a future tick count
	void scheduleLaunchTiming(int64_t atTickCount, int32_t numRepeatsUntil,
	                          int32_t armedLaunchLengthForOneRepeat) {
		if (atTickCount > launchEventAtSwungTickCount) {
			playbackHandler.stopOutputRecordingAtLoopEnd = false;
			switchToArrangementAtLaunchEvent = false;
			launchEventAtSwungTickCount = atTickCount;
			numRepeatsTilLaunch = numRepeatsUntil;
			currentArmedLaunchLengthForOneRepeat = armedLaunchLengthForOneRepeat;

			int32_t ticksTilLaunchEvent = atTickCount - playbackHandler.lastSwungTickActioned;
			if (playbackHandler.swungTicksTilNextEvent > ticksTilLaunchEvent) {
				playbackHandler.swungTicksTilNextEvent = ticksTilLaunchEvent;
				playbackHandler.scheduleSwungTick();
			}
		}
	}

	// areAnyClipsArmed: checks if any session clip has a non-OFF arm state
	// Implemented out-of-line (needs Song definition from song_mock.h)
	bool areAnyClipsArmed();

	// --- Session state fields (matching real firmware) ---
	int64_t launchEventAtSwungTickCount = 0;
	int16_t numRepeatsTilLaunch = 0;
	int32_t currentArmedLaunchLengthForOneRepeat = 0;
	bool switchToArrangementAtLaunchEvent = false;
	uint8_t lastSectionArmed = REALLY_OUT_OF_RANGE;
};

extern Session session;
