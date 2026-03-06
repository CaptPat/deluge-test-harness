#include "CppUTest/TestHarness.h"
#include "dsp/delay/delay.h"
#include "dsp/stereo_sample.h"
#include <cstring>

// Delay::process uses spareRenderingBuffer as scratch space.
// It's defined as int32_t[2][128] in platform_stubs.cpp.
// A StereoSample is 8 bytes, so 128 int32_t = 64 StereoSample max.
static constexpr int kBufSize = 32;

TEST_GROUP(DelaySmokeTest) {
	Delay delay;
	StereoSample buf[kBufSize];

	void setup() override {
		memset(buf, 0, sizeof(buf));
	}

	void teardown() override {
		delay.discardBuffers();
	}

	void fillImpulse(int32_t amplitude = 1 << 24) {
		memset(buf, 0, sizeof(buf));
		buf[0].l = amplitude;
		buf[0].r = amplitude;
	}

	void fillDC(int32_t amplitude = 1 << 20) {
		for (auto& s : buf) {
			s.l = amplitude;
			s.r = amplitude;
		}
	}
};

// --- Lifecycle: init → process → discard → re-init ---

TEST(DelaySmokeTest, processWithActiveDelay) {
	delay.informWhetherActive(true, 44100);
	CHECK(delay.isActive());

	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 30; // high feedback
	delay.userRateLastTime = 44100;
	delay.countCyclesWithoutChange = 0;

	fillImpulse();
	delay.process({buf, kBufSize}, state);
	// Should not crash; output buffer should still have data
}

TEST(DelaySmokeTest, processMultipleBlocks) {
	delay.informWhetherActive(true, 44100);
	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 29;
	delay.userRateLastTime = 44100;
	delay.countCyclesWithoutChange = 0;

	// Process several blocks to exercise buffer wrap-around
	for (int block = 0; block < 20; block++) {
		fillDC(1 << 18);
		delay.process({buf, kBufSize}, state);
	}
	// If we got here, no crash on repeated processing
}

TEST(DelaySmokeTest, discardAndReinit) {
	delay.informWhetherActive(true, 44100);
	CHECK(delay.isActive());

	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 28;
	delay.userRateLastTime = 44100;

	fillImpulse();
	delay.process({buf, kBufSize}, state);

	delay.discardBuffers();
	CHECK_FALSE(delay.isActive());

	// Re-init at different rate
	delay.informWhetherActive(true, 22050);
	CHECK(delay.isActive());

	state.userDelayRate = 22050;
	delay.userRateLastTime = 22050;
	fillImpulse();
	delay.process({buf, kBufSize}, state);
}

// --- Parameter changes mid-stream ---

TEST(DelaySmokeTest, feedbackChangesMidStream) {
	delay.informWhetherActive(true, 44100);
	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 28;
	delay.userRateLastTime = 44100;

	fillDC();
	delay.process({buf, kBufSize}, state);

	// Crank feedback way up
	state.delayFeedbackAmount = INT32_MAX;
	fillDC();
	delay.process({buf, kBufSize}, state);

	// Drop feedback to zero
	state.delayFeedbackAmount = 0;
	fillDC();
	delay.process({buf, kBufSize}, state);
}

TEST(DelaySmokeTest, rateChangeTriggersResample) {
	delay.informWhetherActive(true, 44100);
	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 29;
	delay.userRateLastTime = 44100;
	delay.countCyclesWithoutChange = 0;

	fillDC();
	delay.process({buf, kBufSize}, state);

	// Change rate dramatically — should trigger resampling path
	state.userDelayRate = 11025;
	fillDC();
	delay.process({buf, kBufSize}, state);

	// Change rate again
	state.userDelayRate = 88200;
	fillDC();
	delay.process({buf, kBufSize}, state);
}

// --- Analog vs digital mode ---

TEST(DelaySmokeTest, analogModeProcess) {
	delay.analog = true;
	delay.informWhetherActive(true, 44100);
	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 29;
	state.analog_saturation = 8;
	delay.userRateLastTime = 44100;

	fillImpulse();
	delay.process({buf, kBufSize}, state);

	// Process again to exercise IR processor and tanh path
	fillDC();
	delay.process({buf, kBufSize}, state);
}

TEST(DelaySmokeTest, digitalModeProcess) {
	delay.analog = false;
	delay.informWhetherActive(true, 44100);
	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 29;
	delay.userRateLastTime = 44100;

	fillImpulse();
	delay.process({buf, kBufSize}, state);

	fillDC();
	delay.process({buf, kBufSize}, state);
}

// --- Ping-pong ---

TEST(DelaySmokeTest, pingPongStereoProcess) {
	// Use a very high rate so the delay buffer is small and wraps quickly
	delay.pingPong = true;
	delay.informWhetherActive(true, 0xFFFFFFFF);
	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 0xFFFFFFFF;
	state.delayFeedbackAmount = 1 << 30;
	delay.userRateLastTime = 0xFFFFFFFF;

	// Feed asymmetric signal for many blocks to fill buffer and wrap
	for (int block = 0; block < 100; block++) {
		for (auto& s : buf) {
			s.l = 1 << 22;
			s.r = 0;
		}
		delay.process({buf, kBufSize}, state);
	}

	// After ping-pong with left-only input, right channel should have signal
	bool rightHasSignal = false;
	for (auto& s : buf) {
		if (s.r != 0) {
			rightHasSignal = true;
			break;
		}
	}
	CHECK(rightHasSignal);
}

// --- Zero feedback should decay to silence ---

TEST(DelaySmokeTest, zeroFeedbackDecays) {
	delay.informWhetherActive(true, 44100);
	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 100; // nearly zero
	delay.userRateLastTime = 44100;

	delay.setTimeToAbandon(state);
	CHECK(delay.repeatsUntilAbandon <= 1);

	fillImpulse();
	delay.process({buf, kBufSize}, state);
}

// --- Edge: doDelay=false is a no-op ---

TEST(DelaySmokeTest, doDelayFalseIsNoop) {
	delay.informWhetherActive(true, 44100);
	Delay::State state{};
	state.doDelay = false;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 29;

	fillImpulse(1 << 24);
	StereoSample original = buf[0];
	delay.process({buf, kBufSize}, state);
	// Buffer should be untouched
	CHECK_EQUAL(original.l, buf[0].l);
	CHECK_EQUAL(original.r, buf[0].r);
}
