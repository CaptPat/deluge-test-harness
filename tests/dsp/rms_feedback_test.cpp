#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "dsp/compressor/rms_feedback.h"
#include <cstring>
#include <span>

static constexpr int BUF_SIZE = 128;

TEST_GROUP(RMSCompressorTest) {
	RMSFeedbackCompressor comp;
	StereoSample buffer[BUF_SIZE];

	void setup() override {
		comp.reset();
		memset(buffer, 0, sizeof(buffer));
	}
};

TEST(RMSCompressorTest, constructorDefaults) {
	RMSFeedbackCompressor fresh;
	// Default attack should be > 0 ms
	CHECK(fresh.getAttackMS() > 0.0f);
	// Default release should be > 0 ms
	CHECK(fresh.getReleaseMS() > 0.0f);
	// Default threshold is 0
	CHECK_EQUAL(0, fresh.getThreshold());
}

TEST(RMSCompressorTest, setAttackReturnsMS) {
	int32_t ms = comp.setAttack(ONE_Q31 / 2);
	CHECK(ms > 0);
}

TEST(RMSCompressorTest, setReleaseReturnsMS) {
	int32_t ms = comp.setRelease(ONE_Q31 / 2);
	CHECK(ms > 0);
}

TEST(RMSCompressorTest, setThresholdStores) {
	comp.setThreshold(ONE_Q31 / 4);
	CHECK_EQUAL(ONE_Q31 / 4, comp.getThreshold());
}

TEST(RMSCompressorTest, setRatioStores) {
	comp.setRatio(ONE_Q31 / 2);
	CHECK_EQUAL(ONE_Q31 / 2, comp.getRatio());
}

TEST(RMSCompressorTest, setSidechainStores) {
	comp.setSidechain(ONE_Q31 / 4);
	CHECK_EQUAL(ONE_Q31 / 4, comp.getSidechain());
}

TEST(RMSCompressorTest, setBlendStores) {
	comp.setBlend(ONE_Q31);
	CHECK_EQUAL(ONE_Q31, comp.getBlend());
}

TEST(RMSCompressorTest, resetClearsState) {
	// Process some audio
	buffer[0].l = ONE_Q31 / 256;
	buffer[0].r = ONE_Q31 / 256;
	std::span<StereoSample> span(buffer, BUF_SIZE);
	comp.render(span, 1 << 27, 1 << 27, ONE_Q31 >> 3);

	// Reset
	comp.reset();
	// After reset, gainReduction should be 0
	CHECK_EQUAL(0, comp.gainReduction);
}

TEST(RMSCompressorTest, renderSilence) {
	std::span<StereoSample> span(buffer, BUF_SIZE);
	comp.render(span, 1 << 27, 1 << 27, ONE_Q31 >> 3);

	// With silence, output should remain near zero
	bool allZeroish = true;
	for (int i = 0; i < BUF_SIZE; i++) {
		if (std::abs(buffer[i].l) > 1000 || std::abs(buffer[i].r) > 1000) {
			allZeroish = false;
			break;
		}
	}
	CHECK(allZeroish);
}

TEST(RMSCompressorTest, calcRMSSilence) {
	std::span<StereoSample> span(buffer, BUF_SIZE);
	float rms = comp.calcRMS(span);
	// RMS of silence should be very low (log of near-zero)
	CHECK(rms < 5.0f);
}

TEST(RMSCompressorTest, renderVolNeutral) {
	buffer[0].l = ONE_Q31 / 256;
	buffer[0].r = ONE_Q31 / 256;
	std::span<StereoSample> span(buffer, BUF_SIZE);
	// Just verify it doesn't crash
	comp.renderVolNeutral(span, ONE_Q31 >> 3);
}

TEST(RMSCompressorTest, setupConfigures) {
	comp.setup(5 << 24, 5 << 24, 0, 64 << 24, 0, ONE_Q31, 1.35f);
	CHECK(comp.getAttackMS() > 0.0f);
	CHECK(comp.getReleaseMS() > 0.0f);
	CHECK_EQUAL(ONE_Q31, comp.getBlend());
}

TEST(RMSCompressorTest, runEnvelopeAttack) {
	float result = comp.runEnvelope(0.0f, 1.0f, 128.0f);
	// Attack: moving from 0 toward 1
	CHECK(result > 0.0f);
	CHECK(result <= 1.0f);
}

TEST(RMSCompressorTest, runEnvelopeRelease) {
	float result = comp.runEnvelope(1.0f, 0.0f, 128.0f);
	// Release: moving from 1 toward 0
	CHECK(result >= 0.0f);
	CHECK(result < 1.0f);
}
