// Shadow header replacing firmware's playback/mode/session.h
// The real header defines the Session class (PlaybackMode subclass).
// We provide a minimal stub with all pure virtuals implemented as no-ops.

#pragma once

#include "definitions_cxx.hpp"
#include "gui/colour/colour.h"
#include "playback/mode/playback_mode.h"

class Clip;

extern const RGB defaultClipSectionColours[];

class Session : public PlaybackMode {
public:
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
};

extern Session session;
