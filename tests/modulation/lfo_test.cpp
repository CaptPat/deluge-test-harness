// LFO regression tests — ported from firmware/tests/unit/lfo_tests.cpp
// Verifies deterministic waveform generation using seeded PRNG.

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "modulation/lfo.h"
#include "util/waves.h"

TEST_GROUP(LFOTest) {
	void setup() {
		// Set CONG for all tests so they're deterministic
		CONG = 13287131;
	}
};

TEST(LFOTest, renderGlobalTriangle) {
	LFO lfo;
	LFOConfig conf(LFOType::TRIANGLE);
	lfo.setGlobalInitialPhase(conf);
	int32_t numSamples = 10, phaseIncrement = 100;
	CHECK_EQUAL(0, lfo.render(10, conf, 100));
	CHECK_EQUAL(numSamples * phaseIncrement * 2, lfo.render(0, conf, 0));
}

TEST(LFOTest, renderLocalTriangle) {
	LFO lfo;
	LFOConfig conf(LFOType::TRIANGLE);
	lfo.setLocalInitialPhase(conf);
	CHECK_EQUAL(INT32_MIN, lfo.render(10, conf, 100));
	CHECK_EQUAL(INT32_MIN + 2000, lfo.render(0, conf, 0));
}

TEST(LFOTest, renderGlobalSine) {
	LFO lfo;
	LFOConfig conf(LFOType::SINE);
	lfo.setGlobalInitialPhase(conf);
	CHECK_EQUAL(0, lfo.phase);
	lfo.phase = 1024;
	CHECK_EQUAL(3216, lfo.render(0, conf, 0));
}

TEST(LFOTest, renderLocalSine) {
	LFO lfo;
	LFOConfig conf(LFOType::SINE);
	lfo.setLocalInitialPhase(conf);
	CHECK_EQUAL(3221225472u, lfo.phase);
	CHECK_EQUAL(-2147418112, lfo.render(10, conf, 100));
	CHECK_EQUAL(-2147418082, lfo.render(0, conf, 0));
}

TEST(LFOTest, renderGSaw) {
	LFO lfo;
	LFOConfig conf(LFOType::SAW);
	lfo.setLocalInitialPhase(conf);
	uint32_t localPhase = lfo.phase;
	lfo.setGlobalInitialPhase(conf);
	CHECK_EQUAL(localPhase, lfo.phase);
	CHECK_EQUAL(INT32_MIN, lfo.render(10, conf, 100));
	CHECK_EQUAL(INT32_MIN + 1000, lfo.render(0, conf, 0));
}

TEST(LFOTest, renderSquare) {
	LFO lfo;
	LFOConfig conf(LFOType::SQUARE);
	lfo.setLocalInitialPhase(conf);
	uint32_t localPhase = lfo.phase;
	lfo.setGlobalInitialPhase(conf);
	CHECK_EQUAL(localPhase, lfo.phase);
	CHECK_EQUAL(INT32_MAX, lfo.render(0, conf, 0));
	lfo.phase = 0x80000001;
	CHECK_EQUAL(INT32_MIN, lfo.render(0, conf, 0));
}

TEST(LFOTest, renderSampleAndHold) {
	LFO lfo;
	LFOConfig conf(LFOType::SAMPLE_AND_HOLD);
	lfo.setLocalInitialPhase(conf);
	uint32_t localPhase = lfo.phase;
	conf.syncLevel = SYNC_LEVEL_8TH;
	lfo.setGlobalInitialPhase(conf);
	CHECK_EQUAL(localPhase, lfo.phase);
	CHECK_EQUAL(0u, lfo.phase);

	int32_t value = lfo.render(10, conf, 10);
	CHECK_EQUAL(-1392915738, value);
	CHECK_EQUAL(100u, lfo.phase);

	value = lfo.render(10, conf, 10);
	CHECK_EQUAL(-1392915738, value);
	CHECK_EQUAL(200u, lfo.phase);

	lfo.phase = 0;
	value = lfo.render(10, conf, 10);
	CHECK_EQUAL(-28442955, value);
	CHECK_EQUAL(100u, lfo.phase);

	lfo.phase = UINT32_MAX;
	value = lfo.render(10, conf, 10);
	CHECK_EQUAL(-1725170056, value);
	CHECK_EQUAL(99u, lfo.phase);
}

TEST(LFOTest, renderRandomWalk) {
	LFO lfo;
	LFOConfig conf(LFOType::RANDOM_WALK);
	lfo.setLocalInitialPhase(conf);
	uint32_t localPhase = lfo.phase;
	conf.syncLevel = SYNC_LEVEL_8TH;
	lfo.setGlobalInitialPhase(conf);
	CHECK_EQUAL(localPhase, lfo.phase);
	CHECK_EQUAL(0u, lfo.phase);

	int32_t value = lfo.render(10, conf, 10);
	CHECK_EQUAL(-2948644, value);
	CHECK_EQUAL(100u, lfo.phase);

	value = lfo.render(10, conf, 10);
	CHECK_EQUAL(-2948644, value);
	CHECK_EQUAL(200u, lfo.phase);

	lfo.phase = 0;
	value = lfo.render(10, conf, 10);
	CHECK_EQUAL(-78931243, value);
	CHECK_EQUAL(100u, lfo.phase);

	lfo.phase = UINT32_MAX;
	value = lfo.render(10, conf, 10);
	CHECK_EQUAL(-174189095, value);
	CHECK_EQUAL(99u, lfo.phase);
}

// ── Wave utilities ──────────────────────────────────────────────────────

TEST_GROUP(WaveTest){};

TEST(WaveTest, triangle) {
	CHECK_EQUAL(-2147483647, getTriangle(UINT32_MAX));
	CHECK_EQUAL(-2147483648, getTriangle(0));
	CHECK_EQUAL(-2147483646, getTriangle(1));
	CHECK_EQUAL(-2, getTriangle(1073741823));
	CHECK_EQUAL(0, getTriangle(1073741824));
	CHECK_EQUAL(2, getTriangle(1073741825));
	CHECK_EQUAL(2147483646, getTriangle(2147483647u));
	CHECK_EQUAL(2147483647, getTriangle(2147483648u));
	CHECK_EQUAL(2147483645, getTriangle(2147483649u));
	CHECK_EQUAL(1, getTriangle(3221225471u));
	CHECK_EQUAL(-1, getTriangle(3221225472u));
}
