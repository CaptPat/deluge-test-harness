// Tests for Sound state query methods — verifies the real firmware logic
// for determining active sources, envelope sustain, oscillator sync, etc.
// These methods control audio rendering decisions and voice lifecycle.

#include "CppUTest/TestHarness.h"
#include "processing/sound/sound_instrument.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"

namespace params = deluge::modulation::params;

// ── ParamManager helper ────────────────────────────────────────────────
// Sets up a minimal ParamManager with PatchedParamSet for Sound queries.

namespace {

struct SoundTestHelper {
	ParamCollectionSummary summaries[PARAM_COLLECTIONS_STORAGE_NUM]{};
	PatchedParamSet patchedParamSet{&summaries[1]};
	ParamManagerForTimeline paramManager;
	SoundInstrument si;

	SoundTestHelper() {
		paramManager.summaries[1].paramCollection = &patchedParamSet;
	}

	PatchedParamSet& patched() { return patchedParamSet; }
};

} // namespace

// ── hasFilters ─────────────────────────────────────────────────────────

TEST_GROUP(SoundStateQuery){};

TEST(SoundStateQuery, HasFiltersBothOff) {
	SoundInstrument si;
	si.lpfMode = FilterMode::OFF;
	si.hpfMode = FilterMode::OFF;
	CHECK(!si.hasFilters());
}

TEST(SoundStateQuery, HasFiltersLpfOn) {
	SoundInstrument si;
	si.lpfMode = FilterMode::TRANSISTOR_24DB;
	si.hpfMode = FilterMode::OFF;
	CHECK(si.hasFilters());
}

TEST(SoundStateQuery, HasFiltersHpfOn) {
	SoundInstrument si;
	si.lpfMode = FilterMode::OFF;
	si.hpfMode = FilterMode::HPLADDER;
	CHECK(si.hasFilters());
}

TEST(SoundStateQuery, HasFiltersBothOn) {
	SoundInstrument si;
	CHECK(si.hasFilters()); // defaults: TRANSISTOR_24DB + HPLADDER
}

// ── envelopeHasSustainCurrently ────────────────────────────────────────

TEST(SoundStateQuery, EnvelopeSustainCurrentlyDefaultIsMin) {
	SoundTestHelper h;
	// Default patched param value is 0, not -2147483648
	// So sustain != min → true
	CHECK(h.si.envelopeHasSustainCurrently(0, &h.paramManager));
}

TEST(SoundStateQuery, EnvelopeSustainCurrentlyAtMinimum) {
	SoundTestHelper h;
	// Set sustain to minimum value
	h.patched().params[params::LOCAL_ENV_0_SUSTAIN].setCurrentValueBasicForSetup(-2147483648);
	// Set decay <= release so the fallback condition is false too
	h.patched().params[params::LOCAL_ENV_0_DECAY].setCurrentValueBasicForSetup(-2147483648);
	h.patched().params[params::LOCAL_ENV_0_RELEASE].setCurrentValueBasicForSetup(0);
	CHECK(!h.si.envelopeHasSustainCurrently(0, &h.paramManager));
}

TEST(SoundStateQuery, EnvelopeSustainCurrentlyDecayGreaterThanRelease) {
	SoundTestHelper h;
	// Sustain at minimum but decay > release → still has sustain behavior
	h.patched().params[params::LOCAL_ENV_0_SUSTAIN].setCurrentValueBasicForSetup(-2147483648);
	h.patched().params[params::LOCAL_ENV_0_DECAY].setCurrentValueBasicForSetup(1000);
	h.patched().params[params::LOCAL_ENV_0_RELEASE].setCurrentValueBasicForSetup(500);
	CHECK(h.si.envelopeHasSustainCurrently(0, &h.paramManager));
}

TEST(SoundStateQuery, EnvelopeSustainCurrentlyEnvelope1) {
	SoundTestHelper h;
	// Test envelope index 1 (ENV 1, not ENV 0)
	h.patched().params[params::LOCAL_ENV_1_SUSTAIN].setCurrentValueBasicForSetup(1000);
	CHECK(h.si.envelopeHasSustainCurrently(1, &h.paramManager));
}

// ── renderingOscillatorSyncCurrently ───────────────────────────────────

TEST(SoundStateQuery, OscSyncOffByDefault) {
	SoundTestHelper h;
	CHECK(!h.si.renderingOscillatorSyncCurrently(&h.paramManager));
}

TEST(SoundStateQuery, OscSyncOnSubtractiveWithOscBVolume) {
	SoundTestHelper h;
	h.si.oscillatorSync = true;
	h.patched().params[params::LOCAL_OSC_B_VOLUME].setCurrentValueBasicForSetup(1000);
	CHECK(h.si.renderingOscillatorSyncCurrently(&h.paramManager));
}

TEST(SoundStateQuery, OscSyncOnFMReturnsFalse) {
	SoundTestHelper h;
	h.si.oscillatorSync = true;
	h.si.setSynthMode(SynthMode::FM, nullptr);
	CHECK(!h.si.renderingOscillatorSyncCurrently(&h.paramManager));
}

TEST(SoundStateQuery, OscSyncOnRingmodAlwaysTrue) {
	SoundTestHelper h;
	h.si.oscillatorSync = true;
	h.si.setSynthMode(SynthMode::RINGMOD, nullptr);
	CHECK(h.si.renderingOscillatorSyncCurrently(&h.paramManager));
}

TEST(SoundStateQuery, OscSyncOnSubtractiveOscBSilent) {
	SoundTestHelper h;
	h.si.oscillatorSync = true;
	// Osc B volume at minimum → sync not rendering
	h.patched().params[params::LOCAL_OSC_B_VOLUME].setCurrentValueBasicForSetup(-2147483648);
	CHECK(!h.si.renderingOscillatorSyncCurrently(&h.paramManager));
}

// ── isSourceActiveCurrently ────────────────────────────────────────────

TEST(SoundStateQuery, SourceActiveWithVolume) {
	SoundTestHelper h;
	h.patched().params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(1000);
	CHECK(h.si.isSourceActiveCurrently(0, &h.paramManager));
}

TEST(SoundStateQuery, SourceInactiveAtMinVolume) {
	SoundTestHelper h;
	h.patched().params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(-2147483648);
	h.si.sources[0].oscType = OscType::SQUARE; // non-sample, so loaded check irrelevant
	CHECK(!h.si.isSourceActiveCurrently(0, &h.paramManager));
}

TEST(SoundStateQuery, SourceActiveInRingmod) {
	SoundTestHelper h;
	h.si.setSynthMode(SynthMode::RINGMOD, nullptr);
	// RINGMOD makes both sources active regardless of volume
	h.patched().params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(-2147483648);
	CHECK(h.si.isSourceActiveCurrently(0, &h.paramManager));
}

TEST(SoundStateQuery, SourceInactiveSampleNotLoaded) {
	SoundTestHelper h;
	h.patched().params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(1000);
	h.si.sources[0].oscType = OscType::SAMPLE;
	// hasAtLeastOneAudioFileLoaded() returns false (stub)
	CHECK(!h.si.isSourceActiveCurrently(0, &h.paramManager));
}

TEST(SoundStateQuery, SourceActiveFMIgnoresSample) {
	SoundTestHelper h;
	h.si.setSynthMode(SynthMode::FM, nullptr);
	h.patched().params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(1000);
	h.si.sources[0].oscType = OscType::SAMPLE;
	// FM mode doesn't check hasAtLeastOneAudioFileLoaded
	CHECK(h.si.isSourceActiveCurrently(0, &h.paramManager));
}

// ── isNoiseActiveEver ──────────────────────────────────────────────────

TEST(SoundStateQuery, NoiseInactiveWhenVolumeAtMin) {
	SoundTestHelper h;
	// Noise is only inactive when noise volume param is at minimum (-2147483648)
	h.patched().params[params::LOCAL_NOISE_VOLUME].setCurrentValueBasicForSetup(-2147483648);
	CHECK(!h.si.isNoiseActiveEver(&h.paramManager));
}

TEST(SoundStateQuery, NoiseInactiveInFM) {
	SoundTestHelper h;
	h.si.setSynthMode(SynthMode::FM, nullptr);
	h.patched().params[params::LOCAL_NOISE_VOLUME].setCurrentValueBasicForSetup(1000);
	CHECK(!h.si.isNoiseActiveEver(&h.paramManager));
}

TEST(SoundStateQuery, NoiseActiveWithVolume) {
	SoundTestHelper h;
	h.patched().params[params::LOCAL_NOISE_VOLUME].setCurrentValueBasicForSetup(1000);
	CHECK(h.si.isNoiseActiveEver(&h.paramManager));
}
