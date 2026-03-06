// Envelope ADSR regression tests — exercises the firmware's real Envelope
// DSP compiled on x86.

#include "CppUTest/TestHarness.h"
#include "modulation/envelope.h"
#include "model/voice/voice.h"
#include "processing/sound/sound.h"
#include "util/lookuptables/lookuptables.h"

// Helper to compare EnvelopeStage (CppUTest can't print scoped enums)
#define CHECK_STATE(expected, actual) CHECK_EQUAL((int)(expected), (int)(actual))

TEST_GROUP(EnvelopeTest) {};

TEST(EnvelopeTest, noteOnAttack) {
	Envelope env;
	int32_t ret = env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
	CHECK_EQUAL(0, env.lastValue);
	// Return: (lastValue - 1073741824) << 1
	CHECK_EQUAL((int32_t)((0 - 1073741824) * 2), ret);
}

TEST(EnvelopeTest, noteOnDirectDecay) {
	Envelope env;
	int32_t ret = env.noteOn(true);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
	CHECK_EQUAL(2147483647, env.lastValue);
	CHECK_EQUAL((int32_t)(1073741823 * 2), ret);
}

TEST(EnvelopeTest, renderAttackPhase) {
	Envelope env;
	env.noteOn(false);

	// Moderate attack rate — value should increase
	env.render(1, 100, 0, 0, 0, decayTableSmall8);
	CHECK(env.lastValue > 0);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
}

TEST(EnvelopeTest, renderAttackToDecay) {
	Envelope env;
	env.noteOn(false);

	// Very high attack → pos exceeds 8388608 → transition to DECAY
	env.render(1, 8388609, 100, 1073741824u, 100, decayTableSmall8);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
}

TEST(EnvelopeTest, renderDecayPhase) {
	Envelope env;
	env.noteOn(true); // directly to DECAY, lastValue = INT32_MAX
	int32_t valBefore = env.lastValue;

	env.render(10, 0, 500, 1073741824u, 0, decayTableSmall8);
	CHECK(env.lastValue < valBefore);
}

TEST(EnvelopeTest, renderDecayToSustain) {
	Envelope env;
	env.noteOn(true);

	// Very high decay rate → pos exceeds threshold → SUSTAIN
	env.render(1, 0, 8388609, 1073741824u, 0, decayTableSmall8);
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);
}

TEST(EnvelopeTest, unconditionalRelease) {
	Envelope env;
	env.noteOn(false);
	env.unconditionalRelease();
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);
	CHECK_EQUAL(0u, env.pos);
}

TEST(EnvelopeTest, renderReleaseToOff) {
	Envelope env;
	env.noteOn(true);
	// Force to SUSTAIN
	env.render(1, 0, 8388609, 1073741824u, 0, decayTableSmall8);
	env.unconditionalRelease();

	// Fast release → pos exceeds threshold → OFF sentinel
	int32_t ret = env.render(1, 0, 0, 0, 8388609, decayTableSmall8);
	CHECK_EQUAL((int32_t)-2147483648, ret);
	CHECK_STATE(EnvelopeStage::OFF, env.state);
}

TEST(EnvelopeTest, fastRelease) {
	Envelope env;
	env.noteOn(true);
	env.unconditionalRelease(EnvelopeStage::FAST_RELEASE, 65536);
	CHECK_STATE(EnvelopeStage::FAST_RELEASE, env.state);
	CHECK_EQUAL(65536u, env.fastReleaseIncrement);
}

TEST(EnvelopeTest, unconditionalOff) {
	Envelope env;
	env.noteOn(false);
	env.unconditionalOff();
	CHECK_STATE(EnvelopeStage::OFF, env.state);
}

TEST(EnvelopeTest, renderOffReturnsSentinel) {
	Envelope env;
	env.noteOn(false);
	env.unconditionalOff();
	int32_t ret = env.render(1, 0, 0, 0, 0, decayTableSmall8);
	CHECK_EQUAL((int32_t)-2147483648, ret);
}

TEST(EnvelopeTest, resumeAttack) {
	Envelope env;
	env.noteOn(false);
	// Advance attack significantly so lastValue is well into the curve
	env.render(100, 50000, 0, 0, 0, decayTableSmall8);
	int32_t midValue = env.lastValue;
	CHECK(midValue > 0);

	// resumeAttack recalculates pos from oldLastValue via inverse lookup
	env.resumeAttack(midValue);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
}

TEST(EnvelopeTest, noteOnWithVoiceAttack) {
	Envelope env;
	Sound sound;
	Voice voice;
	// Attack below threshold → normal attack
	voice.paramFinalValues[deluge::modulation::params::LOCAL_ENV_0_ATTACK] = 1000;
	voice.paramFinalValues[deluge::modulation::params::LOCAL_ENV_0_SUSTAIN] = 500000;

	env.noteOn(0, &sound, &voice);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);
}

TEST(EnvelopeTest, noteOnWithVoiceDirectDecay) {
	Envelope env;
	Sound sound;
	Voice voice;
	// Attack above threshold (245632) → directly to DECAY
	voice.paramFinalValues[deluge::modulation::params::LOCAL_ENV_0_ATTACK] = 300000;
	voice.paramFinalValues[deluge::modulation::params::LOCAL_ENV_0_SUSTAIN] = 500000;

	env.noteOn(0, &sound, &voice);
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
}

TEST(EnvelopeTest, noteOff) {
	Envelope env;
	Sound sound;
	ParamManagerForTimeline* pm = nullptr;
	env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);

	// noteOff with sustain available → unconditional release
	env.noteOff(0, &sound, pm);
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);
}

// ── Full ADSR lifecycle tests ──────────────────────────────────────────

TEST(EnvelopeTest, fullADSRLifecycle) {
	Envelope env;

	// Attack
	env.noteOn(false);
	CHECK_STATE(EnvelopeStage::ATTACK, env.state);

	// Render through attack phase into decay
	uint32_t attack = 500000; // moderate attack
	uint32_t decay = 500;
	uint32_t sustain = 1073741824u; // half sustain
	uint32_t release = 500;

	// Run attack until we transition to decay
	int maxIter = 100;
	while (env.state == EnvelopeStage::ATTACK && maxIter-- > 0) {
		env.render(128, attack, decay, sustain, release, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::DECAY, env.state);
	CHECK(env.lastValue > 0); // should be near peak

	// Run decay until sustain
	maxIter = 1000;
	while (env.state == EnvelopeStage::DECAY && maxIter-- > 0) {
		env.render(128, attack, decay, sustain, release, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// Value should be near sustain level
	int32_t sustainValue = env.lastValue;
	CHECK(sustainValue > 0);

	// Release
	env.unconditionalRelease();
	CHECK_STATE(EnvelopeStage::RELEASE, env.state);

	// Run until OFF
	maxIter = 1000;
	int32_t ret = 0;
	while (env.state == EnvelopeStage::RELEASE && maxIter-- > 0) {
		ret = env.render(128, attack, decay, sustain, release, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::OFF, env.state);
	CHECK_EQUAL((int32_t)-2147483648, ret); // OFF sentinel
}

TEST(EnvelopeTest, attackValueMonotonicallyIncreases) {
	Envelope env;
	env.noteOn(false);

	int32_t prev = env.lastValue;
	for (int i = 0; i < 20 && env.state == EnvelopeStage::ATTACK; i++) {
		env.render(1, 100000, 500, 1073741824u, 500, decayTableSmall8);
		CHECK(env.lastValue >= prev);
		prev = env.lastValue;
	}
}

TEST(EnvelopeTest, decayValueMonotonicallyDecreases) {
	Envelope env;
	env.noteOn(true); // direct to decay, starts at INT32_MAX

	int32_t prev = env.lastValue;
	for (int i = 0; i < 20 && env.state == EnvelopeStage::DECAY; i++) {
		env.render(10, 0, 100, 1073741824u, 0, decayTableSmall8);
		CHECK(env.lastValue <= prev);
		prev = env.lastValue;
	}
}

TEST(EnvelopeTest, sustainHoldsLevel) {
	Envelope env;
	env.noteOn(true); // direct to decay
	uint32_t sustain = 1073741824u;

	// Drive into sustain
	int maxIter = 1000;
	while (env.state == EnvelopeStage::DECAY && maxIter-- > 0) {
		env.render(128, 0, 500, sustain, 500, decayTableSmall8);
	}
	CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);

	// In sustain, value should remain stable
	int32_t sustainVal = env.lastValue;
	for (int i = 0; i < 10; i++) {
		env.render(128, 0, 500, sustain, 500, decayTableSmall8);
		CHECK_STATE(EnvelopeStage::SUSTAIN, env.state);
	}
	// Value should not have changed much
	int32_t diff = std::abs(env.lastValue - sustainVal);
	CHECK(diff < sustainVal / 10); // within 10%
}

TEST(EnvelopeTest, fastReleaseIsFasterThanNormalRelease) {
	// Normal release
	Envelope envNormal;
	envNormal.noteOn(true);
	envNormal.render(1, 0, 8388609, 1073741824u, 0, decayTableSmall8); // to sustain
	envNormal.unconditionalRelease();

	int normalSteps = 0;
	while (envNormal.state == EnvelopeStage::RELEASE && normalSteps < 1000) {
		envNormal.render(128, 0, 0, 0, 100, decayTableSmall8);
		normalSteps++;
	}

	// Fast release
	Envelope envFast;
	envFast.noteOn(true);
	envFast.render(1, 0, 8388609, 1073741824u, 0, decayTableSmall8); // to sustain
	envFast.unconditionalRelease(EnvelopeStage::FAST_RELEASE, 65536);

	int fastSteps = 0;
	while (envFast.state == EnvelopeStage::FAST_RELEASE && fastSteps < 1000) {
		envFast.render(128, 0, 0, 0, 100, decayTableSmall8);
		fastSteps++;
	}

	CHECK(fastSteps <= normalSteps);
}
