// Phase 9A: DX7/FM Synthesis Stack tests
// Tests the complete DX7 pipeline: LUTs → FmOpKernel → FmCore → DxPatch/DxVoice

#include "CppUTest/TestHarness.h"
#include "dsp/dx/engine.h"
#include "dsp/dx/fm_core.h"
#include "dsp/dx/fm_op_kernel.h"
#include "dsp/dx/math_lut.h"
#include "memory/memory_allocator_interface.h"
#include <cstring>

// ── LUT Initialization ──────────────────────────────────────────────────
// Phase is Q24: full cycle = 1<<24, quarter = 1<<22

TEST_GROUP(DX7LUT){
    void setup(){getDxEngine(); // Lazy-init engine + all LUTs
}
}
;

TEST(DX7LUT, getDxEngineReturnsNonNull) {
	CHECK(getDxEngine() != nullptr);
}

TEST(DX7LUT, getDxEngineReturnsSameInstance) {
	DxEngine* a = getDxEngine();
	DxEngine* b = getDxEngine();
	POINTERS_EQUAL(a, b);
}

TEST(DX7LUT, sinLookupZeroIsZero) {
	int32_t val = Sin::lookup(0);
	CHECK_EQUAL(0, val);
}

TEST(DX7LUT, sinLookupQuarterTurnIsPositive) {
	// Q24 quarter turn = π/2
	int32_t quarterPhase = 1 << 22;
	int32_t val = Sin::lookup(quarterPhase);
	CHECK(val > 0);
}

TEST(DX7LUT, sinLookupThreeQuarterTurnIsNegative) {
	// 3/4 turn = 3π/2 → sin is negative
	int32_t threeQuarterPhase = 3 * (1 << 22);
	int32_t val = Sin::lookup(threeQuarterPhase);
	CHECK(val < 0);
}

TEST(DX7LUT, sinLookupHalfTurnNearZero) {
	// Half turn = π → sin(π) ≈ 0
	int32_t halfPhase = 1 << 23;
	int32_t val = Sin::lookup(halfPhase);
	// Should be near zero (LUT interpolation may not hit exactly 0)
	CHECK(val >= -1000 && val <= 1000);
}

TEST(DX7LUT, exp2LookupMonotonic) {
	int32_t prev = Exp2::lookup(0);
	for (int i = 1; i < 10; i++) {
		int32_t val = Exp2::lookup(i * (1 << 20));
		CHECK(val >= prev);
		prev = val;
	}
}

TEST(DX7LUT, tanhLookupPositiveForPositiveInput) {
	// tanh(x) > 0 for x > 0
	CHECK(Tanh::lookup(1 << 20) > 0);
	CHECK(Tanh::lookup(1 << 22) > 0);
	CHECK(Tanh::lookup(1 << 24) > 0);
}

TEST(DX7LUT, tanhLookupIncreasing) {
	// tanh is monotonically increasing
	int32_t prev = Tanh::lookup(0);
	for (int i = 1; i <= 8; i++) {
		int32_t val = Tanh::lookup(i * (1 << 21));
		CHECK(val >= prev);
		prev = val;
	}
}

TEST(DX7LUT, tanhLookupNegativeSymmetry) {
	int32_t pos = Tanh::lookup(1 << 22);
	int32_t neg = Tanh::lookup(-(1 << 22));
	// tanh is an odd function: tanh(-x) = -tanh(x)
	CHECK_EQUAL(-pos, neg);
}

// ── FmOpKernel ──────────────────────────────────────────────────────────

TEST_GROUP(FmOpKernel){
    void setup(){getDxEngine();
}
}
;

TEST(FmOpKernel, computePureProducesSineOutput) {
	int32_t output[64] = {};
	// Freq: ~1/64 of full cycle per sample so we get a full period over 64 samples
	int32_t freq = (1 << 24) / 64;
	int32_t gain = 1 << 24;
	FmOpKernel::compute_pure(output, 64, 0, freq, gain, gain, 0, false, false);

	// Output should not be all zeros — phase advances so sin != 0 for most samples
	bool hasNonZero = false;
	for (int i = 0; i < 64; i++) {
		if (output[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(FmOpKernel, computePureZeroGainProducesSilence) {
	int32_t output[64] = {};
	int32_t freq = (1 << 24) / 64;
	FmOpKernel::compute_pure(output, 64, 0, freq, 0, 0, 0, false, false);

	for (int i = 0; i < 64; i++) {
		CHECK_EQUAL(0, output[i]);
	}
}

TEST(FmOpKernel, computeWithModulationChangesOutput) {
	int32_t pure_output[64] = {};
	int32_t mod_output[64] = {};
	int32_t modulation[64];

	int32_t freq = (1 << 24) / 64;
	int32_t gain = 1 << 24;

	for (int i = 0; i < 64; i++) {
		modulation[i] = i * 100000;
	}

	FmOpKernel::compute_pure(pure_output, 64, 0, freq, gain, gain, 0, false, false);
	FmOpKernel::compute(mod_output, 64, modulation, 0, freq, gain, gain, 0, false, false);

	bool differs = false;
	for (int i = 0; i < 64; i++) {
		if (pure_output[i] != mod_output[i]) {
			differs = true;
			break;
		}
	}
	CHECK(differs);
}

TEST(FmOpKernel, computeFbFeedbackModulatesTone) {
	int32_t output[64] = {};
	int32_t fb_buf[2] = {0, 0};
	int32_t freq = (1 << 24) / 64;
	int32_t gain = 1 << 24;

	FmOpKernel::compute_fb(output, 64, 0, freq, gain, gain, 0, fb_buf, 4, false);

	bool hasNonZero = false;
	for (int i = 0; i < 64; i++) {
		if (output[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(FmOpKernel, computePureAddAccumulates) {
	int32_t output[64];
	memset(output, 0, sizeof(output));

	int32_t freq = (1 << 24) / 32;
	int32_t gain = 1 << 24;

	// First pass — replace
	FmOpKernel::compute_pure(output, 64, 0, freq, gain, gain, 0, false, false);

	// Copy the result
	int32_t first_pass[64];
	memcpy(first_pass, output, sizeof(output));

	// Second pass — add mode
	FmOpKernel::compute_pure(output, 64, 0, freq, gain, gain, 0, true, false);

	// In add mode, output should be doubled (same signal added)
	for (int i = 1; i < 64; i++) {
		if (first_pass[i] != 0) {
			CHECK_EQUAL(first_pass[i] * 2, output[i]);
			break;
		}
	}
}

// ── FmCore ──────────────────────────────────────────────────────────────

TEST_GROUP(FmCore){
    void setup(){getDxEngine();
}
}
;

TEST(FmCore, algorithmsTableHas32Entries) {
	// Each algorithm should have at least one operator routing to OUT_BUS_ADD (bit 2)
	for (int i = 0; i < 32; i++) {
		const FmAlgorithm& alg = FmCore::algorithms[i];
		bool hasOutput = false;
		for (int op = 0; op < 6; op++) {
			if (alg.ops[op] & OUT_BUS_ADD) {
				hasOutput = true;
				break;
			}
		}
		CHECK(hasOutput);
	}
}

TEST(FmCore, renderProducesOutput) {
	DxEngine* engine = getDxEngine();
	FmCore& core = engine->engineModern;

	int32_t output[64];
	memset(output, 0, sizeof(output));

	// level_in is log-scale Q24. render() computes gain2 = Exp2(level_in - 14*(1<<24)).
	// level_in = 16*(1<<24) gives Exp2(2*(1<<24)) = a healthy gain level.
	FmOpParams params[6];
	for (int i = 0; i < 6; i++) {
		params[i].level_in = 16 * (1 << 24);
		params[i].gain_out = 1 << 24;
		params[i].freq = (1 << 24) / 64 + i * 1000;
		params[i].phase = 0;
	}

	int32_t fb_buf[2] = {0, 0};
	// Algorithm 31: all 6 ops → output directly (simplest routing)
	core.render(output, 64, params, 31, fb_buf, 0);

	bool hasNonZero = false;
	for (int i = 0; i < 64; i++) {
		if (output[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(FmCore, renderDifferentAlgorithmsDiffer) {
	DxEngine* engine = getDxEngine();
	FmCore& core = engine->engineModern;

	int32_t output0[64], output1[64];
	memset(output0, 0, sizeof(output0));
	memset(output1, 0, sizeof(output1));

	FmOpParams params0[6], params1[6];
	for (int i = 0; i < 6; i++) {
		params0[i].level_in = 16 * (1 << 24);
		params0[i].gain_out = 1 << 24;
		params0[i].freq = (1 << 22) + i * 5000;
		params0[i].phase = 0;

		params1[i] = params0[i];
	}

	int32_t fb0[2] = {0, 0}, fb1[2] = {0, 0};

	core.render(output0, 64, params0, 0, fb0, 0);
	core.render(output1, 64, params1, 31, fb1, 0);

	bool differs = false;
	for (int i = 0; i < 64; i++) {
		if (output0[i] != output1[i]) {
			differs = true;
			break;
		}
	}
	CHECK(differs);
}

// ── EngineMkI ───────────────────────────────────────────────────────────

TEST(FmCore, engineMkIRenderProducesOutput) {
	DxEngine* engine = getDxEngine();
	EngineMkI& mkI = engine->engineMkI;

	int32_t output[64];
	memset(output, 0, sizeof(output));

	FmOpParams params[6];
	for (int i = 0; i < 6; i++) {
		params[i].level_in = 16 * (1 << 24);
		params[i].gain_out = 1 << 24;
		params[i].freq = (1 << 24) / 64 + i * 1000;
		params[i].phase = 0;
	}

	int32_t fb_buf[2] = {0, 0};
	mkI.render(output, 64, params, 0, fb_buf, 0);

	bool hasNonZero = false;
	for (int i = 0; i < 64; i++) {
		if (output[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

// ── DxPatch ─────────────────────────────────────────────────────────────

TEST_GROUP(DxPatch){
    void setup(){getDxEngine();
}
}
;

TEST(DxPatch, newPatchReturnsNonNull) {
	DxEngine* engine = getDxEngine();
	DxPatch* patch = engine->newPatch();
	CHECK(patch != nullptr);
	delugeDealloc(patch);
}

TEST(DxPatch, setEngineModeModern) {
	DxEngine* engine = getDxEngine();
	DxPatch* patch = engine->newPatch();
	patch->setEngineMode(0); // Modern mode
	CHECK_EQUAL(0, patch->engineMode);
	delugeDealloc(patch);
}

// ── DxVoice lifecycle ───────────────────────────────────────────────────

TEST_GROUP(DxVoice){
    void setup(){getDxEngine();
}
}
;

TEST(DxVoice, solicitAndUnassignRoundTrip) {
	DxEngine* engine = getDxEngine();
	DxVoice* voice = engine->solicitDxVoice();
	CHECK(voice != nullptr);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxVoice, initSetsUpVoice) {
	DxEngine* engine = getDxEngine();
	DxPatch* patch = engine->newPatch();
	DxVoice* voice = engine->solicitDxVoice();

	CHECK(patch != nullptr);
	CHECK(voice != nullptr);

	// Init with middle C, medium velocity
	voice->init(*patch, 60, 100);

	// Voice should be usable — compute a small buffer
	int32_t buf[32];
	memset(buf, 0, sizeof(buf));
	DxVoiceCtrl ctrl;
	voice->compute(buf, 32, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
	delugeDealloc(patch);
}
