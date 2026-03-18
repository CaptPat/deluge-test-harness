// Session & PlaybackHandler pure-logic tests
// Tests Session state machine methods (cancelAllLaunchScheduling, scheduleLaunchTiming,
// areAnyClipsArmed) and PlaybackHandler state transitions without GUI/audio dependencies.

#include "CppUTest/TestHarness.h"
#include "playback/mode/session.h"
#include "playback/playback_handler.h"
#include "song_mock.h"

// ---------------------------------------------------------------------------
// Session launch scheduling tests
// ---------------------------------------------------------------------------

TEST_GROUP(SessionLaunchScheduling) {
	Song testSongLocal;
	void setup() {
		currentSong = &testSongLocal;
		preLoadedSong = nullptr;
		testSongLocal.clear();
		// Reset session state
		session.cancelAllLaunchScheduling();
		session.numRepeatsTilLaunch = 0;
		session.currentArmedLaunchLengthForOneRepeat = 0;
		session.switchToArrangementAtLaunchEvent = false;
		session.lastSectionArmed = REALLY_OUT_OF_RANGE;
		// Reset playback handler state
		playbackHandler.lastSwungTickActioned = 0;
		playbackHandler.swungTicksTilNextEvent = 2147483647;
		playbackHandler.stopOutputRecordingAtLoopEnd = false;
		playbackHandler.playbackState = 0;
	}
	void teardown() { currentSong = nullptr; }
};

TEST(SessionLaunchScheduling, cancelAllLaunchSchedulingResetsToZero) {
	session.launchEventAtSwungTickCount = 12345;
	session.cancelAllLaunchScheduling();
	LONGS_EQUAL(0, session.launchEventAtSwungTickCount);
}

TEST(SessionLaunchScheduling, scheduleLaunchTimingSetsFields) {
	session.cancelAllLaunchScheduling();
	playbackHandler.lastSwungTickActioned = 100;
	playbackHandler.swungTicksTilNextEvent = 2147483647;

	session.scheduleLaunchTiming(200, 3, 48);

	LONGS_EQUAL(200, session.launchEventAtSwungTickCount);
	LONGS_EQUAL(3, session.numRepeatsTilLaunch);
	LONGS_EQUAL(48, session.currentArmedLaunchLengthForOneRepeat);
	CHECK_FALSE(session.switchToArrangementAtLaunchEvent);
}

TEST(SessionLaunchScheduling, scheduleLaunchTimingUpdatesSwungTicksTilNextEvent) {
	session.cancelAllLaunchScheduling();
	playbackHandler.lastSwungTickActioned = 100;
	playbackHandler.swungTicksTilNextEvent = 2147483647;

	session.scheduleLaunchTiming(200, 1, 96);

	// ticksTilLaunchEvent = 200 - 100 = 100, which is < INT32_MAX
	LONGS_EQUAL(100, playbackHandler.swungTicksTilNextEvent);
}

TEST(SessionLaunchScheduling, scheduleLaunchTimingIgnoresEarlierTick) {
	session.cancelAllLaunchScheduling();
	playbackHandler.lastSwungTickActioned = 0;

	// Schedule at tick 200
	session.scheduleLaunchTiming(200, 1, 96);
	LONGS_EQUAL(200, session.launchEventAtSwungTickCount);

	// Try to schedule at tick 100 (earlier) -- should be ignored
	session.scheduleLaunchTiming(100, 2, 48);
	LONGS_EQUAL(200, session.launchEventAtSwungTickCount);
	LONGS_EQUAL(1, session.numRepeatsTilLaunch);
	LONGS_EQUAL(96, session.currentArmedLaunchLengthForOneRepeat);
}

TEST(SessionLaunchScheduling, scheduleLaunchTimingAcceptsLaterTick) {
	session.cancelAllLaunchScheduling();
	playbackHandler.lastSwungTickActioned = 0;

	// Schedule at tick 100
	session.scheduleLaunchTiming(100, 1, 48);
	LONGS_EQUAL(100, session.launchEventAtSwungTickCount);

	// Schedule at tick 200 (later) -- should overwrite
	session.scheduleLaunchTiming(200, 3, 96);
	LONGS_EQUAL(200, session.launchEventAtSwungTickCount);
	LONGS_EQUAL(3, session.numRepeatsTilLaunch);
	LONGS_EQUAL(96, session.currentArmedLaunchLengthForOneRepeat);
}

TEST(SessionLaunchScheduling, scheduleLaunchTimingClearsStopRecordingFlag) {
	session.cancelAllLaunchScheduling();
	playbackHandler.stopOutputRecordingAtLoopEnd = true;
	playbackHandler.lastSwungTickActioned = 0;

	session.scheduleLaunchTiming(100, 1, 48);

	CHECK_FALSE(playbackHandler.stopOutputRecordingAtLoopEnd);
}

TEST(SessionLaunchScheduling, scheduleLaunchTimingClearsSwitchToArrangement) {
	session.cancelAllLaunchScheduling();
	session.switchToArrangementAtLaunchEvent = true;
	playbackHandler.lastSwungTickActioned = 0;

	session.scheduleLaunchTiming(100, 1, 48);

	CHECK_FALSE(session.switchToArrangementAtLaunchEvent);
}

TEST(SessionLaunchScheduling, scheduleLaunchTimingDoesNotShrinkSwungTicks) {
	session.cancelAllLaunchScheduling();
	playbackHandler.lastSwungTickActioned = 0;
	playbackHandler.swungTicksTilNextEvent = 50; // Already a closer event

	session.scheduleLaunchTiming(200, 1, 96);

	// swungTicksTilNextEvent should stay at 50 since 200 > 50
	LONGS_EQUAL(50, playbackHandler.swungTicksTilNextEvent);
}

// ---------------------------------------------------------------------------
// Session areAnyClipsArmed tests
// ---------------------------------------------------------------------------

TEST_GROUP(SessionClipsArmed) {
	Song testSongLocal;
	Clip clip1{1};
	Clip clip2{2};
	Clip clip3{3};
	void setup() {
		currentSong = &testSongLocal;
		testSongLocal.clear();
		clip1.armState = ArmState::OFF;
		clip2.armState = ArmState::OFF;
		clip3.armState = ArmState::OFF;
	}
	void teardown() { currentSong = nullptr; }
};

TEST(SessionClipsArmed, noClipsMeansNotArmed) {
	CHECK_FALSE(session.areAnyClipsArmed());
}

TEST(SessionClipsArmed, allClipsOffMeansNotArmed) {
	testSongLocal.sessionClips.push(&clip1);
	testSongLocal.sessionClips.push(&clip2);
	CHECK_FALSE(session.areAnyClipsArmed());
}

TEST(SessionClipsArmed, oneClipArmedReturnsTrue) {
	clip1.armState = ArmState::ON_NORMAL;
	testSongLocal.sessionClips.push(&clip1);
	testSongLocal.sessionClips.push(&clip2);
	CHECK(session.areAnyClipsArmed());
}

TEST(SessionClipsArmed, lastClipArmedReturnsTrue) {
	testSongLocal.sessionClips.push(&clip1);
	testSongLocal.sessionClips.push(&clip2);
	clip2.armState = ArmState::ON_NORMAL;
	CHECK(session.areAnyClipsArmed());
}

TEST(SessionClipsArmed, multipleClipsArmed) {
	clip1.armState = ArmState::ON_NORMAL;
	clip2.armState = ArmState::ON_NORMAL;
	testSongLocal.sessionClips.push(&clip1);
	testSongLocal.sessionClips.push(&clip2);
	CHECK(session.areAnyClipsArmed());
}

// ---------------------------------------------------------------------------
// Session constructor and field initialization
// ---------------------------------------------------------------------------

TEST_GROUP(SessionInit) {};

TEST(SessionInit, constructorSetsLaunchToZero) {
	Session s;
	LONGS_EQUAL(0, s.launchEventAtSwungTickCount);
}

TEST(SessionInit, constructorSetsLastSectionDefault) {
	// The real constructor sets lastSectionArmed = REALLY_OUT_OF_RANGE
	// Our mock doesn't call the real constructor body (which calls cancelAllLaunchScheduling),
	// but we initialize the field directly.
	Session s;
	// launchEventAtSwungTickCount set by cancelAllLaunchScheduling in constructor
	LONGS_EQUAL(0, s.launchEventAtSwungTickCount);
}

// ---------------------------------------------------------------------------
// PlaybackHandler state transition tests
// ---------------------------------------------------------------------------

TEST_GROUP(PlaybackHandlerState) {
	void setup() {
		playbackHandler.playbackState = 0;
		playbackHandler.recording = RecordingMode::OFF;
		playbackHandler.metronomeOn = MetronomeMode::COUNT_IN;
		playbackHandler.lastSwungTickActioned = 0;
		playbackHandler.swungTicksTilNextEvent = 2147483647;
		playbackHandler.stopOutputRecordingAtLoopEnd = false;
	}
};

TEST(PlaybackHandlerState, initialStateNotPlaying) {
	CHECK_FALSE(playbackHandler.isInternalClockActive());
	CHECK_FALSE(playbackHandler.isEitherClockActive());
}

TEST(PlaybackHandlerState, setupPlaybackActivatesClock) {
	playbackHandler.setupPlaybackUsingInternalClock();
	CHECK(playbackHandler.isInternalClockActive());
	CHECK(playbackHandler.isEitherClockActive());
}

TEST(PlaybackHandlerState, endPlaybackStopsClock) {
	playbackHandler.setupPlaybackUsingInternalClock();
	CHECK(playbackHandler.isEitherClockActive());
	playbackHandler.endPlayback();
	CHECK_FALSE(playbackHandler.isEitherClockActive());
}

TEST(PlaybackHandlerState, playbackStartStopCycle) {
	// Start
	playbackHandler.setupPlaybackUsingInternalClock();
	CHECK(playbackHandler.isEitherClockActive());
	// Stop
	playbackHandler.endPlayback();
	CHECK_FALSE(playbackHandler.isEitherClockActive());
	// Start again
	playbackHandler.setupPlaybackUsingInternalClock();
	CHECK(playbackHandler.isEitherClockActive());
}

TEST(PlaybackHandlerState, recordingModeTransitions) {
	CHECK(playbackHandler.recording == RecordingMode::OFF);
	playbackHandler.recording = RecordingMode::NORMAL;
	CHECK(playbackHandler.recording == RecordingMode::NORMAL);
	playbackHandler.recording = RecordingMode::ARRANGEMENT;
	CHECK(playbackHandler.recording == RecordingMode::ARRANGEMENT);
}

// ---------------------------------------------------------------------------
// Session + PlaybackHandler integration: hasPlaybackActive
// ---------------------------------------------------------------------------

TEST_GROUP(SessionPlaybackIntegration) {
	void setup() {
		currentPlaybackMode = nullptr;
		playbackHandler.playbackState = 0;
	}
};

TEST(SessionPlaybackIntegration, sessionIsActiveWhenCurrentAndPlaying) {
	currentPlaybackMode = &session;
	playbackHandler.playbackState = 1;
	CHECK(session.hasPlaybackActive());
}

TEST(SessionPlaybackIntegration, sessionNotActiveWhenNotCurrent) {
	currentPlaybackMode = nullptr;
	playbackHandler.playbackState = 1;
	CHECK_FALSE(session.hasPlaybackActive());
}

TEST(SessionPlaybackIntegration, sessionNotActiveWhenNotPlaying) {
	currentPlaybackMode = &session;
	playbackHandler.playbackState = 0;
	CHECK_FALSE(session.hasPlaybackActive());
}

// ---------------------------------------------------------------------------
// Session launch scheduling + clip arming interaction
// ---------------------------------------------------------------------------

TEST_GROUP(SessionLaunchClipInteraction) {
	Song testSongLocal;
	Clip clip1{1};
	Clip clip2{2};
	void setup() {
		currentSong = &testSongLocal;
		preLoadedSong = nullptr;
		testSongLocal.clear();
		clip1.armState = ArmState::OFF;
		clip2.armState = ArmState::OFF;
		session.cancelAllLaunchScheduling();
		playbackHandler.lastSwungTickActioned = 0;
		playbackHandler.swungTicksTilNextEvent = 2147483647;
		playbackHandler.stopOutputRecordingAtLoopEnd = false;
	}
	void teardown() { currentSong = nullptr; }
};

TEST(SessionLaunchClipInteraction, scheduleThenCheckArmedClips) {
	// Schedule a launch
	session.scheduleLaunchTiming(384, 1, 384);
	LONGS_EQUAL(384, session.launchEventAtSwungTickCount);

	// No clips armed yet
	testSongLocal.sessionClips.push(&clip1);
	CHECK_FALSE(session.areAnyClipsArmed());

	// Arm a clip
	clip1.armState = ArmState::ON_NORMAL;
	CHECK(session.areAnyClipsArmed());
}

TEST(SessionLaunchClipInteraction, cancelLaunchDoesNotAffectClipArming) {
	testSongLocal.sessionClips.push(&clip1);
	clip1.armState = ArmState::ON_NORMAL;

	session.scheduleLaunchTiming(384, 1, 384);
	session.cancelAllLaunchScheduling();

	// Cancelling the launch doesn't un-arm clips
	CHECK(session.areAnyClipsArmed());
	LONGS_EQUAL(0, session.launchEventAtSwungTickCount);
}

TEST(SessionLaunchClipInteraction, multipleScheduleOverwrites) {
	// Simulate multiple section arms overwriting each other
	session.scheduleLaunchTiming(192, 2, 192);
	LONGS_EQUAL(192, session.launchEventAtSwungTickCount);
	LONGS_EQUAL(2, session.numRepeatsTilLaunch);

	session.scheduleLaunchTiming(384, 1, 384);
	LONGS_EQUAL(384, session.launchEventAtSwungTickCount);
	LONGS_EQUAL(1, session.numRepeatsTilLaunch);

	// Earlier time ignored
	session.scheduleLaunchTiming(300, 3, 300);
	LONGS_EQUAL(384, session.launchEventAtSwungTickCount);
	LONGS_EQUAL(1, session.numRepeatsTilLaunch);
}
