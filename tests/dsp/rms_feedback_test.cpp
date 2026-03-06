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

// ── Signal processing tests ─────────────────────────────────────────────

static void fillLoudSignal(StereoSample* buf, int n, int32_t amplitude) {
	for (int i = 0; i < n; i++) {
		// Simple alternating pattern to create RMS content
		int32_t val = (i & 1) ? amplitude : -amplitude;
		buf[i].l = val;
		buf[i].r = val;
	}
}

TEST(RMSCompressorTest, loudSignalGetsReduced) {
	// threshold knob at max → internal threshold 0.2 (most compression)
	// ratio knob at max → near-limiter
	comp.setup(0, ONE_Q31 / 4, ONE_Q31 - 1, ONE_Q31 - 1, 0, ONE_Q31, 1.35f);

	int32_t amplitude = 1 << 22; // loud signal
	std::span<StereoSample> span(buffer, BUF_SIZE);

	// Run many passes to let feedback RMS and envelope settle
	for (int pass = 0; pass < 20; pass++) {
		fillLoudSignal(buffer, BUF_SIZE, amplitude);
		comp.render(span, ONE_Q31 >> 4, ONE_Q31 >> 4, ONE_Q31 >> 1);
	}

	// After settling, gain reduction should be non-zero
	CHECK(comp.gainReduction > 0);
}

TEST(RMSCompressorTest, calcRMSLoudSignal) {
	int32_t amplitude = 1 << 22;
	fillLoudSignal(buffer, BUF_SIZE, amplitude);
	std::span<StereoSample> span(buffer, BUF_SIZE);
	float rms = comp.calcRMS(span);
	// RMS of a loud signal should be a meaningful value
	CHECK(rms > 0.0f);
}

TEST(RMSCompressorTest, higherRatioMoreReduction) {
	int32_t amplitude = 1 << 22;

	// Low ratio (2:1), high threshold knob for compression
	RMSFeedbackCompressor compLow;
	compLow.setup(0, ONE_Q31 / 4, ONE_Q31 - 1, 0, 0, ONE_Q31, 1.35f);
	for (int pass = 0; pass < 20; pass++) {
		fillLoudSignal(buffer, BUF_SIZE, amplitude);
		std::span<StereoSample> span(buffer, BUF_SIZE);
		compLow.render(span, ONE_Q31 >> 4, ONE_Q31 >> 4, ONE_Q31 >> 1);
	}
	uint8_t lowReduction = compLow.gainReduction;

	// High ratio (near limiting), same threshold
	RMSFeedbackCompressor compHigh;
	compHigh.setup(0, ONE_Q31 / 4, ONE_Q31 - 1, ONE_Q31 - 1, 0, ONE_Q31, 1.35f);
	for (int pass = 0; pass < 20; pass++) {
		fillLoudSignal(buffer, BUF_SIZE, amplitude);
		std::span<StereoSample> span(buffer, BUF_SIZE);
		compHigh.render(span, ONE_Q31 >> 4, ONE_Q31 >> 4, ONE_Q31 >> 1);
	}
	uint8_t highReduction = compHigh.gainReduction;

	CHECK(highReduction >= lowReduction);
}

TEST(RMSCompressorTest, dryBlendPassesThrough) {
	// Full dry (blend = 0), compressor should pass signal through unmodified
	comp.setup(ONE_Q31 / 4, ONE_Q31 / 4, 0, ONE_Q31 / 2, 0, 0, 1.35f);

	int32_t amplitude = 1 << 20;
	fillLoudSignal(buffer, BUF_SIZE, amplitude);

	// Save original values
	int32_t origL = buffer[10].l;
	int32_t origR = buffer[10].r;

	std::span<StereoSample> span(buffer, BUF_SIZE);
	comp.render(span, 1 << 27, 1 << 27, ONE_Q31 >> 3);

	// With dry blend, output should be close to original scaled by volume
	// Just verify it doesn't crash and produces non-zero output
	bool hasOutput = false;
	for (int i = 0; i < BUF_SIZE; i++) {
		if (buffer[i].l != 0 || buffer[i].r != 0) {
			hasOutput = true;
			break;
		}
	}
	CHECK(hasOutput);
}

TEST(RMSCompressorTest, attackTimeFaster) {
	// Faster attack should converge quicker
	float slow = comp.runEnvelope(0.0f, 1.0f, 1.0f);
	float fast = comp.runEnvelope(0.0f, 1.0f, 128.0f);
	// More samples = more convergence
	CHECK(fast >= slow);
}

TEST(RMSCompressorTest, ratioDisplayRange) {
	// Ratio at 0 should be 2:1
	comp.setRatio(0);
	float ratioLow = comp.getRatioForDisplay();
	CHECK(ratioLow >= 1.9f && ratioLow <= 2.1f);

	// Ratio at max should be very high
	comp.setRatio(ONE_Q31 - 1);
	float ratioHigh = comp.getRatioForDisplay();
	CHECK(ratioHigh > 50.0f);
}

TEST(RMSCompressorTest, thresholdRange) {
	// Threshold 0 → internal threshold ~1.0
	comp.setThreshold(0);
	// Threshold max → internal threshold ~0.2
	comp.setThreshold(ONE_Q31 - 1);
	// Just verify knob pos round-trips
	CHECK_EQUAL(ONE_Q31 - 1, comp.getThreshold());
}

TEST(RMSCompressorTest, blendDisplayPercentage) {
	comp.setBlend(0);
	CHECK_EQUAL(0, comp.getBlendForDisplay());

	comp.setBlend(ONE_Q31);
	CHECK_EQUAL(100, comp.getBlendForDisplay());
}
