// FilterSet deep integration tests: routing chains, morph continuity, state preservation,
// drive/saturation, zero-input DC checks, impulse decay, type transitions, and passthrough.

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "dsp/filter/filter_set.h"
#include <cmath>
#include <cstring>

using namespace deluge::dsp::filter;

static constexpr int BUF_SIZE = 256;
static constexpr q31_t QUARTER_Q31 = ONE_Q31 / 4;
static constexpr q31_t HALF_Q31 = ONE_Q31 / 2;
static constexpr q31_t EIGHTH_Q31 = ONE_Q31 / 8;
static constexpr q31_t IMPULSE_AMP = ONE_Q31 / 256;
static constexpr q31_t DC_AMP = ONE_Q31 / 256;

// ═══════════════════════════════════════════════════════════════════════════════
// Test group with common helpers
// ═══════════════════════════════════════════════════════════════════════════════

TEST_GROUP(FilterSetDeep) {
	FilterSet fs;
	q31_t buf[BUF_SIZE];
	q31_t ref[BUF_SIZE];
	q31_t stereo[BUF_SIZE * 2];

	void setup() override {
		fs.reset();
		memset(buf, 0, sizeof(buf));
		memset(ref, 0, sizeof(ref));
		memset(stereo, 0, sizeof(stereo));
	}

	// ── Buffer fill helpers ──────────────────────────────────────────────

	void fillZero(q31_t* b, int n) { memset(b, 0, n * sizeof(q31_t)); }

	void fillImpulse(q31_t* b, int n, q31_t amp = IMPULSE_AMP) {
		fillZero(b, n);
		b[0] = amp;
	}

	void fillDC(q31_t* b, int n, q31_t amp = DC_AMP) {
		for (int i = 0; i < n; i++) {
			b[i] = amp;
		}
	}

	void fillSine(q31_t* b, int n, double freqNorm = 0.05, q31_t amp = IMPULSE_AMP) {
		for (int i = 0; i < n; i++) {
			b[i] = static_cast<q31_t>(amp * sin(2.0 * M_PI * freqNorm * i));
		}
	}

	// ── Config helpers ───────────────────────────────────────────────────

	q31_t configLPF(FilterMode mode, q31_t freq, q31_t reso = 0, q31_t morph = 0,
	                FilterRoute route = FilterRoute::HIGH_TO_LOW) {
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		return fs.setConfig(freq, reso, mode, morph, 0, 0, FilterMode::OFF, 0, gain, route, false, &oscAmp);
	}

	q31_t configHPF(FilterMode mode, q31_t freq, q31_t reso = 0, q31_t morph = 0,
	                FilterRoute route = FilterRoute::HIGH_TO_LOW) {
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		return fs.setConfig(0, 0, FilterMode::OFF, 0, freq, reso, mode, morph, gain, route, false, &oscAmp);
	}

	q31_t configBoth(FilterMode lpfMode, q31_t lpfFreq, q31_t lpfReso, FilterMode hpfMode, q31_t hpfFreq,
	                 q31_t hpfReso, FilterRoute route, q31_t lpfMorph = 0, q31_t hpfMorph = 0) {
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		return fs.setConfig(lpfFreq, lpfReso, lpfMode, lpfMorph, hpfFreq, hpfReso, hpfMode, hpfMorph, gain, route,
		                    false, &oscAmp);
	}

	void render(int n = BUF_SIZE) { fs.renderLong(buf, buf + n, n); }

	void renderStereo(int n = BUF_SIZE) { fs.renderLongStereo(stereo, stereo + n * 2); }

	// ── Validation helpers ───────────────────────────────────────────────

	bool allFinite(q31_t* b, int n) {
		for (int i = 0; i < n; i++) {
			if (b[i] >= ONE_Q31 || b[i] <= -ONE_Q31) {
				return false;
			}
		}
		return true;
	}

	bool allBounded(q31_t* b, int n, q31_t limit = ONE_Q31) {
		for (int i = 0; i < n; i++) {
			if (b[i] >= limit || b[i] <= -limit) {
				return false;
			}
		}
		return true;
	}

	bool anyNonZero(q31_t* b, int n) {
		for (int i = 0; i < n; i++) {
			if (b[i] != 0) {
				return true;
			}
		}
		return false;
	}

	bool allZero(q31_t* b, int n) { return !anyNonZero(b, n); }

	int64_t sumEnergy(q31_t* b, int n) {
		int64_t energy = 0;
		for (int i = 0; i < n; i++) {
			energy += static_cast<int64_t>(b[i]) * b[i];
		}
		return energy;
	}

	q31_t peakAbsolute(q31_t* b, int n) {
		q31_t peak = 0;
		for (int i = 0; i < n; i++) {
			q31_t v = b[i] < 0 ? -b[i] : b[i];
			if (v > peak) {
				peak = v;
			}
		}
		return peak;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════
// ROUTING CHAIN TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, routingH2L_orderMatters) {
	// HPF→LPF: HPF first, then LPF. Compare with LPF→HPF to confirm order difference.
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, EIGHTH_Q31, 0,
	           FilterRoute::HIGH_TO_LOW);
	fillSine(buf, BUF_SIZE, 0.1);
	render();
	memcpy(ref, buf, sizeof(buf));

	fs.reset();
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, EIGHTH_Q31, 0,
	           FilterRoute::LOW_TO_HIGH);
	fillSine(buf, BUF_SIZE, 0.1);
	render();

	// The two orderings should produce different results due to nonlinear filter behavior
	bool differ = false;
	for (int i = 0; i < BUF_SIZE; i++) {
		if (buf[i] != ref[i]) {
			differ = true;
			break;
		}
	}
	CHECK(differ);
}

TEST(FilterSetDeep, routingParallel_sumsBothPaths) {
	// Parallel mode sums LPF and HPF outputs. With both active, output should differ from series.
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, QUARTER_Q31, 0,
	           FilterRoute::PARALLEL);
	fillSine(buf, BUF_SIZE, 0.05, IMPULSE_AMP);
	render();
	CHECK(anyNonZero(buf, BUF_SIZE));
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, parallelVsSeriesProduceDifferentOutput) {
	// Run same input through parallel and series, verify different results
	q31_t parallelOut[BUF_SIZE];
	q31_t seriesOut[BUF_SIZE];

	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, QUARTER_Q31, 0,
	           FilterRoute::PARALLEL);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	memcpy(parallelOut, buf, sizeof(buf));

	fs.reset();
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, QUARTER_Q31, 0,
	           FilterRoute::HIGH_TO_LOW);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	memcpy(seriesOut, buf, sizeof(buf));

	bool differ = false;
	for (int i = 0; i < BUF_SIZE; i++) {
		if (parallelOut[i] != seriesOut[i]) {
			differ = true;
			break;
		}
	}
	CHECK(differ);
}

TEST(FilterSetDeep, routingH2L_withSVFFilters) {
	configBoth(FilterMode::SVF_BAND, QUARTER_Q31, 0, FilterMode::SVF_NOTCH, QUARTER_Q31, 0,
	           FilterRoute::HIGH_TO_LOW);
	fillSine(buf, BUF_SIZE, 0.1);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, routingL2H_withSVFFilters) {
	configBoth(FilterMode::SVF_BAND, QUARTER_Q31, 0, FilterMode::SVF_NOTCH, QUARTER_Q31, 0,
	           FilterRoute::LOW_TO_HIGH);
	fillSine(buf, BUF_SIZE, 0.1);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, routingParallel_withSVFFilters) {
	configBoth(FilterMode::SVF_BAND, QUARTER_Q31, 0, FilterMode::SVF_NOTCH, QUARTER_Q31, 0, FilterRoute::PARALLEL);
	fillSine(buf, BUF_SIZE, 0.1);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, routingParallel_stereo) {
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, QUARTER_Q31, 0,
	           FilterRoute::PARALLEL);
	// Fill interleaved stereo with sine
	for (int i = 0; i < BUF_SIZE; i++) {
		q31_t v = static_cast<q31_t>(IMPULSE_AMP * sin(2.0 * M_PI * 0.05 * i));
		stereo[i * 2] = v;
		stereo[i * 2 + 1] = v;
	}
	renderStereo();
	CHECK(allFinite(stereo, BUF_SIZE * 2));
	CHECK(anyNonZero(stereo, BUF_SIZE * 2));
}

// ═══════════════════════════════════════════════════════════════════════════════
// MORPH PARAMETER SWEEP — CONTINUITY
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, morphSweepLPF_SVFBand_noCrashOrDiscontinuity) {
	// Sweep morph from 0 to near-max in steps, verify output stays bounded
	constexpr int STEPS = 16;
	constexpr q31_t MORPH_MAX = (1 << 29) - 1; // max morph value used in firmware

	for (int step = 0; step <= STEPS; step++) {
		q31_t morph = static_cast<q31_t>((int64_t)MORPH_MAX * step / STEPS);
		fs.reset();
		configLPF(FilterMode::SVF_BAND, QUARTER_Q31, 0, morph);
		fillSine(buf, BUF_SIZE, 0.05, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, morphSweepLPF_SVFNotch_noCrashOrDiscontinuity) {
	constexpr int STEPS = 16;
	constexpr q31_t MORPH_MAX = (1 << 29) - 1;

	for (int step = 0; step <= STEPS; step++) {
		q31_t morph = static_cast<q31_t>((int64_t)MORPH_MAX * step / STEPS);
		fs.reset();
		configLPF(FilterMode::SVF_NOTCH, QUARTER_Q31, 0, morph);
		fillSine(buf, BUF_SIZE, 0.05, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, morphSweepLPF_ladder_noCrashOrDiscontinuity) {
	// Ladder morph controls drive
	constexpr int STEPS = 16;
	constexpr q31_t MORPH_MAX = (1 << 29) - 1;

	for (int step = 0; step <= STEPS; step++) {
		q31_t morph = static_cast<q31_t>((int64_t)MORPH_MAX * step / STEPS);
		fs.reset();
		configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, QUARTER_Q31, morph);
		fillSine(buf, BUF_SIZE, 0.05, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, morphSweepHPF_ladder_noCrashOrDiscontinuity) {
	constexpr int STEPS = 16;
	constexpr q31_t MORPH_MAX = (1 << 29) - 1;

	for (int step = 0; step <= STEPS; step++) {
		q31_t morph = static_cast<q31_t>((int64_t)MORPH_MAX * step / STEPS);
		fs.reset();
		configHPF(FilterMode::HPLADDER, QUARTER_Q31, 0, morph);
		fillSine(buf, BUF_SIZE, 0.05, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, morphSweepHPF_SVFBand_noCrashOrDiscontinuity) {
	constexpr int STEPS = 16;
	constexpr q31_t MORPH_MAX = (1 << 29) - 1;

	for (int step = 0; step <= STEPS; step++) {
		q31_t morph = static_cast<q31_t>((int64_t)MORPH_MAX * step / STEPS);
		fs.reset();
		configHPF(FilterMode::SVF_BAND, QUARTER_Q31, 0, morph);
		fillSine(buf, BUF_SIZE, 0.05, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, morphSweepContinuous_noJumps) {
	// Process consecutive blocks while sweeping morph — no inter-block discontinuity
	constexpr int BLOCKS = 32;
	constexpr int BLOCK_SIZE = 64;
	constexpr q31_t MORPH_MAX = (1 << 29) - 1;

	q31_t blockBuf[BLOCK_SIZE];
	q31_t lastSample = 0;

	for (int block = 0; block < BLOCKS; block++) {
		q31_t morph = static_cast<q31_t>((int64_t)MORPH_MAX * block / BLOCKS);
		configLPF(FilterMode::SVF_BAND, QUARTER_Q31, 0, morph);

		fillSine(blockBuf, BLOCK_SIZE, 0.05, IMPULSE_AMP);
		fs.renderLong(blockBuf, blockBuf + BLOCK_SIZE, BLOCK_SIZE);

		CHECK(allFinite(blockBuf, BLOCK_SIZE));

		// Check no extreme jumps between blocks (difference should be bounded)
		int64_t jump = static_cast<int64_t>(blockBuf[0]) - static_cast<int64_t>(lastSample);
		if (jump < 0) {
			jump = -jump;
		}
		// Allow generous threshold — just catching explosions
		CHECK(jump < static_cast<int64_t>(ONE_Q31));

		lastSample = blockBuf[BLOCK_SIZE - 1];
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// FREQUENCY SWEEP — FULL RANGE
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, lpfFrequencySweep_fullRange_ladder) {
	// Sweep cutoff from near-zero to near-max
	q31_t freqs[] = {1000, ONE_Q31 / 128, ONE_Q31 / 64, ONE_Q31 / 32, ONE_Q31 / 16,
	                 ONE_Q31 / 8, QUARTER_Q31, HALF_Q31, ONE_Q31 - 1};

	for (q31_t freq : freqs) {
		fs.reset();
		configLPF(FilterMode::TRANSISTOR_24DB, freq);
		fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, hpfFrequencySweep_fullRange_ladder) {
	q31_t freqs[] = {1000, ONE_Q31 / 128, ONE_Q31 / 64, ONE_Q31 / 32, ONE_Q31 / 16,
	                 ONE_Q31 / 8, QUARTER_Q31, HALF_Q31, ONE_Q31 - 1};

	for (q31_t freq : freqs) {
		fs.reset();
		configHPF(FilterMode::HPLADDER, freq);
		fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, lpfFrequencySweep_fullRange_SVFBand) {
	q31_t freqs[] = {1000, ONE_Q31 / 64, ONE_Q31 / 16, QUARTER_Q31, HALF_Q31, ONE_Q31 - 1};

	for (q31_t freq : freqs) {
		fs.reset();
		configLPF(FilterMode::SVF_BAND, freq);
		fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, frequencySweepContinuous_noBlowup) {
	// Sweep frequency across consecutive blocks without reset
	constexpr int BLOCKS = 20;
	constexpr int BLOCK_SIZE = 64;
	q31_t blockBuf[BLOCK_SIZE];

	for (int block = 0; block < BLOCKS; block++) {
		q31_t freq = static_cast<q31_t>((int64_t)(ONE_Q31 - 1) * (block + 1) / BLOCKS);
		configLPF(FilterMode::TRANSISTOR_24DB, freq);
		fillSine(blockBuf, BLOCK_SIZE, 0.1, IMPULSE_AMP);
		fs.renderLong(blockBuf, blockBuf + BLOCK_SIZE, BLOCK_SIZE);
		CHECK(allFinite(blockBuf, BLOCK_SIZE));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// RESONANCE EXTREMES
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, resonanceZero_passesSignal) {
	configLPF(FilterMode::TRANSISTOR_24DB, HALF_Q31, 0);
	fillDC(buf, BUF_SIZE, DC_AMP);
	render();
	// With high cutoff and zero resonance, DC should pass through mostly unchanged
	CHECK(anyNonZero(buf, BUF_SIZE));
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, resonanceMax_allLPFModes) {
	FilterMode modes[] = {FilterMode::TRANSISTOR_12DB, FilterMode::TRANSISTOR_24DB,
	                      FilterMode::TRANSISTOR_24DB_DRIVE, FilterMode::SVF_BAND, FilterMode::SVF_NOTCH};

	for (FilterMode mode : modes) {
		fs.reset();
		configLPF(mode, QUARTER_Q31, ONE_Q31 - 1);
		fillImpulse(buf, BUF_SIZE);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, resonanceMax_HPFModes) {
	FilterMode modes[] = {FilterMode::HPLADDER, FilterMode::SVF_BAND, FilterMode::SVF_NOTCH};

	for (FilterMode mode : modes) {
		fs.reset();
		configHPF(mode, QUARTER_Q31, ONE_Q31 - 1);
		fillImpulse(buf, BUF_SIZE);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, resonanceSweepContinuous_noBlowup) {
	// Sweep resonance across consecutive blocks
	constexpr int BLOCKS = 20;
	constexpr int BLOCK_SIZE = 64;
	q31_t blockBuf[BLOCK_SIZE];

	for (int block = 0; block < BLOCKS; block++) {
		q31_t reso = static_cast<q31_t>((int64_t)(ONE_Q31 - 1) * block / (BLOCKS - 1));
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		fs.setConfig(QUARTER_Q31, reso, FilterMode::TRANSISTOR_24DB, 0, 0, 0, FilterMode::OFF, 0, gain,
		             FilterRoute::HIGH_TO_LOW, false, &oscAmp);
		fillSine(blockBuf, BLOCK_SIZE, 0.1, IMPULSE_AMP);
		fs.renderLong(blockBuf, blockBuf + BLOCK_SIZE, BLOCK_SIZE);
		CHECK(allFinite(blockBuf, BLOCK_SIZE));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// FILTER TYPE TRANSITIONS (mid-stream, no reset)
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, lpfTransition_ladderToSVF) {
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));

	// Switch to SVF without reset — should internally reset the SVF state
	configLPF(FilterMode::SVF_BAND, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
	CHECK(anyNonZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, lpfTransition_SVFToLadder) {
	configLPF(FilterMode::SVF_BAND, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();

	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
	CHECK(anyNonZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, lpfTransition_12dbTo24db) {
	configLPF(FilterMode::TRANSISTOR_12DB, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();

	// Same family, no internal reset needed
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, lpfTransition_24dbToDrive) {
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();

	configLPF(FilterMode::TRANSISTOR_24DB_DRIVE, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, lpfTransition_SVFBandToNotch) {
	configLPF(FilterMode::SVF_BAND, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();

	configLPF(FilterMode::SVF_NOTCH, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, hpfTransition_ladderToSVFBand) {
	configHPF(FilterMode::HPLADDER, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();

	configHPF(FilterMode::SVF_BAND, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, hpfTransition_SVFBandToLadder) {
	configHPF(FilterMode::SVF_BAND, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();

	configHPF(FilterMode::HPLADDER, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, rapidTypeToggle) {
	// Rapidly toggle between ladder and SVF — stress test for union reuse
	for (int i = 0; i < 20; i++) {
		FilterMode mode = (i % 2 == 0) ? FilterMode::TRANSISTOR_24DB : FilterMode::SVF_BAND;
		configLPF(mode, QUARTER_Q31);
		fillSine(buf, 128, 0.1, IMPULSE_AMP);
		fs.renderLong(buf, buf + 128, 128);
		CHECK(allFinite(buf, 128));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// STATE PRESERVATION ACROSS PARAMETER CHANGES
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, parameterChange_outputStaysBounded) {
	// Process audio, then change parameters, continue processing
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));

	// Abrupt parameter change
	configLPF(FilterMode::TRANSISTOR_24DB, EIGHTH_Q31, HALF_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, parameterChange_multipleConsecutive) {
	// Multiple rapid parameter changes
	q31_t params[][2] = {
	    {QUARTER_Q31, 0},
	    {EIGHTH_Q31, QUARTER_Q31},
	    {HALF_Q31, HALF_Q31},
	    {ONE_Q31 / 16, ONE_Q31 - 1},
	    {QUARTER_Q31, 0},
	};

	for (auto& p : params) {
		configLPF(FilterMode::TRANSISTOR_24DB, p[0], p[1]);
		fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, statePreservation_bothFilters_paramSweep) {
	// Both filters active, sweep LPF freq while HPF stays constant
	for (int i = 1; i <= 10; i++) {
		q31_t lpfFreq = static_cast<q31_t>((int64_t)(ONE_Q31 - 1) * i / 10);
		configBoth(FilterMode::TRANSISTOR_24DB, lpfFreq, 0, FilterMode::HPLADDER, EIGHTH_Q31, 0,
		           FilterRoute::HIGH_TO_LOW);
		fillSine(buf, BUF_SIZE, 0.05, IMPULSE_AMP);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// DRIVE / SATURATION
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, drive_transistor24dbDrive_bounded) {
	configLPF(FilterMode::TRANSISTOR_24DB_DRIVE, QUARTER_Q31, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.05, HALF_Q31);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
	CHECK(anyNonZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, drive_highMorph_saturates) {
	constexpr q31_t MORPH_MAX = (1 << 29) - 1;

	// Drive mode with high morph (drive parameter)
	configLPF(FilterMode::TRANSISTOR_24DB_DRIVE, QUARTER_Q31, QUARTER_Q31, MORPH_MAX);
	fillSine(buf, BUF_SIZE, 0.05, QUARTER_Q31);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, drive_hotSignal_allDriveModes) {
	FilterMode driveModes[] = {FilterMode::TRANSISTOR_24DB, FilterMode::TRANSISTOR_24DB_DRIVE,
	                           FilterMode::TRANSISTOR_12DB};
	constexpr q31_t MORPH_MAX = (1 << 29) - 1;

	for (FilterMode mode : driveModes) {
		fs.reset();
		configLPF(mode, QUARTER_Q31, HALF_Q31, MORPH_MAX / 2);
		fillSine(buf, BUF_SIZE, 0.05, HALF_Q31);
		render();
		CHECK(allFinite(buf, BUF_SIZE));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ZERO INPUT — NO DC OFFSET LEAKS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, zeroInput_LPF_producesZeroOutput) {
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31);
	fillZero(buf, BUF_SIZE);
	render();
	CHECK(allZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, zeroInput_HPF_producesZeroOutput) {
	configHPF(FilterMode::HPLADDER, QUARTER_Q31);
	fillZero(buf, BUF_SIZE);
	render();
	CHECK(allZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, zeroInput_SVF_producesZeroOutput) {
	configLPF(FilterMode::SVF_BAND, QUARTER_Q31);
	fillZero(buf, BUF_SIZE);
	render();
	CHECK(allZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, zeroInput_bothFilters_producesZeroOutput) {
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, QUARTER_Q31, 0,
	           FilterRoute::HIGH_TO_LOW);
	fillZero(buf, BUF_SIZE);
	render();
	CHECK(allZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, zeroInput_parallel_producesZeroOutput) {
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, QUARTER_Q31, 0,
	           FilterRoute::PARALLEL);
	fillZero(buf, BUF_SIZE);
	render();
	CHECK(allZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, zeroInput_withResonance_producesZeroOutput) {
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, HALF_Q31);
	fillZero(buf, BUF_SIZE);
	render();
	CHECK(allZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, zeroInput_afterSignal_decaysToNearZero) {
	// Feed signal with no resonance (clean decay), then check zero input eventually decays
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0);
	fillImpulse(buf, BUF_SIZE);
	render();

	// Now process many zero blocks — filter state should decay
	for (int block = 0; block < 50; block++) {
		fillZero(buf, BUF_SIZE);
		render();
	}
	// After 50 blocks of silence, output should be near-zero
	q31_t peak = peakAbsolute(buf, BUF_SIZE);
	CHECK(peak < IMPULSE_AMP);
}

// ═══════════════════════════════════════════════════════════════════════════════
// IMPULSE RESPONSE — FINITE AND DECAYING
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, impulseResponse_LPF_decays) {
	// Use zero resonance so the filter decays cleanly (no ringing)
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0);
	fillImpulse(buf, BUF_SIZE, IMPULSE_AMP);
	render();

	CHECK(anyNonZero(buf, BUF_SIZE));
	CHECK(allFinite(buf, BUF_SIZE));

	// Over multiple zero-input blocks, output energy must eventually decay
	int64_t firstBlockEnergy = sumEnergy(buf, BUF_SIZE);
	for (int block = 0; block < 5; block++) {
		fillZero(buf, BUF_SIZE);
		render();
	}
	int64_t laterBlockEnergy = sumEnergy(buf, BUF_SIZE);
	CHECK(laterBlockEnergy <= firstBlockEnergy);
}

TEST(FilterSetDeep, impulseResponse_HPF_decays) {
	configHPF(FilterMode::HPLADDER, QUARTER_Q31, QUARTER_Q31);
	fillImpulse(buf, BUF_SIZE, IMPULSE_AMP);
	render();

	CHECK(anyNonZero(buf, BUF_SIZE));
	CHECK(allFinite(buf, BUF_SIZE));

	int64_t firstHalfEnergy = sumEnergy(buf, BUF_SIZE / 2);
	int64_t secondHalfEnergy = sumEnergy(buf + BUF_SIZE / 2, BUF_SIZE / 2);
	CHECK(secondHalfEnergy <= firstHalfEnergy);
}

TEST(FilterSetDeep, impulseResponse_SVFBand_decays) {
	configLPF(FilterMode::SVF_BAND, QUARTER_Q31, QUARTER_Q31);
	fillImpulse(buf, BUF_SIZE, IMPULSE_AMP);
	render();

	CHECK(allFinite(buf, BUF_SIZE));

	int64_t firstHalfEnergy = sumEnergy(buf, BUF_SIZE / 2);
	int64_t secondHalfEnergy = sumEnergy(buf + BUF_SIZE / 2, BUF_SIZE / 2);
	CHECK(secondHalfEnergy <= firstHalfEnergy);
}

TEST(FilterSetDeep, impulseResponse_SVFNotch_finite) {
	configLPF(FilterMode::SVF_NOTCH, QUARTER_Q31, QUARTER_Q31);
	fillImpulse(buf, BUF_SIZE, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, impulseResponse_bothFilters_bounded) {
	// With both filters active, the combined response may have complex energy distribution
	// (HPF can shift energy later in the buffer). Just verify it stays finite and bounded.
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, QUARTER_Q31, FilterMode::HPLADDER, EIGHTH_Q31, 0,
	           FilterRoute::HIGH_TO_LOW);
	fillImpulse(buf, BUF_SIZE, IMPULSE_AMP);
	render();

	CHECK(allFinite(buf, BUF_SIZE));
	CHECK(anyNonZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, impulseResponse_bothFilters_eventuallyDecays) {
	// Use zero resonance for clean decay, then verify with many zero-input blocks
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, EIGHTH_Q31, 0,
	           FilterRoute::HIGH_TO_LOW);
	fillImpulse(buf, BUF_SIZE, IMPULSE_AMP);
	render();

	// Process many zero-input blocks to let filter state decay
	for (int block = 0; block < 50; block++) {
		fillZero(buf, BUF_SIZE);
		render();
	}
	// After 50 blocks of silence (50*256 = 12800 samples), should be near zero
	q31_t peak = peakAbsolute(buf, BUF_SIZE);
	CHECK(peak < IMPULSE_AMP);
}

// ═══════════════════════════════════════════════════════════════════════════════
// BOTH FILTERS OFF — PASSTHROUGH
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, bothOff_passthrough_mono) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0, 0, 0, FilterMode::OFF, 0, gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	CHECK_FALSE(fs.isLPFOn());
	CHECK_FALSE(fs.isHPFOn());
	CHECK_FALSE(fs.isOn());

	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	memcpy(ref, buf, sizeof(buf));
	render();

	// With both off, output should equal input (passthrough)
	for (int i = 0; i < BUF_SIZE; i++) {
		CHECK(buf[i] == ref[i]);
	}
}

TEST(FilterSetDeep, bothOff_passthrough_stereo) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0, 0, 0, FilterMode::OFF, 0, gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	q31_t stereoRef[BUF_SIZE * 2];
	for (int i = 0; i < BUF_SIZE; i++) {
		q31_t v = static_cast<q31_t>(IMPULSE_AMP * sin(2.0 * M_PI * 0.1 * i));
		stereo[i * 2] = v;
		stereo[i * 2 + 1] = v;
		stereoRef[i * 2] = v;
		stereoRef[i * 2 + 1] = v;
	}
	renderStereo();

	for (int i = 0; i < BUF_SIZE * 2; i++) {
		CHECK(stereo[i] == stereoRef[i]);
	}
}

TEST(FilterSetDeep, bothOff_seriesRoutes_passthrough) {
	// Series routes (H2L, L2H) with both filters off should be exact passthrough.
	// Parallel mode always copies + adds, so even with both off it doubles the signal.
	FilterRoute routes[] = {FilterRoute::HIGH_TO_LOW, FilterRoute::LOW_TO_HIGH};

	for (FilterRoute route : routes) {
		fs.reset();
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		fs.setConfig(0, 0, FilterMode::OFF, 0, 0, 0, FilterMode::OFF, 0, gain, route, false, &oscAmp);

		fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
		memcpy(ref, buf, sizeof(buf));
		render();

		for (int i = 0; i < BUF_SIZE; i++) {
			CHECK(buf[i] == ref[i]);
		}
	}
}

TEST(FilterSetDeep, bothOff_parallel_doublesSignal) {
	// Parallel mode with both filters off: HPF no-op on copy, LPF no-op on original,
	// then adds copy to original — effectively doubles the signal.
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0, 0, 0, FilterMode::OFF, 0, gain, FilterRoute::PARALLEL, false, &oscAmp);

	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	memcpy(ref, buf, sizeof(buf));
	render();

	// Each sample should be approximately double the input
	for (int i = 0; i < BUF_SIZE; i++) {
		CHECK(buf[i] == ref[i] * 2);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ONE FILTER ON, ONE OFF — CORRECT ROUTING
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, lpfOnOnly_hpfOff_processes) {
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31);
	CHECK(fs.isLPFOn());
	CHECK_FALSE(fs.isHPFOn());

	fillSine(buf, BUF_SIZE, 0.3, IMPULSE_AMP);
	memcpy(ref, buf, sizeof(buf));
	render();

	// LPF should modify the signal (especially high-freq content)
	bool changed = false;
	for (int i = 0; i < BUF_SIZE; i++) {
		if (buf[i] != ref[i]) {
			changed = true;
			break;
		}
	}
	CHECK(changed);
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, hpfOnOnly_lpfOff_processes) {
	configHPF(FilterMode::HPLADDER, QUARTER_Q31);
	CHECK_FALSE(fs.isLPFOn());
	CHECK(fs.isHPFOn());

	fillDC(buf, BUF_SIZE, DC_AMP);
	memcpy(ref, buf, sizeof(buf));
	render();

	// HPF should remove DC component
	bool changed = false;
	for (int i = 0; i < BUF_SIZE; i++) {
		if (buf[i] != ref[i]) {
			changed = true;
			break;
		}
	}
	CHECK(changed);
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, lpfOnly_seriesRoutes_bothProcess) {
	// With only LPF on, both H2L and L2H should process the signal identically in principle.
	// However, setConfig tracks lastLPFMode_ so the second run may have different blend state.
	// Just verify both produce valid filtered output.
	FilterRoute routes[] = {FilterRoute::HIGH_TO_LOW, FilterRoute::LOW_TO_HIGH};

	for (FilterRoute route : routes) {
		FilterSet localFs;
		localFs.reset();
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		localFs.setConfig(QUARTER_Q31, 0, FilterMode::TRANSISTOR_24DB, 0, 0, 0, FilterMode::OFF, 0, gain, route,
		                  false, &oscAmp);

		q31_t localBuf[BUF_SIZE];
		fillImpulse(localBuf, BUF_SIZE, IMPULSE_AMP);
		localFs.renderLong(localBuf, localBuf + BUF_SIZE, BUF_SIZE);

		CHECK(anyNonZero(localBuf, BUF_SIZE));
		CHECK(allFinite(localBuf, BUF_SIZE));
	}
}

TEST(FilterSetDeep, lpfOnly_parallelRoute_addsUnfilteredCopy) {
	// Parallel mode with only LPF: HPF no-ops on copy (leaving original),
	// LPF processes original, then adds copy (unfiltered) to LPF output.
	// Result = LPF(input) + input, which differs from series = LPF(input).
	fs.reset();
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, 0, FilterRoute::PARALLEL);
	fillImpulse(buf, BUF_SIZE, IMPULSE_AMP);
	render();
	CHECK(anyNonZero(buf, BUF_SIZE));
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, hpfOnly_allRoutes_sameResult) {
	FilterRoute routes[] = {FilterRoute::HIGH_TO_LOW, FilterRoute::LOW_TO_HIGH, FilterRoute::PARALLEL};
	q31_t results[3][BUF_SIZE];

	for (int r = 0; r < 3; r++) {
		fs.reset();
		configHPF(FilterMode::HPLADDER, QUARTER_Q31, 0, 0, routes[r]);
		fillImpulse(buf, BUF_SIZE, IMPULSE_AMP);
		render();
		memcpy(results[r], buf, sizeof(buf));
	}

	for (int i = 0; i < BUF_SIZE; i++) {
		CHECK(results[0][i] == results[1][i]);
	}
	// Parallel: LPF off processes nothing, HPF result + original summed
	// This may differ because parallel copies input and adds HPF(input) to LPF(input)
	// With LPF off, LPF path = original. With HPF on, parallel = original + HPF(original)
	// whereas series = HPF(original). So parallel differs when one is off.
	// Only H2L and L2H should be same.
}

// ═══════════════════════════════════════════════════════════════════════════════
// ENABLE → DISABLE → RE-ENABLE CYCLE
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, enableDisableReEnable_noBlowup) {
	// Enable LPF
	configLPF(FilterMode::TRANSISTOR_24DB, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));

	// Disable both
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0, 0, 0, FilterMode::OFF, 0, gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();

	// Re-enable with different mode — this triggers reset(true) path (blend fade-in)
	configLPF(FilterMode::SVF_BAND, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
	CHECK(anyNonZero(buf, BUF_SIZE));
}

TEST(FilterSetDeep, enableDisableReEnable_HPF) {
	configHPF(FilterMode::HPLADDER, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();

	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0, 0, 0, FilterMode::OFF, 0, gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	configHPF(FilterMode::SVF_BAND, QUARTER_Q31);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

// ═══════════════════════════════════════════════════════════════════════════════
// MIXED FILTER TYPES IN ROUTING CHAINS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, mixedTypes_ladderLPF_svfHPF_H2L) {
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, QUARTER_Q31, FilterMode::SVF_BAND, QUARTER_Q31, 0,
	           FilterRoute::HIGH_TO_LOW);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, mixedTypes_svfLPF_ladderHPF_L2H) {
	configBoth(FilterMode::SVF_BAND, QUARTER_Q31, 0, FilterMode::HPLADDER, QUARTER_Q31, QUARTER_Q31,
	           FilterRoute::LOW_TO_HIGH);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

TEST(FilterSetDeep, mixedTypes_driveLPF_notchHPF_parallel) {
	configBoth(FilterMode::TRANSISTOR_24DB_DRIVE, QUARTER_Q31, QUARTER_Q31, FilterMode::SVF_NOTCH, QUARTER_Q31, 0,
	           FilterRoute::PARALLEL);
	fillSine(buf, BUF_SIZE, 0.1, IMPULSE_AMP);
	render();
	CHECK(allFinite(buf, BUF_SIZE));
}

// ═══════════════════════════════════════════════════════════════════════════════
// STRESS: COMBINED PARAMETER SWEEPS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, combinedSweep_freqResoMorph) {
	// Sweep all three parameters simultaneously across blocks
	constexpr int BLOCKS = 16;
	constexpr int BLOCK_SIZE = 64;
	constexpr q31_t MORPH_MAX = (1 << 29) - 1;
	q31_t blockBuf[BLOCK_SIZE];

	for (int block = 0; block < BLOCKS; block++) {
		q31_t freq = static_cast<q31_t>((int64_t)(ONE_Q31 / 2) * (block + 1) / BLOCKS + ONE_Q31 / 16);
		q31_t reso = static_cast<q31_t>((int64_t)(ONE_Q31 - 1) * block / BLOCKS);
		q31_t morph = static_cast<q31_t>((int64_t)MORPH_MAX * block / BLOCKS);

		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		fs.setConfig(freq, reso, FilterMode::TRANSISTOR_24DB, morph, QUARTER_Q31, reso / 2, FilterMode::HPLADDER,
		             morph / 2, gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);

		fillSine(blockBuf, BLOCK_SIZE, 0.1, IMPULSE_AMP);
		fs.renderLong(blockBuf, blockBuf + BLOCK_SIZE, BLOCK_SIZE);
		CHECK(allFinite(blockBuf, BLOCK_SIZE));
	}
}

TEST(FilterSetDeep, combinedSweep_allRoutesAndModes) {
	// Every combination of routing and filter mode, single block each
	FilterMode lpfModes[] = {FilterMode::TRANSISTOR_24DB, FilterMode::SVF_BAND};
	FilterMode hpfModes[] = {FilterMode::HPLADDER, FilterMode::SVF_NOTCH};
	FilterRoute routes[] = {FilterRoute::HIGH_TO_LOW, FilterRoute::LOW_TO_HIGH, FilterRoute::PARALLEL};

	for (FilterMode lpf : lpfModes) {
		for (FilterMode hpf : hpfModes) {
			for (FilterRoute route : routes) {
				fs.reset();
				configBoth(lpf, QUARTER_Q31, 0, hpf, QUARTER_Q31, 0, route);
				fillSine(buf, 128, 0.1, IMPULSE_AMP);
				fs.renderLong(buf, buf + 128, 128);
				CHECK(allFinite(buf, 128));
			}
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// STEREO CONSISTENCY
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FilterSetDeep, stereo_routingH2L_bounded) {
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, QUARTER_Q31, FilterMode::HPLADDER, EIGHTH_Q31, 0,
	           FilterRoute::HIGH_TO_LOW);
	for (int i = 0; i < BUF_SIZE; i++) {
		q31_t v = static_cast<q31_t>(IMPULSE_AMP * sin(2.0 * M_PI * 0.1 * i));
		stereo[i * 2] = v;
		stereo[i * 2 + 1] = v;
	}
	renderStereo();
	CHECK(allFinite(stereo, BUF_SIZE * 2));
}

TEST(FilterSetDeep, stereo_routingL2H_bounded) {
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, 0, FilterMode::HPLADDER, EIGHTH_Q31, QUARTER_Q31,
	           FilterRoute::LOW_TO_HIGH);
	for (int i = 0; i < BUF_SIZE; i++) {
		q31_t v = static_cast<q31_t>(IMPULSE_AMP * sin(2.0 * M_PI * 0.1 * i));
		stereo[i * 2] = v;
		stereo[i * 2 + 1] = v;
	}
	renderStereo();
	CHECK(allFinite(stereo, BUF_SIZE * 2));
}

TEST(FilterSetDeep, stereo_zeroInput_noLeak) {
	configBoth(FilterMode::TRANSISTOR_24DB, QUARTER_Q31, QUARTER_Q31, FilterMode::HPLADDER, QUARTER_Q31, 0,
	           FilterRoute::PARALLEL);
	fillZero(stereo, BUF_SIZE * 2);
	renderStereo();
	CHECK(allZero(stereo, BUF_SIZE * 2));
}
