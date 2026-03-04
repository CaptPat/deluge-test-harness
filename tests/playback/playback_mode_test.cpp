// Phase 11: PlaybackMode tests
// Tests the base PlaybackMode class (constructor, destructor, hasPlaybackActive)

#include "CppUTest/TestHarness.h"
#include "playback/mode/playback_mode.h"
#include "playback/playback_handler.h"

// Concrete subclass for testing (PlaybackMode has pure virtuals)
class TestPlaybackMode : public PlaybackMode {
public:
	void setupPlayback() override {}
	bool endPlayback() override { return false; }
	void doTickForward(int32_t) override {}
	void resetPlayPos(int32_t, bool, int32_t) override {}
	void resyncToSongTicks(Song*) override {}
	void reversionDone() override {}
	bool isOutputAvailable(Output*) override { return true; }
	void stopOutputRecordingAtLoopEnd() override {}
	int32_t getPosAtWhichClipWillCut(ModelStackWithTimelineCounter const*) override { return 0; }
	bool willClipContinuePlayingAtEnd(ModelStackWithTimelineCounter const*) override { return false; }
	bool willClipLoopAtSomePoint(ModelStackWithTimelineCounter const*) override { return false; }
	void reSyncClip(ModelStackWithTimelineCounter*, bool, bool) override {}
};

TEST_GROUP(PlaybackMode) {
	void setup() {
		currentPlaybackMode = nullptr;
		playbackHandler.playbackState = 0;
	}
};

TEST(PlaybackMode, constructorDestructorNocrash) {
	TestPlaybackMode pm;
	CHECK(true);
}

TEST(PlaybackMode, hasPlaybackActiveWhenNotCurrentMode) {
	TestPlaybackMode pm;
	currentPlaybackMode = nullptr;
	playbackHandler.playbackState = 1;
	CHECK(!pm.hasPlaybackActive());
}

TEST(PlaybackMode, hasPlaybackActiveWhenPlaybackStopped) {
	TestPlaybackMode pm;
	currentPlaybackMode = &pm;
	playbackHandler.playbackState = 0;
	CHECK(!pm.hasPlaybackActive());
}

TEST(PlaybackMode, hasPlaybackActiveWhenBothConditionsMet) {
	TestPlaybackMode pm;
	currentPlaybackMode = &pm;
	playbackHandler.playbackState = 1;
	CHECK(pm.hasPlaybackActive());
}

TEST(PlaybackMode, currentPlaybackModeExtern) {
	TestPlaybackMode pm;
	currentPlaybackMode = &pm;
	CHECK(currentPlaybackMode == &pm);
	currentPlaybackMode = nullptr;
}
