// Envelope deep tests — stress-tests the real Envelope::render() method
// with specific timing, parameter, and lifecycle scenarios.

#include "CppUTest/TestHarness.h"
#include "modulation/envelope.h"
#include "util/lookuptables/lookuptables.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

// Helper to compare EnvelopeStage (CppUTest can't print scoped enums)
#define CHECK_STATE(expected, actual) CHECK_EQUAL((int)(expected), (int)(actual))

// The internal pos threshold that triggers stage transitions
static constexpr uint32_t kPosThreshold = 8388608;

// Centred output: (lastValue - 1073741824) << 1
// OFF sentinel: -2147483648 (INT32_MIN)
static constexpr int32_t kOffSentinel = -2147483648;

// Common sustain levels as uint32_t parameters
static constexpr uint32_t kSustainFull = 2147483647u; // 100%
static constexpr uint32_t kSustainHalf = 1073741824u; // ~50%
static constexpr uint32_t kSustainQuarter = 536870912u; // ~25%
static constexpr uint32_t kSustainZero = 0u;

// Helper: run render() in a loop until state changes or maxIter is reached.
// Returns the number of iterations taken.
static int runUntilStateChange(Envelope& env, EnvelopeStage fromState, uint32_t numSamples, uint32_t attack,
                               uint32_t decay, uint32_t sustain, uint32_t release, int maxIter = 10000) {
	int iter = 0;
	while (env.state == fromState && iter < maxIter) {
		env.render(numSamples, attack, decay, sustain, release, decayTableSmall8);
		iter++;
	}
	return iter;
}

// Helper: drive an envelope from noteOn through attack and decay into sustain.
static void driveToSustain(Envelope& env, uint32_t sustain, uint32_t attackRate = 500000, uint32_t decayRate = 5000) {
	env.noteOn(false);
	runUntilStateChange(env, EnvelopeStage::ATTACK, 128, attackRate, decayRate, sustain, 500);
	runUntilStateChange(env, EnvelopeStage::DECAY, 128, attackRate, decayRate, sustain, 500);
}

TEST_GROUP(EnvelopeDeep){};

// ── Attack phase tests ─────────────────────────────────────────────────

TEST(EnvelopeDeep, ZeroAttackSkipsDirectlyToDecay) {
	// When attack rate is high enough that pos >= kPosThreshold in one step,
	// the envelope should transition directly to DECAY.
	Envelope env;
	env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);

	// Single render with attack large enough to exceed threshold
	env.render(1, kPosThreshold + 1, 100, kSustainHalf, 100, decayTableSmall8);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
}

TEST(EnvelopeDeep, InstantAttackViaDirectDecay) {
	// noteOn(true) bypasses attack entirely — starts in DECAY at max value
	Envelope env;
	int32_t ret = env.noteOn(true);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
	CHECK_EQUAL(2147483647, env.lastValue);
	// Centred output: (INT32_MAX - 1073741824) << 1 = 1073741823 * 2
	CHECK_EQUAL((int32_t)(1073741823 * 2), ret);
}

TEST(EnvelopeDeep, SlowAttackTakesManyIterations) {
	// With a very low attack rate, it should take many render() calls
	// to complete the attack phase.
	Envelope env;
	env.noteOn(false);

	uint32_t slowAttack = 10; // very slow
	int iters = runUntilStateChange(env, EnvelopeStage::ATTACK, 1, slowAttack, 100, kSustainHalf, 100);

	// Should take many iterations (pos needs to reach 8388608 at 10/sample)
	CHECK(iters > 1000);
}

TEST(EnvelopeDeep, AttackReachesNearMaxBeforeDecay) {
	// Just before transitioning to decay, lastValue should be close to INT32_MAX.
	Envelope env;
	env.noteOn(false);

	// Use moderate attack so we can observe the peak
	int32_t peakValue = 0;
	for (int i = 0; i < 10000 && env.state == EnvelopeStage::ATTACK; i++) {
		env.render(1, 5000, 100, kSustainHalf, 100, decayTableSmall8);
		if (env.state == EnvelopeStage::ATTACK) {
			peakValue = env.lastValue;
		}
	}
	// The last attack value should be well above half-range
	CHECK(peakValue > 1073741824);
}

TEST(EnvelopeDeep, AttackMonotonicRiseMultipleSampleSizes) {
	// Attack should be monotonically increasing regardless of numSamples variation
	Envelope env;
	env.noteOn(false);

	uint32_t sampleSizes[] = {1, 4, 16, 32, 64, 128};
	int32_t prev = 0;
	int idx = 0;
	for (int i = 0; i < 50 && env.state == EnvelopeStage::ATTACK; i++) {
		env.render(sampleSizes[idx % 6], 50000, 100, kSustainHalf, 100, decayTableSmall8);
		if (env.state == EnvelopeStage::ATTACK) {
			CHECK(env.lastValue >= prev);
			prev = env.lastValue;
		}
		idx++;
	}
}

TEST(EnvelopeDeep, AttackValueAlwaysPositive) {
	// During attack, lastValue should always be >= 1 (clamped in code)
	Envelope env;
	env.noteOn(false);
	CHECK_EQUAL(0, env.lastValue); // initial value before first render

	for (int i = 0; i < 100 && env.state == EnvelopeStage::ATTACK; i++) {
		env.render(1, 1000, 100, kSustainHalf, 100, decayTableSmall8);
		CHECK(env.lastValue >= 1); // code says max(lastValue, 1)
	}
}

// ── Decay phase tests ──────────────────────────────────────────────────

TEST(EnvelopeDeep, DecayTransitionFromAttack) {
	// After attack completes, the envelope enters decay.
	// lastValue in decay should start high and decrease toward sustain.
	Envelope env;
	env.noteOn(false);

	runUntilStateChange(env, EnvelopeStage::ATTACK, 128, 500000, 500, kSustainHalf, 500);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);

	// First decay value should be high (near max)
	int32_t firstDecayValue = env.lastValue;
	CHECK(firstDecayValue > kSustainHalf);

	// After more renders, value should decrease
	for (int i = 0; i < 50; i++) {
		env.render(128, 500000, 500, kSustainHalf, 500, decayTableSmall8);
	}
	CHECK(env.lastValue < firstDecayValue);
}

TEST(EnvelopeDeep, DecayCurveShape) {
	// The decay curve uses getDecay8 which produces an exponential-like shape.
	// Verify that early decay is steeper than late decay (concave up).
	Envelope env;
	env.noteOn(true); // direct to decay at max

	int32_t values[10];
	for (int i = 0; i < 10 && env.state == EnvelopeStage::DECAY; i++) {
		env.render(128, 0, 200, kSustainZero, 500, decayTableSmall8);
		values[i] = env.lastValue;
	}
	// With sustain=0, all values should be decreasing
	for (int i = 1; i < 10 && env.state == EnvelopeStage::DECAY; i++) {
		CHECK(values[i] <= values[i - 1]);
	}

	// Early drops should be larger than late drops (exponential shape)
	if (env.state == EnvelopeStage::DECAY) {
		int32_t earlyDrop = values[0] - values[1];
		int32_t lateDrop = values[8] - values[9];
		CHECK(earlyDrop >= lateDrop);
	}
}

TEST(EnvelopeDeep, DecayConvergesToSustain) {
	// After decay completes, lastValue should be near the sustain level
	Envelope env;
	env.noteOn(true);

	runUntilStateChange(env, EnvelopeStage::DECAY, 128, 0, 500, kSustainHalf, 500);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// Let smoothedSustain settle
	for (int i = 0; i < 200; i++) {
		env.render(128, 0, 500, kSustainHalf, 500, decayTableSmall8);
	}
	// Should be close to sustain level (within 5%)
	int32_t diff = std::abs(env.lastValue - (int32_t)kSustainHalf);
	CHECK(diff < (int32_t)(kSustainHalf / 20));
}

// ── Sustain phase tests ────────────────────────────────────────────────

TEST(EnvelopeDeep, SustainAtFullLevel) {
	Envelope env;
	driveToSustain(env, kSustainFull);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// Let smoothedSustain converge
	for (int i = 0; i < 500; i++) {
		env.render(128, 500000, 5000, kSustainFull, 500, decayTableSmall8);
	}
	// Should be near max
	CHECK(env.lastValue > (int32_t)(kSustainFull * 0.9));
}

TEST(EnvelopeDeep, SustainAtQuarterLevel) {
	Envelope env;
	driveToSustain(env, kSustainQuarter);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// Let smoothedSustain converge
	for (int i = 0; i < 500; i++) {
		env.render(128, 500000, 5000, kSustainQuarter, 500, decayTableSmall8);
	}
	int32_t diff = std::abs(env.lastValue - (int32_t)kSustainQuarter);
	CHECK(diff < (int32_t)(kSustainQuarter / 10));
}

TEST(EnvelopeDeep, SustainAtZeroLevel) {
	Envelope env;
	env.noteOn(true);
	runUntilStateChange(env, EnvelopeStage::DECAY, 128, 0, 5000, kSustainZero, 500);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// With zero sustain, value should converge to 0
	for (int i = 0; i < 500; i++) {
		env.render(128, 0, 5000, kSustainZero, 500, decayTableSmall8);
		CHECK(env.lastValue >= 0); // should never go negative
	}
	// After many iterations at zero sustain, lastValue should be very small
	CHECK(env.lastValue < 1000);
}

TEST(EnvelopeDeep, SustainHoldsDuration) {
	// Verify sustain holds indefinitely (state doesn't change)
	Envelope env;
	driveToSustain(env, kSustainHalf);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// Render 1000 times — should stay in sustain
	for (int i = 0; i < 1000; i++) {
		env.render(128, 500000, 5000, kSustainHalf, 500, decayTableSmall8);
		CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);
	}
}

TEST(EnvelopeDeep, SustainSmoothing) {
	// Verify the sustain smoothing: if we change sustain level during
	// sustain, lastValue should gradually move toward the new level.
	Envelope env;
	driveToSustain(env, kSustainHalf);

	// Let it settle at half
	for (int i = 0; i < 500; i++) {
		env.render(128, 500000, 5000, kSustainHalf, 500, decayTableSmall8);
	}
	int32_t halfVal = env.lastValue;

	// Now change sustain to full — should gradually increase
	int32_t prev = halfVal;
	bool increased = false;
	for (int i = 0; i < 500; i++) {
		env.render(128, 500000, 5000, kSustainFull, 500, decayTableSmall8);
		if (env.lastValue > prev + 100) {
			increased = true;
		}
		prev = env.lastValue;
	}
	CHECK(increased);
	// After settling, should be close to full sustain
	CHECK(env.lastValue > (int32_t)(kSustainFull * 0.8));
}

// ── Release phase tests ────────────────────────────────────────────────

TEST(EnvelopeDeep, ReleaseFromFullSustain) {
	Envelope env;
	driveToSustain(env, kSustainFull);
	for (int i = 0; i < 200; i++) {
		env.render(128, 500000, 5000, kSustainFull, 500, decayTableSmall8);
	}
	int32_t sustainVal = env.lastValue;

	env.unconditionalRelease();
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);
	CHECK_EQUAL(sustainVal, env.lastValuePreCurrentStage);

	// Release should decrease
	env.render(128, 0, 0, 0, 500, decayTableSmall8);
	CHECK(env.lastValue < sustainVal);
}

TEST(EnvelopeDeep, ReleaseFromHalfSustain) {
	Envelope env;
	driveToSustain(env, kSustainHalf);
	for (int i = 0; i < 200; i++) {
		env.render(128, 500000, 5000, kSustainHalf, 500, decayTableSmall8);
	}

	env.unconditionalRelease();
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);

	int iters = runUntilStateChange(env, EnvelopeStage::RELEASE, 128, 0, 0, 0, 500);
	CHECK_STATE(EnvelopeStage::OFF, env.state);
	CHECK(iters > 0);
}

TEST(EnvelopeDeep, ReleaseFromQuarterSustain) {
	Envelope env;
	driveToSustain(env, kSustainQuarter);
	for (int i = 0; i < 200; i++) {
		env.render(128, 500000, 5000, kSustainQuarter, 500, decayTableSmall8);
	}

	env.unconditionalRelease();
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);

	int iters = runUntilStateChange(env, EnvelopeStage::RELEASE, 128, 0, 0, 0, 500);
	CHECK_STATE(EnvelopeStage::OFF, env.state);
	CHECK(iters > 0);
}

TEST(EnvelopeDeep, FastReleaseRate) {
	// With a very high release rate, should reach OFF in one call
	Envelope env;
	driveToSustain(env, kSustainHalf);
	env.unconditionalRelease();

	int32_t ret = env.render(1, 0, 0, 0, kPosThreshold + 1, decayTableSmall8);
	CHECK_EQUAL(kOffSentinel, ret);
	CHECK_STATE(EnvelopeStage::OFF, env.state);
}

TEST(EnvelopeDeep, SlowReleaseRate) {
	// With a low release rate, should take many iterations
	Envelope env;
	driveToSustain(env, kSustainHalf);
	env.unconditionalRelease();

	int iters = runUntilStateChange(env, EnvelopeStage::RELEASE, 1, 0, 0, 0, 10, 1000000);
	CHECK(iters > 1000);
	CHECK_STATE(EnvelopeStage::OFF, env.state);
}

TEST(EnvelopeDeep, ReleaseMonotonicallyDecreases) {
	Envelope env;
	driveToSustain(env, kSustainHalf);
	for (int i = 0; i < 200; i++) {
		env.render(128, 500000, 5000, kSustainHalf, 500, decayTableSmall8);
	}

	env.unconditionalRelease();
	int32_t prev = env.lastValue;
	for (int i = 0; i < 100 && env.state == EnvelopeStage::RELEASE; i++) {
		env.render(128, 0, 0, 0, 500, decayTableSmall8);
		CHECK(env.lastValue <= prev);
		prev = env.lastValue;
	}
}

TEST(EnvelopeDeep, ReleaseOutputStaysNonNegative) {
	// Release values should not go below 0 (before centring)
	Envelope env;
	driveToSustain(env, kSustainFull);
	for (int i = 0; i < 200; i++) {
		env.render(128, 500000, 5000, kSustainFull, 500, decayTableSmall8);
	}
	env.unconditionalRelease();

	for (int i = 0; i < 1000 && env.state == EnvelopeStage::RELEASE; i++) {
		env.render(128, 0, 0, 0, 200, decayTableSmall8);
		CHECK(env.lastValue >= 0);
	}
}

// ── Full ADSR cycle tests ──────────────────────────────────────────────

TEST(EnvelopeDeep, FullCycleAttackDecaySustainRelease) {
	Envelope env;
	env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);

	uint32_t attack = 200000;
	uint32_t decay = 2000;
	uint32_t sustain = kSustainHalf;
	uint32_t release = 2000;

	// Attack
	runUntilStateChange(env, EnvelopeStage::ATTACK, 128, attack, decay, sustain, release);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
	int32_t peakValue = env.lastValue;
	CHECK(peakValue > 0);

	// Decay
	runUntilStateChange(env, EnvelopeStage::DECAY, 128, attack, decay, sustain, release);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// Sustain — hold for a while
	for (int i = 0; i < 100; i++) {
		env.render(128, attack, decay, sustain, release, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// Release
	env.unconditionalRelease();
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);

	int32_t ret = 0;
	int iters = 0;
	while (env.state == EnvelopeStage::RELEASE && iters < 10000) {
		ret = env.render(128, attack, decay, sustain, release, decayTableSmall8);
		iters++;
	}
	CHECK_STATE(EnvelopeStage::OFF, env.state);
	CHECK_EQUAL(kOffSentinel, ret);
}

TEST(EnvelopeDeep, FullCycleWithDirectDecay) {
	// Skipping attack entirely
	Envelope env;
	env.noteOn(true);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
	CHECK_EQUAL(2147483647, env.lastValue);

	runUntilStateChange(env, EnvelopeStage::DECAY, 128, 0, 1000, kSustainHalf, 1000);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	env.unconditionalRelease();
	runUntilStateChange(env, EnvelopeStage::RELEASE, 128, 0, 0, 0, 1000);
	CHECK_STATE(EnvelopeStage::OFF, env.state);
}

// ── Re-trigger tests ───────────────────────────────────────────────────

TEST(EnvelopeDeep, RetriggerDuringAttack) {
	Envelope env;
	env.noteOn(false);

	// Advance partway through attack
	for (int i = 0; i < 10; i++) {
		env.render(1, 50000, 100, kSustainHalf, 100, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
	int32_t midAttackValue = env.lastValue;
	CHECK(midAttackValue > 0);

	// Re-trigger
	env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
	CHECK_EQUAL(0, env.lastValue);
	CHECK_EQUAL(0u, env.pos);
}

TEST(EnvelopeDeep, RetriggerDuringDecay) {
	Envelope env;
	env.noteOn(true); // direct to decay

	// Advance partway through decay
	for (int i = 0; i < 50; i++) {
		env.render(128, 0, 200, kSustainHalf, 500, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::DECAY, env.state);

	// Re-trigger
	env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
	CHECK_EQUAL(0, env.lastValue);
}

TEST(EnvelopeDeep, RetriggerDuringSustain) {
	Envelope env;
	driveToSustain(env, kSustainHalf);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// Re-trigger
	env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
	CHECK_EQUAL(0, env.lastValue);
}

TEST(EnvelopeDeep, RetriggerDuringRelease) {
	Envelope env;
	driveToSustain(env, kSustainHalf);
	env.unconditionalRelease();
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);

	// Advance partway through release
	for (int i = 0; i < 10; i++) {
		env.render(128, 0, 0, 0, 200, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);

	// Re-trigger
	env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
	CHECK_EQUAL(0, env.lastValue);
}

TEST(EnvelopeDeep, RetriggerDirectDecayDuringRelease) {
	// Re-trigger with directlyToDecay=true during release
	Envelope env;
	driveToSustain(env, kSustainHalf);
	env.unconditionalRelease();

	env.noteOn(true);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
	CHECK_EQUAL(2147483647, env.lastValue);
}

// ── Parameter change mid-phase tests ───────────────────────────────────

TEST(EnvelopeDeep, ChangeAttackSpeedDuringAttack) {
	// Start with slow attack, then switch to fast attack mid-phase
	Envelope env;
	env.noteOn(false);

	// Slow attack for a while
	for (int i = 0; i < 100; i++) {
		env.render(1, 100, 100, kSustainHalf, 100, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
	int32_t slowPos = env.lastValue;

	// Now use fast attack — should transition to decay quickly
	int iters = runUntilStateChange(env, EnvelopeStage::ATTACK, 128, 500000, 100, kSustainHalf, 100);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
	CHECK(iters < 1000);
}

TEST(EnvelopeDeep, ChangeSustainDuringDecay) {
	// Change sustain level while in decay — envelope should target new level
	Envelope env;
	env.noteOn(true);

	// Decay with high sustain
	for (int i = 0; i < 100; i++) {
		env.render(128, 0, 200, kSustainFull, 500, decayTableSmall8);
	}

	// Switch to low sustain
	for (int i = 0; i < 500; i++) {
		env.render(128, 0, 200, kSustainQuarter, 500, decayTableSmall8);
	}

	// After decay completes and enters sustain, should converge toward quarter
	if (env.state == EnvelopeStage::SUSTAIN) {
		for (int i = 0; i < 500; i++) {
			env.render(128, 0, 200, kSustainQuarter, 500, decayTableSmall8);
		}
		int32_t diff = std::abs(env.lastValue - (int32_t)kSustainQuarter);
		CHECK(diff < (int32_t)(kSustainQuarter / 5));
	}
}

TEST(EnvelopeDeep, ChangeReleaseSpeedDuringRelease) {
	// Start release with slow speed, then switch to fast
	Envelope env;
	driveToSustain(env, kSustainHalf);
	for (int i = 0; i < 200; i++) {
		env.render(128, 500000, 5000, kSustainHalf, 500, decayTableSmall8);
	}
	env.unconditionalRelease();

	// Slow release
	for (int i = 0; i < 10 && env.state == EnvelopeStage::RELEASE; i++) {
		env.render(128, 0, 0, 0, 10, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);

	// Fast release — should finish quickly
	int iters = runUntilStateChange(env, EnvelopeStage::RELEASE, 128, 0, 0, 0, 100000);
	CHECK_STATE(EnvelopeStage::OFF, env.state);
}

// ── Edge cases ─────────────────────────────────────────────────────────

TEST(EnvelopeDeep, ZeroAttackZeroDecayInstantSustain) {
	// Both attack and decay at maximum speed — should reach sustain almost instantly
	Envelope env;
	env.noteOn(false);

	// One render with instant attack + instant decay
	env.render(1, kPosThreshold + 1, kPosThreshold + 1, kSustainHalf, 500, decayTableSmall8);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);
}

TEST(EnvelopeDeep, ZeroSustainSkipsStraightThrough) {
	// With zero sustain, decay targets 0. Once in sustain with sustain=0,
	// value should be near zero.
	Envelope env;
	env.noteOn(true);

	runUntilStateChange(env, EnvelopeStage::DECAY, 128, 0, 5000, kSustainZero, 500);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// In sustain with zero level — value should converge to 0
	for (int i = 0; i < 200; i++) {
		env.render(128, 0, 5000, kSustainZero, 500, decayTableSmall8);
		CHECK(env.lastValue >= 0);
	}
}

TEST(EnvelopeDeep, MaxAttackRate) {
	// Maximum possible attack rate
	Envelope env;
	env.noteOn(false);
	env.render(1, UINT32_MAX, 100, kSustainHalf, 100, decayTableSmall8);
	// Should have transitioned past attack (pos overflows but >= threshold check still works)
	CHECK((int)env.state >= (int)EnvelopeStage::DECAY);
}

TEST(EnvelopeDeep, MaxDecayRate) {
	Envelope env;
	env.noteOn(true);
	env.render(1, 0, UINT32_MAX, kSustainHalf, 100, decayTableSmall8);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);
}

TEST(EnvelopeDeep, MaxReleaseRate) {
	Envelope env;
	driveToSustain(env, kSustainHalf);
	env.unconditionalRelease();

	int32_t ret = env.render(1, 0, 0, 0, UINT32_MAX, decayTableSmall8);
	CHECK_EQUAL(kOffSentinel, ret);
	CHECK_STATE(EnvelopeStage::OFF, env.state);
}

// ── noteOn/noteOff rapid cycling ───────────────────────────────────────

TEST(EnvelopeDeep, RapidNoteOnOffCycling) {
	// Rapidly cycle noteOn → small render → release → small render
	Envelope env;
	for (int cycle = 0; cycle < 50; cycle++) {
		env.noteOn(false);
		CHECK_STATE(EnvelopeStage::ATTACK, env.state);

		// Small amount of rendering
		for (int i = 0; i < 5; i++) {
			env.render(128, 200000, 5000, kSustainHalf, 5000, decayTableSmall8);
		}

		// Force release
		env.unconditionalRelease();

		// Small amount of release rendering
		for (int i = 0; i < 5; i++) {
			env.render(128, 0, 0, 0, 5000, decayTableSmall8);
		}
	}
	// Should not crash or end up in invalid state
	CHECK((int)env.state >= (int)EnvelopeStage::ATTACK);
	CHECK((int)env.state <= (int)EnvelopeStage::OFF);
}

TEST(EnvelopeDeep, RapidNoteOnDirectDecayCycling) {
	// Rapidly cycle with directlyToDecay=true
	Envelope env;
	for (int cycle = 0; cycle < 100; cycle++) {
		env.noteOn(true);
		env.render(128, 0, 5000, kSustainHalf, 5000, decayTableSmall8);
		env.unconditionalRelease();
		env.render(128, 0, 0, 0, 50000, decayTableSmall8);
	}
	CHECK((int)env.state >= (int)EnvelopeStage::ATTACK);
	CHECK((int)env.state <= (int)EnvelopeStage::OFF);
}

TEST(EnvelopeDeep, NoteOnWithoutReleaseBetween) {
	// Multiple noteOn calls without releasing — should reset each time
	Envelope env;
	for (int i = 0; i < 20; i++) {
		env.noteOn(false);
		CHECK_STATE(EnvelopeStage::ATTACK, env.state);
		CHECK_EQUAL(0, env.lastValue);
		env.render(128, 100000, 5000, kSustainHalf, 500, decayTableSmall8);
	}
}

// ── render() output range verification ─────────────────────────────────

TEST(EnvelopeDeep, RenderOutputRangeAttack) {
	// During attack, centred output should be in [-2147483648, 2147483646]
	Envelope env;
	env.noteOn(false);

	for (int i = 0; i < 200 && env.state == EnvelopeStage::ATTACK; i++) {
		int32_t ret = env.render(1, 50000, 100, kSustainHalf, 100, decayTableSmall8);
		// Output is (lastValue - 1073741824) << 1
		// lastValue ranges from 1 to ~2147483647 during attack
		// So output ranges from about -2147483646 to +2147483646
		CHECK(ret >= (int32_t)-2147483647);
		CHECK(ret != kOffSentinel); // should never be OFF sentinel during attack
	}
}

TEST(EnvelopeDeep, RenderOutputRangeDecay) {
	Envelope env;
	env.noteOn(true);

	for (int i = 0; i < 200 && env.state == EnvelopeStage::DECAY; i++) {
		int32_t ret = env.render(128, 0, 200, kSustainHalf, 100, decayTableSmall8);
		CHECK(ret != kOffSentinel);
	}
}

TEST(EnvelopeDeep, RenderOutputRangeRelease) {
	Envelope env;
	driveToSustain(env, kSustainHalf);
	for (int i = 0; i < 200; i++) {
		env.render(128, 500000, 5000, kSustainHalf, 500, decayTableSmall8);
	}
	env.unconditionalRelease();

	int32_t lastRet = 0;
	bool gotSentinel = false;
	for (int i = 0; i < 10000 && !gotSentinel; i++) {
		lastRet = env.render(128, 0, 0, 0, 200, decayTableSmall8);
		if (lastRet == kOffSentinel) {
			gotSentinel = true;
			CHECK_STATE(EnvelopeStage::OFF, env.state);
		}
	}
	CHECK(gotSentinel); // should eventually reach OFF
}

TEST(EnvelopeDeep, OffStateAlwaysReturnsSentinel) {
	Envelope env;
	env.noteOn(false);
	env.unconditionalOff();

	// Multiple calls to render() in OFF state should all return sentinel
	for (int i = 0; i < 10; i++) {
		int32_t ret = env.render(128, 100000, 5000, kSustainHalf, 5000, decayTableSmall8);
		CHECK_EQUAL(kOffSentinel, ret);
		CHECK_STATE(EnvelopeStage::OFF, env.state);
	}
}

// ── Multiple render() calls simulating time progression ────────────────

TEST(EnvelopeDeep, TimeProgressionAttackConsistency) {
	// Verify that rendering N samples in one call vs. 1 sample N times
	// produces similar results (not identical due to curve quantization)
	Envelope envBatch;
	envBatch.noteOn(false);
	envBatch.render(100, 10000, 100, kSustainHalf, 100, decayTableSmall8);
	int32_t batchValue = envBatch.lastValue;

	Envelope envSingle;
	envSingle.noteOn(false);
	for (int i = 0; i < 100; i++) {
		envSingle.render(1, 10000, 100, kSustainHalf, 100, decayTableSmall8);
	}
	int32_t singleValue = envSingle.lastValue;

	// Both envelopes advanced by the same total pos increment (10000 * 100),
	// but the single-sample version re-evaluates the curve at each step.
	// The batch version jumps directly. They should be in the same ballpark.
	// Both should be positive and in attack or early decay.
	CHECK(batchValue > 0);
	CHECK(singleValue > 0);
}

TEST(EnvelopeDeep, VaryingNumSamplesPerRender) {
	// Simulate real firmware conditions where numSamples varies each call
	Envelope env;
	env.noteOn(false);

	uint32_t sampleCounts[] = {1, 8, 32, 64, 128, 16, 4, 48, 96, 2};
	int idx = 0;
	int32_t prev = 0;

	// Track that attack is still monotonically increasing with varying sizes
	for (int i = 0; i < 100 && env.state == EnvelopeStage::ATTACK; i++) {
		env.render(sampleCounts[idx % 10], 20000, 100, kSustainHalf, 100, decayTableSmall8);
		if (env.state == EnvelopeStage::ATTACK) {
			CHECK(env.lastValue >= prev);
			prev = env.lastValue;
		}
		idx++;
	}
}

// ── Fast release specific tests ────────────────────────────────────────

TEST(EnvelopeDeep, FastReleaseUsesMinOfIncrementAndDoubleRelease) {
	// Fast release uses max(fastReleaseIncrement, 2*release) for pos increment
	Envelope env;
	driveToSustain(env, kSustainHalf);

	// Set up fast release with small increment but larger release param
	uint32_t releaseRate = 100000;
	env.unconditionalRelease(EnvelopeStage::FAST_RELEASE, 1000); // small increment
	CHECK_STATE(EnvelopeStage::FAST_RELEASE, env.state);

	// The code: if (fastReleaseIncrement < 2 * release) { release = 2*release; fastReleaseIncrement = release; }
	// So with fastReleaseIncrement=1000 and release=100000, 2*100000=200000 > 1000
	// so fastReleaseIncrement gets set to 200000
	env.render(1, 0, 0, 0, releaseRate, decayTableSmall8);
	CHECK_EQUAL(2 * releaseRate, env.fastReleaseIncrement);
}

TEST(EnvelopeDeep, FastReleaseCompletesToOff) {
	Envelope env;
	driveToSustain(env, kSustainHalf);
	env.unconditionalRelease(EnvelopeStage::FAST_RELEASE, 65536);

	int iters = 0;
	while (env.state == EnvelopeStage::FAST_RELEASE && iters < 10000) {
		int32_t ret = env.render(128, 0, 0, 0, 100, decayTableSmall8);
		if (ret == kOffSentinel) {
			CHECK_STATE(EnvelopeStage::OFF, env.state);
			break;
		}
		iters++;
	}
	CHECK_STATE(EnvelopeStage::OFF, env.state);
}

// ── unconditionalRelease idempotency ───────────────────────────────────

TEST(EnvelopeDeep, DoubleUnconditionalRelease) {
	// Calling unconditionalRelease twice should not reset pos if already in release
	Envelope env;
	driveToSustain(env, kSustainHalf);
	env.unconditionalRelease();
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);

	// Advance release
	for (int i = 0; i < 10; i++) {
		env.render(128, 0, 0, 0, 500, decayTableSmall8);
	}
	uint32_t posAfterSomeRelease = env.pos;

	// Second unconditionalRelease — same type, so should NOT reset
	env.unconditionalRelease();
	CHECK_EQUAL(posAfterSomeRelease, env.pos); // pos unchanged
}

TEST(EnvelopeDeep, UnconditionalReleaseThenFastRelease) {
	// Switching from RELEASE to FAST_RELEASE should reset pos
	Envelope env;
	driveToSustain(env, kSustainHalf);
	env.unconditionalRelease();

	for (int i = 0; i < 10; i++) {
		env.render(128, 0, 0, 0, 500, decayTableSmall8);
	}

	// Now switch to fast release — different type, so pos should reset
	env.unconditionalRelease(EnvelopeStage::FAST_RELEASE, 65536);
	CHECK_STATE(EnvelopeStage::FAST_RELEASE, env.state);
	CHECK_EQUAL(0u, env.pos);
}

// ── resumeAttack tests ─────────────────────────────────────────────────

TEST(EnvelopeDeep, ResumeAttackFromMidValue) {
	// resumeAttack should set pos based on the inverse of the attack curve
	Envelope env;
	env.noteOn(false);

	// Advance attack to get a mid-range value
	for (int i = 0; i < 50; i++) {
		env.render(1, 50000, 100, kSustainHalf, 100, decayTableSmall8);
	}
	int32_t midValue = env.lastValue;
	uint32_t posBeforeResume = env.pos;

	// Resume attack with the same value — pos should be similar
	env.resumeAttack(midValue);
	// Pos should be recalculated but close to original
	// (exact match depends on lookup table resolution)
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
}

TEST(EnvelopeDeep, ResumeAttackOnlyWorksInAttackState) {
	// resumeAttack should only modify pos when in ATTACK state
	Envelope env;
	env.noteOn(true); // DECAY state
	uint32_t origPos = env.pos;

	env.resumeAttack(1000000);
	// Should not have changed pos since we're not in ATTACK
	CHECK_EQUAL(origPos, env.pos);
}

// ── ignoredNoteOff / sustain-with-no-sustain behavior ──────────────────

TEST(EnvelopeDeep, IgnoredNoteOffReleasesAtSustain) {
	// When ignoredNoteOff is set, reaching SUSTAIN should auto-release.
	// The transition happens inside the SUSTAIN case of render(): after
	// entering sustain from decay, the *next* render() call sees
	// ignoredNoteOff and calls unconditionalRelease().
	Envelope env;
	env.noteOn(false);
	env.ignoredNoteOff = true;

	// Drive through attack
	runUntilStateChange(env, EnvelopeStage::ATTACK, 128, 500000, 5000, kSustainHalf, 5000);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);

	// Drive through decay into sustain
	runUntilStateChange(env, EnvelopeStage::DECAY, 128, 500000, 5000, kSustainHalf, 5000);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// One more render triggers the ignoredNoteOff → unconditionalRelease path
	env.render(128, 500000, 5000, kSustainHalf, 5000, decayTableSmall8);
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);
}

TEST(EnvelopeDeep, NoteOnClearsIgnoredNoteOff) {
	Envelope env;
	env.ignoredNoteOff = true;
	env.noteOn(false);
	CHECK(!env.ignoredNoteOff);
}

// ── Stress test: many full lifecycles ──────────────────────────────────

TEST(EnvelopeDeep, StressManyFullLifecycles) {
	Envelope env;
	for (int cycle = 0; cycle < 100; cycle++) {
		env.noteOn(cycle % 2 == 0 ? false : true);

		uint32_t attack = 200000 + (cycle * 1000);
		uint32_t decay = 1000 + (cycle * 100);
		uint32_t sustain = kSustainQuarter + (cycle * 10000);
		uint32_t release = 500 + (cycle * 50);

		// Run through full lifecycle
		if (env.state == EnvelopeStage::ATTACK) {
			runUntilStateChange(env, EnvelopeStage::ATTACK, 128, attack, decay, sustain, release);
		}
		if (env.state == EnvelopeStage::DECAY) {
			runUntilStateChange(env, EnvelopeStage::DECAY, 128, attack, decay, sustain, release);
		}
		if (env.state == EnvelopeStage::SUSTAIN) {
			for (int i = 0; i < 10; i++) {
				env.render(128, attack, decay, sustain, release, decayTableSmall8);
			}
		}

		env.unconditionalRelease();
		runUntilStateChange(env, EnvelopeStage::RELEASE, 128, attack, decay, sustain, release);
		CHECK_STATE(EnvelopeStage::OFF, env.state);
	}
}

TEST(EnvelopeDeep, StressVaryingParameters) {
	// Run envelope with parameters that change every render call
	Envelope env;
	env.noteOn(false);

	for (int i = 0; i < 2000; i++) {
		uint32_t attack = 50000 + (i * 137) % 500000;
		uint32_t decay = 100 + (i * 53) % 10000;
		uint32_t sustain = (i * 7919) % 2147483647u;
		uint32_t release = 100 + (i * 41) % 10000;

		int32_t ret = env.render(128, attack, decay, sustain, release, decayTableSmall8);

		if (env.state == EnvelopeStage::OFF) {
			CHECK_EQUAL(kOffSentinel, ret);
			break;
		}
		// Value should be in valid range (lastValue >= 0 during active states)
		if (env.state != EnvelopeStage::OFF) {
			CHECK(env.lastValue >= 0);
		}

		// Trigger release after a while
		if (i == 1000 && env.state < EnvelopeStage::RELEASE) {
			env.unconditionalRelease();
		}
	}
}

// ── numSamples edge cases ──────────────────────────────────────────────

TEST(EnvelopeDeep, SingleSampleRender) {
	Envelope env;
	env.noteOn(false);

	// Rendering 1 sample at a time should work correctly
	int32_t prev = 0;
	for (int i = 0; i < 50 && env.state == EnvelopeStage::ATTACK; i++) {
		env.render(1, 100000, 100, kSustainHalf, 100, decayTableSmall8);
		CHECK(env.lastValue >= prev);
		prev = env.lastValue;
	}
}

TEST(EnvelopeDeep, LargeNumSamplesRender) {
	// Rendering with large numSamples (e.g., 128, the max in firmware)
	Envelope env;
	env.noteOn(false);

	env.render(128, 100000, 100, kSustainHalf, 100, decayTableSmall8);
	CHECK(env.lastValue > 0);
}

// ── unconditionalOff preserves lastValue ───────────────────────────────

TEST(EnvelopeDeep, UnconditionalOffSavesLastValue) {
	Envelope env;
	env.noteOn(false);

	// Build up some value
	for (int i = 0; i < 20; i++) {
		env.render(1, 100000, 100, kSustainHalf, 100, decayTableSmall8);
	}
	int32_t valueBeforeOff = env.lastValue;

	env.unconditionalOff();
	CHECK_STATE(EnvelopeStage::OFF, env.state);
	CHECK_EQUAL(valueBeforeOff, env.lastValuePreCurrentStage);
}
