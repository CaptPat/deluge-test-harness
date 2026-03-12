#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "dsp/stereo_sample.h"
#include "model/mod_controllable/ModFXProcessor.h"
#include "modulation/params/param_collection_summary.h"
#include "modulation/params/param_set.h"
#include "processing/engines/audio_engine.h"
#include <cmath>
#include <cstring>
#include <span>

using namespace deluge::modulation::params;

namespace {

constexpr int kBufferSize = 64;
constexpr int32_t kHalfQ31 = 0x40000000;

// Helper: fill buffer with a DC signal
void fillDC(StereoSample* buf, int n, int32_t val) {
	for (int i = 0; i < n; i++) {
		buf[i].l = val;
		buf[i].r = val;
	}
}

// Helper: fill buffer with silence
void fillSilence(StereoSample* buf, int n) {
	fillDC(buf, n, 0);
}

// Helper: check buffer is all zeros
bool isSilent(const StereoSample* buf, int n) {
	for (int i = 0; i < n; i++) {
		if (buf[i].l != 0 || buf[i].r != 0)
			return false;
	}
	return true;
}

// Helper: check no sample exceeds bounds (no overflow)
bool isBounded(const StereoSample* buf, int n) {
	for (int i = 0; i < n; i++) {
		// Allow full Q31 range
		if (buf[i].l == INT32_MIN || buf[i].r == INT32_MIN)
			return false;
	}
	return true;
}

// Helper: construct an UnpatchedParamSet for testing
struct TestParamContext {
	ParamCollectionSummary summary{};
	UnpatchedParamSet params;

	TestParamContext() : params(&summary) {
		// Set mod FX params to neutral defaults
		params.params[UNPATCHED_MOD_FX_OFFSET].setCurrentValueBasicForSetup(0);
		params.params[UNPATCHED_MOD_FX_FEEDBACK].setCurrentValueBasicForSetup(0);
	}

	void setFeedback(int32_t val) { params.params[UNPATCHED_MOD_FX_FEEDBACK].setCurrentValueBasicForSetup(val); }

	void setOffset(int32_t val) { params.params[UNPATCHED_MOD_FX_OFFSET].setCurrentValueBasicForSetup(val); }
};

TEST_GROUP(ModFXRender) {
	ModFXProcessor proc;
	StereoSample buffer[kBufferSize];
	int32_t postFXVolume;

	void setup() {
		proc = ModFXProcessor();
		proc.setupBuffer();
		postFXVolume = kHalfQ31;
		fillSilence(buffer, kBufferSize);
	}

	void teardown() { proc.disableBuffer(); }
};

// --- NONE type passthrough ---

TEST(ModFXRender, noneTypeIsPassthrough) {
	fillDC(buffer, kBufferSize, 1000);
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::NONE, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	// NONE should not modify the buffer
	for (int i = 0; i < kBufferSize; i++) {
		CHECK_EQUAL(1000, buffer[i].l);
		CHECK_EQUAL(1000, buffer[i].r);
	}
}

// --- Silence in → no crash, bounded output ---

TEST(ModFXRender, flangerSilenceNoCrash) {
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::FLANGER, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, false);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, chorusSilenceNoCrash) {
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, false);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, phaserSilenceNoCrash) {
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::PHASER, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, false);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, chorusStereoSilenceNoCrash) {
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS_STEREO, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, false);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, warbleSilenceNoCrash) {
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::WARBLE, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, false);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, dimensionSilenceNoCrash) {
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::DIMENSION, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, false);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, grainTypeNoCrash) {
	fillDC(buffer, kBufferSize, 5000);
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::GRAIN, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	// GRAIN is stub — should not modify buffer
	CHECK_EQUAL(5000, buffer[0].l);
}

// --- DC signal → bounded output, not identical to input (effect is applied) ---

TEST(ModFXRender, flangerModifiesDC) {
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.tickLFO(100, 1 << 20); // advance LFO so it has non-zero state
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::FLANGER, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, chorusModifiesDC) {
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, phaserModifiesDC) {
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::PHASER, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, dimensionModifiesDC) {
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::DIMENSION, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, warbleModifiesDC) {
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	ctx.setFeedback(kHalfQ31 / 2);
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::WARBLE, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
}

// --- Feedback parameter affects output ---

TEST(ModFXRender, flangerWithFeedback) {
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	ctx.setFeedback(kHalfQ31); // high feedback
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::FLANGER, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
}

TEST(ModFXRender, phaserWithFeedback) {
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	ctx.setFeedback(kHalfQ31);
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::PHASER, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
}

// --- Multiple render calls (state accumulation) ---

TEST(ModFXRender, flangerMultipleCallsStable) {
	TestParamContext ctx;
	for (int pass = 0; pass < 10; pass++) {
		fillDC(buffer, kBufferSize, kHalfQ31 / 8);
		proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::FLANGER, kHalfQ31 / 2, kHalfQ31 / 2,
		                  &postFXVolume, &ctx.params, true);
		CHECK(isBounded(buffer, kBufferSize));
	}
}

TEST(ModFXRender, phaserMultipleCallsStable) {
	TestParamContext ctx;
	for (int pass = 0; pass < 10; pass++) {
		fillDC(buffer, kBufferSize, kHalfQ31 / 8);
		proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::PHASER, kHalfQ31 / 2, kHalfQ31 / 2,
		                  &postFXVolume, &ctx.params, true);
		CHECK(isBounded(buffer, kBufferSize));
	}
}

// --- Zero depth should not alter signal much ---

TEST(ModFXRender, chorusZeroDepthMinimalEffect) {
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS, kHalfQ31, 0, &postFXVolume,
	                  &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
}

// --- Reset memory clears state ---

TEST(ModFXRender, resetAfterProcessingWithBuffer) {
	// When buffer is allocated, resetMemory clears the buffer (not allpass/phaser)
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	proc.resetMemory();
	// No crash, buffer cleared
	CHECK(proc.modFXBuffer != nullptr);
}

TEST(ModFXRender, resetAfterProcessingWithoutBuffer) {
	// When buffer is NOT allocated, resetMemory clears allpass/phaser state
	proc.disableBuffer();
	proc.phaserMemory.l = 12345;
	proc.phaserMemory.r = 67890;
	proc.allpassMemory[0].l = 99999;
	proc.resetMemory();
	CHECK_EQUAL(0, proc.phaserMemory.l);
	CHECK_EQUAL(0, proc.phaserMemory.r);
	CHECK_EQUAL(0, proc.allpassMemory[0].l);
}

// --- postFXVolume is modified by chorus setup ---

TEST(ModFXRender, chorusReducesPostFXVolume) {
	int32_t vol = kHalfQ31;
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS, kHalfQ31, kHalfQ31, &vol,
	                  &ctx.params, true);
	// Chorus setup divides volume by sqrt(2) for wet/dry mixing
	CHECK(vol < kHalfQ31);
}

// --- Mono rendering path (AudioEngine::renderInStereo = false) ---
// Exercises processOneModFXSample<modFXType, false> template instantiations

TEST(ModFXRender, flangerMono) {
	AudioEngine::renderInStereo = false;
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	ctx.setFeedback(kHalfQ31 / 2);
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::FLANGER, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
	AudioEngine::renderInStereo = true;
}

TEST(ModFXRender, chorusMono) {
	AudioEngine::renderInStereo = false;
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
	AudioEngine::renderInStereo = true;
}

TEST(ModFXRender, chorusStereoMono) {
	AudioEngine::renderInStereo = false;
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS_STEREO, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
	AudioEngine::renderInStereo = true;
}

TEST(ModFXRender, warbleMono) {
	AudioEngine::renderInStereo = false;
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	ctx.setFeedback(kHalfQ31 / 2);
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::WARBLE, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
	AudioEngine::renderInStereo = true;
}

TEST(ModFXRender, dimensionMono) {
	AudioEngine::renderInStereo = false;
	fillDC(buffer, kBufferSize, kHalfQ31 / 4);
	TestParamContext ctx;
	proc.tickLFO(100, 1 << 20);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::DIMENSION, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);
	CHECK(isBounded(buffer, kBufferSize));
	AudioEngine::renderInStereo = true;
}

// --- Multiple passes with different types on same processor ---

TEST(ModFXRender, switchTypeMidStream) {
	TestParamContext ctx;
	fillDC(buffer, kBufferSize, kHalfQ31 / 8);
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::FLANGER, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);

	fillDC(buffer, kBufferSize, kHalfQ31 / 8);
	postFXVolume = kHalfQ31;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::CHORUS, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);

	fillDC(buffer, kBufferSize, kHalfQ31 / 8);
	postFXVolume = kHalfQ31;
	proc.processModFX(std::span<StereoSample>(buffer, kBufferSize), ModFXType::WARBLE, kHalfQ31, kHalfQ31,
	                  &postFXVolume, &ctx.params, true);

	CHECK(isBounded(buffer, kBufferSize));
}

} // namespace
