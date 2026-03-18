// Deep coverage tests for ModControllableAudio methods.
//
// Tests properties and state accessible through SoundInstrument (which inherits
// ModControllableAudio via Sound). Focuses on:
// - Constructor defaults (filter modes, filter route, clipping, EQ, SRR)
// - StutterConfig defaults and field mutation
// - Delay member defaults and field mutation (pingPong, analog, syncType, syncLevel)
// - Sidechain member defaults and field mutation
// - RMSFeedbackCompressor setup/query round-trip
// - Filter mode/route defaults and direct mutation
// - Clipping amount setting
// - ModFXType transitions (all pairs via setModFXType stub)
// - cloneFrom stub behavior verification
// - ModControllable base: getKnobPosForNonExistentParam, getModKnobMode
// - EQ state fields (bassFreq, trebleFreq, withoutTreble/bassOnly zeroing)
// - sampleRateReductionOnLastTime flag
// - midi_knobs vector (empty default)
// - grainFX null default
// - Filter config and ModFXType compile-time constants
// - Two-instance state isolation

#include "CppUTest/TestHarness.h"
#include "model/model_stack.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound_instrument.h"

namespace params = deluge::modulation::params;

namespace {

// Lightweight fixture: heap-allocated SoundInstrument with no voice allocation
struct MCAFixture {
	SoundInstrument* si;

	MCAFixture() { si = new SoundInstrument; }

	~MCAFixture() {
		si->killAllVoices();
		std::erase(AudioEngine::sounds, static_cast<Sound*>(si));
	}
};

// Fixture with ParamManager wired up (needed for methods that query params)
struct MCAParamFixture {
	ParamCollectionSummary* summaries;
	PatchedParamSet* patchedParamSet;
	UnpatchedParamSet* unpatchedParamSet;
	ParamManagerForTimeline* paramManager;
	SoundInstrument* si;
	char* modelStackMemory;

	MCAParamFixture() {
		summaries = new ParamCollectionSummary[PARAM_COLLECTIONS_STORAGE_NUM]{};
		patchedParamSet = new PatchedParamSet{&summaries[1]};
		unpatchedParamSet = new UnpatchedParamSet{&summaries[0]};
		paramManager = new ParamManagerForTimeline;
		si = new SoundInstrument;
		modelStackMemory = new char[1024]{};

		paramManager->summaries[0].paramCollection = unpatchedParamSet;
		paramManager->summaries[1].paramCollection = patchedParamSet;
		unpatchedParamSet->kind = params::Kind::UNPATCHED_SOUND;
	}

	~MCAParamFixture() {
		si->killAllVoices();
		std::erase(AudioEngine::sounds, static_cast<Sound*>(si));
	}

	ModelStackWithThreeMainThings* getModelStack() {
		auto* ms = (ModelStackWithThreeMainThings*)modelStackMemory;
		ms->song = nullptr;
		ms->setTimelineCounter(nullptr);
		ms->setNoteRow(nullptr);
		ms->noteRowId = 0;
		ms->modControllable = si;
		ms->paramManager = paramManager;
		return ms;
	}
};

} // namespace

// =============================================================================
// Constructor defaults (via stub constructor)
// =============================================================================

TEST_GROUP(MCADefaults){};

TEST(MCADefaults, FilterModesDefault) {
	MCAFixture f;
	CHECK(f.si->lpfMode == FilterMode::TRANSISTOR_24DB);
	CHECK(f.si->hpfMode == FilterMode::HPLADDER);
}

TEST(MCADefaults, FilterRouteDefault) {
	MCAFixture f;
	CHECK(f.si->filterRoute == FilterRoute::HIGH_TO_LOW);
}

TEST(MCADefaults, ClippingAmountDefaultZero) {
	MCAFixture f;
	CHECK_EQUAL(0, f.si->clippingAmount);
}

TEST(MCADefaults, EQFreqsDefaultZero) {
	MCAFixture f;
	CHECK_EQUAL(0, f.si->bassFreq);
	CHECK_EQUAL(0, f.si->trebleFreq);
}

TEST(MCADefaults, EQStateFieldsDefaultZero) {
	MCAFixture f;
	CHECK_EQUAL(0, f.si->withoutTrebleL);
	CHECK_EQUAL(0, f.si->bassOnlyL);
	CHECK_EQUAL(0, f.si->withoutTrebleR);
	CHECK_EQUAL(0, f.si->bassOnlyR);
}

TEST(MCADefaults, SRROffByDefault) {
	MCAFixture f;
	CHECK(!f.si->sampleRateReductionOnLastTime);
}

TEST(MCADefaults, SampleRatePositionsDefaultZero) {
	MCAFixture f;
	CHECK_EQUAL(0u, f.si->lowSampleRatePos);
	CHECK_EQUAL(0u, f.si->highSampleRatePos);
}

TEST(MCADefaults, GrainFXNullByDefault) {
	MCAFixture f;
	POINTERS_EQUAL(nullptr, f.si->grainFX);
}

TEST(MCADefaults, MidiKnobsEmptyByDefault) {
	MCAFixture f;
	CHECK_EQUAL(0u, f.si->midi_knobs.size());
}

TEST(MCADefaults, ModFXTypeDefaultNone) {
	MCAFixture f;
	CHECK(f.si->getModFXType() == ModFXType::NONE);
}

// =============================================================================
// StutterConfig defaults and mutation
// =============================================================================

TEST_GROUP(MCAStutterConfig){};

TEST(MCAStutterConfig, DefaultQuantizedTrue) {
	MCAFixture f;
	CHECK(f.si->stutterConfig.quantized);
}

TEST(MCAStutterConfig, DefaultReversedFalse) {
	MCAFixture f;
	CHECK(!f.si->stutterConfig.reversed);
}

TEST(MCAStutterConfig, DefaultPingPongFalse) {
	MCAFixture f;
	CHECK(!f.si->stutterConfig.pingPong);
}

TEST(MCAStutterConfig, MutateQuantized) {
	MCAFixture f;
	f.si->stutterConfig.quantized = false;
	CHECK(!f.si->stutterConfig.quantized);
}

TEST(MCAStutterConfig, MutateReversed) {
	MCAFixture f;
	f.si->stutterConfig.reversed = true;
	CHECK(f.si->stutterConfig.reversed);
}

TEST(MCAStutterConfig, MutatePingPong) {
	MCAFixture f;
	f.si->stutterConfig.pingPong = true;
	CHECK(f.si->stutterConfig.pingPong);
}

TEST(MCAStutterConfig, AllFieldsMutatedTogether) {
	MCAFixture f;
	f.si->stutterConfig.quantized = false;
	f.si->stutterConfig.reversed = true;
	f.si->stutterConfig.pingPong = true;
	CHECK(!f.si->stutterConfig.quantized);
	CHECK(f.si->stutterConfig.reversed);
	CHECK(f.si->stutterConfig.pingPong);
}

// =============================================================================
// Delay member defaults and mutation
// =============================================================================

TEST_GROUP(MCADelay){};

TEST(MCADelay, PingPongDefaultTrue) {
	MCAFixture f;
	CHECK(f.si->delay.pingPong);
}

TEST(MCADelay, AnalogDefaultFalse) {
	MCAFixture f;
	CHECK(!f.si->delay.analog);
}

TEST(MCADelay, SyncTypeDefaultEven) {
	MCAFixture f;
	CHECK_EQUAL(SYNC_TYPE_EVEN, f.si->delay.syncType);
}

TEST(MCADelay, TogglePingPong) {
	MCAFixture f;
	bool original = f.si->delay.pingPong;
	f.si->delay.pingPong = !original;
	CHECK(f.si->delay.pingPong != original);
	f.si->delay.pingPong = !f.si->delay.pingPong;
	CHECK(f.si->delay.pingPong == original);
}

TEST(MCADelay, ToggleAnalog) {
	MCAFixture f;
	CHECK(!f.si->delay.analog);
	f.si->delay.analog = true;
	CHECK(f.si->delay.analog);
}

TEST(MCADelay, SyncTypeCycle) {
	MCAFixture f;
	f.si->delay.syncType = SYNC_TYPE_EVEN;
	CHECK_EQUAL(SYNC_TYPE_EVEN, f.si->delay.syncType);

	f.si->delay.syncType = SYNC_TYPE_TRIPLET;
	CHECK_EQUAL(SYNC_TYPE_TRIPLET, f.si->delay.syncType);

	f.si->delay.syncType = SYNC_TYPE_DOTTED;
	CHECK_EQUAL(SYNC_TYPE_DOTTED, f.si->delay.syncType);
}

TEST(MCADelay, SyncLevelRange) {
	MCAFixture f;
	f.si->delay.syncLevel = SYNC_LEVEL_NONE;
	CHECK_EQUAL(SYNC_LEVEL_NONE, f.si->delay.syncLevel);

	f.si->delay.syncLevel = SYNC_LEVEL_256TH;
	CHECK_EQUAL(SYNC_LEVEL_256TH, f.si->delay.syncLevel);

	f.si->delay.syncLevel = SYNC_LEVEL_WHOLE;
	CHECK_EQUAL(SYNC_LEVEL_WHOLE, f.si->delay.syncLevel);
}

TEST(MCADelay, SyncLevelDefaultIs16th) {
	MCAFixture f;
	// Delay default constructor sets syncLevel = SYNC_LEVEL_16TH
	// (the stub constructor may override, but the Delay member itself defaults to 16th)
	CHECK_EQUAL(SYNC_LEVEL_16TH, f.si->delay.syncLevel);
}

// =============================================================================
// Sidechain member defaults and mutation
// =============================================================================

TEST_GROUP(MCASidechain){};

TEST(MCASidechain, SyncTypeDefaultEven) {
	MCAFixture f;
	CHECK_EQUAL(SYNC_TYPE_EVEN, f.si->sidechain.syncType);
}

TEST(MCASidechain, AttackFieldWritable) {
	MCAFixture f;
	f.si->sidechain.attack = 12345;
	CHECK_EQUAL(12345, f.si->sidechain.attack);
}

TEST(MCASidechain, ReleaseFieldWritable) {
	MCAFixture f;
	f.si->sidechain.release = 67890;
	CHECK_EQUAL(67890, f.si->sidechain.release);
}

TEST(MCASidechain, SyncLevelWritable) {
	MCAFixture f;
	f.si->sidechain.syncLevel = SYNC_LEVEL_8TH;
	CHECK_EQUAL(SYNC_LEVEL_8TH, f.si->sidechain.syncLevel);
}

TEST(MCASidechain, PendingHitStrengthDefault) {
	MCAFixture f;
	CHECK_EQUAL(0, f.si->sidechain.pendingHitStrength);
}

// =============================================================================
// RMSFeedbackCompressor setup and queries
// The constructor calls setup with: attack=5<<24, release=5<<24, threshold=0,
// ratio=64<<24, sidechain=0, blend=ONE_Q31, baseGain=1.35
// =============================================================================

TEST_GROUP(MCACompressor){};

TEST(MCACompressor, DefaultAttackMatchesConstructor) {
	MCAFixture f;
	// Constructor: setAttack(5 << 24) = 83886080
	CHECK_EQUAL(static_cast<q31_t>(5 << 24), f.si->compressor.getAttack());
}

TEST(MCACompressor, DefaultReleaseMatchesConstructor) {
	MCAFixture f;
	CHECK_EQUAL(static_cast<q31_t>(5 << 24), f.si->compressor.getRelease());
}

TEST(MCACompressor, DefaultThresholdIsZero) {
	MCAFixture f;
	CHECK_EQUAL(0, f.si->compressor.getThreshold());
}

TEST(MCACompressor, DefaultRatioMatchesConstructor) {
	MCAFixture f;
	// Constructor: setRatio(64 << 24) = 1073741824
	CHECK_EQUAL(static_cast<q31_t>(64 << 24), f.si->compressor.getRatio());
}

TEST(MCACompressor, DefaultSidechainIsZero) {
	MCAFixture f;
	CHECK_EQUAL(0, f.si->compressor.getSidechain());
}

TEST(MCACompressor, SetAttackRoundTrip) {
	MCAFixture f;
	q31_t val = 500000000;
	f.si->compressor.setAttack(val);
	CHECK_EQUAL(val, f.si->compressor.getAttack());
}

TEST(MCACompressor, SetReleaseRoundTrip) {
	MCAFixture f;
	q31_t val = 1000000000;
	f.si->compressor.setRelease(val);
	CHECK_EQUAL(val, f.si->compressor.getRelease());
}

TEST(MCACompressor, SetThresholdRoundTrip) {
	MCAFixture f;
	q31_t val = 800000000;
	f.si->compressor.setThreshold(val);
	CHECK_EQUAL(val, f.si->compressor.getThreshold());
}

TEST(MCACompressor, SetRatioRoundTrip) {
	MCAFixture f;
	q31_t val = 1500000000;
	f.si->compressor.setRatio(val);
	CHECK_EQUAL(val, f.si->compressor.getRatio());
}

TEST(MCACompressor, SetSidechainRoundTrip) {
	MCAFixture f;
	q31_t val = 300000000;
	f.si->compressor.setSidechain(val);
	CHECK_EQUAL(val, f.si->compressor.getSidechain());
}

TEST(MCACompressor, SetBlendRoundTrip) {
	MCAFixture f;
	q31_t val = 1073741824; // ONE_Q31 / 2
	f.si->compressor.setBlend(val);
	CHECK_EQUAL(val, f.si->compressor.getBlend());
}

TEST(MCACompressor, GainReductionDefaultZero) {
	MCAFixture f;
	CHECK_EQUAL(0, f.si->compressor.gainReduction);
}

TEST(MCACompressor, GainReductionWritable) {
	MCAFixture f;
	f.si->compressor.gainReduction = 42;
	CHECK_EQUAL(42, f.si->compressor.gainReduction);
}

TEST(MCACompressor, ResetDoesNotAffectGainReduction) {
	MCAFixture f;
	// reset() clears internal state (state, er, mean, onLastTime)
	// but does NOT clear gainReduction -- it persists for display purposes
	f.si->compressor.gainReduction = 42;
	f.si->compressor.reset();
	CHECK_EQUAL(42, f.si->compressor.gainReduction);
}

TEST(MCACompressor, FullSetupRoundTrip) {
	MCAFixture f;
	q31_t a = 100000000, r = 200000000, t = 300000000;
	q31_t rat = 400000000, fc = 500000000, blend = 600000000;
	f.si->compressor.setup(a, r, t, rat, fc, blend, 1.0f);
	CHECK_EQUAL(a, f.si->compressor.getAttack());
	CHECK_EQUAL(r, f.si->compressor.getRelease());
	CHECK_EQUAL(t, f.si->compressor.getThreshold());
	CHECK_EQUAL(rat, f.si->compressor.getRatio());
	CHECK_EQUAL(fc, f.si->compressor.getSidechain());
	CHECK_EQUAL(blend, f.si->compressor.getBlend());
}

TEST(MCACompressor, SetAttackTwiceOverwrites) {
	MCAFixture f;
	f.si->compressor.setAttack(100000000);
	f.si->compressor.setAttack(999999999);
	CHECK_EQUAL(999999999, f.si->compressor.getAttack());
}

TEST(MCACompressor, SetRatioZero) {
	MCAFixture f;
	f.si->compressor.setRatio(0);
	CHECK_EQUAL(0, f.si->compressor.getRatio());
}

TEST(MCACompressor, SetBlendZeroForFullDry) {
	MCAFixture f;
	f.si->compressor.setBlend(0);
	CHECK_EQUAL(0, f.si->compressor.getBlend());
}

// =============================================================================
// Filter mode and route mutation
// =============================================================================

TEST_GROUP(MCAFilters){};

TEST(MCAFilters, LPFModeMutable) {
	MCAFixture f;
	f.si->lpfMode = FilterMode::TRANSISTOR_12DB;
	CHECK(f.si->lpfMode == FilterMode::TRANSISTOR_12DB);

	f.si->lpfMode = FilterMode::TRANSISTOR_24DB_DRIVE;
	CHECK(f.si->lpfMode == FilterMode::TRANSISTOR_24DB_DRIVE);

	f.si->lpfMode = FilterMode::SVF_BAND;
	CHECK(f.si->lpfMode == FilterMode::SVF_BAND);

	f.si->lpfMode = FilterMode::SVF_NOTCH;
	CHECK(f.si->lpfMode == FilterMode::SVF_NOTCH);
}

TEST(MCAFilters, HPFModeMutable) {
	MCAFixture f;
	f.si->hpfMode = FilterMode::HPLADDER;
	CHECK(f.si->hpfMode == FilterMode::HPLADDER);

	f.si->hpfMode = FilterMode::SVF_BAND;
	CHECK(f.si->hpfMode == FilterMode::SVF_BAND);

	f.si->hpfMode = FilterMode::SVF_NOTCH;
	CHECK(f.si->hpfMode == FilterMode::SVF_NOTCH);
}

TEST(MCAFilters, FilterRouteAllValues) {
	MCAFixture f;
	f.si->filterRoute = FilterRoute::HIGH_TO_LOW;
	CHECK(f.si->filterRoute == FilterRoute::HIGH_TO_LOW);

	f.si->filterRoute = FilterRoute::LOW_TO_HIGH;
	CHECK(f.si->filterRoute == FilterRoute::LOW_TO_HIGH);

	f.si->filterRoute = FilterRoute::PARALLEL;
	CHECK(f.si->filterRoute == FilterRoute::PARALLEL);
}

TEST(MCAFilters, FilterModeOffSentinel) {
	MCAFixture f;
	f.si->lpfMode = FilterMode::OFF;
	CHECK(f.si->lpfMode == FilterMode::OFF);
}

TEST(MCAFilters, AllFilterModesSettable) {
	MCAFixture f;
	FilterMode modes[] = {
	    FilterMode::TRANSISTOR_12DB, FilterMode::TRANSISTOR_24DB, FilterMode::TRANSISTOR_24DB_DRIVE,
	    FilterMode::SVF_BAND,        FilterMode::SVF_NOTCH,       FilterMode::HPLADDER,
	    FilterMode::OFF,
	};
	for (auto mode : modes) {
		f.si->lpfMode = mode;
		CHECK(f.si->lpfMode == mode);
	}
}

// =============================================================================
// Clipping amount
// =============================================================================

TEST_GROUP(MCAClipping){};

TEST(MCAClipping, SetAndQuery) {
	MCAFixture f;
	f.si->clippingAmount = 0;
	CHECK_EQUAL(0, f.si->clippingAmount);

	f.si->clippingAmount = 128;
	CHECK_EQUAL(128, f.si->clippingAmount);

	f.si->clippingAmount = 255;
	CHECK_EQUAL(255, f.si->clippingAmount);
}

TEST(MCAClipping, FullRange) {
	MCAFixture f;
	// uint8_t range: 0 to 255
	for (int i = 0; i <= 255; i += 51) {
		f.si->clippingAmount = static_cast<uint8_t>(i);
		CHECK_EQUAL(i, f.si->clippingAmount);
	}
}

// =============================================================================
// ModFXType transitions
// =============================================================================

TEST_GROUP(MCAModFX){};

TEST(MCAModFX, DefaultIsNone) {
	MCAFixture f;
	CHECK(f.si->getModFXType() == ModFXType::NONE);
}

TEST(MCAModFX, SetToFlanger) {
	MCAFixture f;
	bool ok = f.si->setModFXType(ModFXType::FLANGER);
	CHECK(ok);
	CHECK(f.si->getModFXType() == ModFXType::FLANGER);
}

TEST(MCAModFX, SetToChorus) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::CHORUS);
	CHECK(f.si->getModFXType() == ModFXType::CHORUS);
}

TEST(MCAModFX, SetToPhaser) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::PHASER);
	CHECK(f.si->getModFXType() == ModFXType::PHASER);
}

TEST(MCAModFX, SetToChorusStereo) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::CHORUS_STEREO);
	CHECK(f.si->getModFXType() == ModFXType::CHORUS_STEREO);
}

TEST(MCAModFX, SetToWarble) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::WARBLE);
	CHECK(f.si->getModFXType() == ModFXType::WARBLE);
}

TEST(MCAModFX, SetToDimension) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::DIMENSION);
	CHECK(f.si->getModFXType() == ModFXType::DIMENSION);
}

TEST(MCAModFX, SetToGrain) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::GRAIN);
	CHECK(f.si->getModFXType() == ModFXType::GRAIN);
}

TEST(MCAModFX, TransitionFlangerToPhaser) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::FLANGER);
	CHECK(f.si->getModFXType() == ModFXType::FLANGER);
	f.si->setModFXType(ModFXType::PHASER);
	CHECK(f.si->getModFXType() == ModFXType::PHASER);
}

TEST(MCAModFX, TransitionGrainToNone) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::GRAIN);
	CHECK(f.si->getModFXType() == ModFXType::GRAIN);
	f.si->setModFXType(ModFXType::NONE);
	CHECK(f.si->getModFXType() == ModFXType::NONE);
}

TEST(MCAModFX, CycleAllTypes) {
	MCAFixture f;
	ModFXType types[] = {
	    ModFXType::NONE,          ModFXType::FLANGER, ModFXType::CHORUS,    ModFXType::PHASER,
	    ModFXType::CHORUS_STEREO, ModFXType::WARBLE,  ModFXType::DIMENSION, ModFXType::GRAIN,
	};
	for (auto type : types) {
		f.si->setModFXType(type);
		CHECK(f.si->getModFXType() == type);
	}
}

TEST(MCAModFX, SetSameTypeTwice) {
	MCAFixture f;
	f.si->setModFXType(ModFXType::WARBLE);
	f.si->setModFXType(ModFXType::WARBLE);
	CHECK(f.si->getModFXType() == ModFXType::WARBLE);
}

TEST(MCAModFX, RapidCycleDoesNotCorrupt) {
	MCAFixture f;
	// Rapidly transition through all types multiple times
	for (int round = 0; round < 3; round++) {
		for (uint8_t t = 0; t < kNumModFXTypes; t++) {
			f.si->setModFXType(static_cast<ModFXType>(t));
			CHECK(f.si->getModFXType() == static_cast<ModFXType>(t));
		}
	}
}

TEST(MCAModFX, ModFXTypeFieldDirectAccess) {
	MCAFixture f;
	// modFXType_ is a public member, verify setModFXType updates it
	f.si->setModFXType(ModFXType::DIMENSION);
	CHECK(f.si->modFXType_ == ModFXType::DIMENSION);
}

// =============================================================================
// cloneFrom stub verification
// cloneFrom is stubbed as a no-op in the test harness, so we verify the stub
// behavior: destination fields should NOT change after cloneFrom.
// =============================================================================

TEST_GROUP(MCACloneFromStub){};

TEST(MCACloneFromStub, StubDoesNotCopyFilterModes) {
	MCAFixture src, dst;
	src.si->lpfMode = FilterMode::SVF_NOTCH;
	FilterMode dstOriginal = dst.si->lpfMode;
	dst.si->cloneFrom(src.si);
	// Stub is a no-op, so dst retains its original value
	CHECK(dst.si->lpfMode == dstOriginal);
}

TEST(MCACloneFromStub, StubDoesNotCopyClipping) {
	MCAFixture src, dst;
	src.si->clippingAmount = 200;
	uint8_t dstOriginal = dst.si->clippingAmount;
	dst.si->cloneFrom(src.si);
	CHECK_EQUAL(dstOriginal, dst.si->clippingAmount);
}

TEST(MCACloneFromStub, StubDoesNotCopyModFXType) {
	MCAFixture src, dst;
	src.si->setModFXType(ModFXType::DIMENSION);
	ModFXType dstOriginal = dst.si->getModFXType();
	dst.si->cloneFrom(src.si);
	CHECK(dst.si->getModFXType() == dstOriginal);
}

// =============================================================================
// EQ state fields mutation
// =============================================================================

TEST_GROUP(MCAEQ){};

TEST(MCAEQ, BassFreqMutable) {
	MCAFixture f;
	f.si->bassFreq = 120000000;
	CHECK_EQUAL(120000000, f.si->bassFreq);
}

TEST(MCAEQ, TrebleFreqMutable) {
	MCAFixture f;
	f.si->trebleFreq = 700000000;
	CHECK_EQUAL(700000000, f.si->trebleFreq);
}

TEST(MCAEQ, WithoutTrebleLMutable) {
	MCAFixture f;
	f.si->withoutTrebleL = 12345;
	CHECK_EQUAL(12345, f.si->withoutTrebleL);
}

TEST(MCAEQ, BassOnlyLMutable) {
	MCAFixture f;
	f.si->bassOnlyL = -54321;
	CHECK_EQUAL(-54321, f.si->bassOnlyL);
}

TEST(MCAEQ, WithoutTrebleRMutable) {
	MCAFixture f;
	f.si->withoutTrebleR = 99999;
	CHECK_EQUAL(99999, f.si->withoutTrebleR);
}

TEST(MCAEQ, BassOnlyRMutable) {
	MCAFixture f;
	f.si->bassOnlyR = -11111;
	CHECK_EQUAL(-11111, f.si->bassOnlyR);
}

// =============================================================================
// SRR flag
// =============================================================================

TEST_GROUP(MCASRR){};

TEST(MCASRR, FlagToggles) {
	MCAFixture f;
	CHECK(!f.si->sampleRateReductionOnLastTime);
	f.si->sampleRateReductionOnLastTime = true;
	CHECK(f.si->sampleRateReductionOnLastTime);
	f.si->sampleRateReductionOnLastTime = false;
	CHECK(!f.si->sampleRateReductionOnLastTime);
}

// =============================================================================
// ModControllable base class methods (real, not stubbed)
// =============================================================================

TEST_GROUP(MCABaseClass){};

TEST(MCABaseClass, GetKnobPosForNonExistentParamReturnsMinus64) {
	MCAFixture f;
	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms = (ModelStackWithAutoParam*)mem;

	int32_t result = f.si->getKnobPosForNonExistentParam(0, ms);
	CHECK_EQUAL(-64, result);
}

TEST(MCABaseClass, GetKnobPosForNonExistentParamDifferentEncoders) {
	MCAFixture f;
	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms = (ModelStackWithAutoParam*)mem;

	// Should return -64 regardless of which encoder
	for (int i = 0; i < 4; i++) {
		CHECK_EQUAL(-64, f.si->getKnobPosForNonExistentParam(i, ms));
	}
}

TEST(MCABaseClass, GetModKnobModeReturnsNonNull) {
	MCAFixture f;
	// SoundInstrument::getModKnobMode() returns &modKnobMode (a member field)
	uint8_t* mode = f.si->getModKnobMode();
	CHECK(mode != nullptr);
}

TEST(MCABaseClass, GetModKnobModeReturnsWritablePointer) {
	MCAFixture f;
	uint8_t* mode = f.si->getModKnobMode();
	CHECK(mode != nullptr);
	// Should be writable and reflect back
	*mode = 3;
	CHECK_EQUAL(3, *f.si->getModKnobMode());
}

TEST(MCABaseClass, GetParamFromModEncoderSoundStubReturnsNull) {
	MCAFixture f;
	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms3 = (ModelStackWithThreeMainThings*)mem;

	// Sound::getParamFromModEncoder is stubbed to return nullptr
	ModelStackWithAutoParam* result = f.si->getParamFromModEncoder(0, ms3, false);
	POINTERS_EQUAL(nullptr, result);
}

// =============================================================================
// Stub method coverage (verify they link and don't crash)
// =============================================================================

TEST_GROUP(MCAStubs){};

TEST(MCAStubs, BeginEndStutterDontCrash) {
	MCAParamFixture f;
	f.si->beginStutter(f.paramManager);
	f.si->endStutter(f.paramManager);
}

TEST(MCAStubs, WontBeRenderedForAWhileDontCrash) {
	MCAFixture f;
	f.si->wontBeRenderedForAWhile();
}

TEST(MCAStubs, ModFXProcessorResetMemoryDontCrash) {
	MCAFixture f;
	f.si->modfx.resetMemory();
}

TEST(MCAStubs, DelayFieldMutationDoesntCrash) {
	MCAFixture f;
	// Protected switch methods are stubbed; verify delay fields directly
	bool orig = f.si->delay.pingPong;
	f.si->delay.pingPong = !orig;
	CHECK(f.si->delay.pingPong != orig);
}

// =============================================================================
// Two-instance isolation (ensure separate SoundInstruments are independent)
// =============================================================================

TEST_GROUP(MCAIsolation){};

TEST(MCAIsolation, TwoInstancesIndependentFilterMode) {
	MCAFixture a, b;
	a.si->lpfMode = FilterMode::SVF_NOTCH;
	b.si->lpfMode = FilterMode::TRANSISTOR_12DB;
	CHECK(a.si->lpfMode == FilterMode::SVF_NOTCH);
	CHECK(b.si->lpfMode == FilterMode::TRANSISTOR_12DB);
}

TEST(MCAIsolation, TwoInstancesIndependentModFX) {
	MCAFixture a, b;
	a.si->setModFXType(ModFXType::GRAIN);
	b.si->setModFXType(ModFXType::FLANGER);
	CHECK(a.si->getModFXType() == ModFXType::GRAIN);
	CHECK(b.si->getModFXType() == ModFXType::FLANGER);
}

TEST(MCAIsolation, TwoInstancesIndependentClipping) {
	MCAFixture a, b;
	a.si->clippingAmount = 100;
	b.si->clippingAmount = 200;
	CHECK_EQUAL(100, a.si->clippingAmount);
	CHECK_EQUAL(200, b.si->clippingAmount);
}

TEST(MCAIsolation, TwoInstancesIndependentDelay) {
	MCAFixture a, b;
	a.si->delay.analog = true;
	b.si->delay.analog = false;
	CHECK(a.si->delay.analog);
	CHECK(!b.si->delay.analog);
}

TEST(MCAIsolation, TwoInstancesIndependentCompressor) {
	MCAFixture a, b;
	a.si->compressor.setThreshold(999999999);
	b.si->compressor.setThreshold(111111111);
	CHECK_EQUAL(999999999, a.si->compressor.getThreshold());
	CHECK_EQUAL(111111111, b.si->compressor.getThreshold());
}

TEST(MCAIsolation, TwoInstancesIndependentStutter) {
	MCAFixture a, b;
	a.si->stutterConfig.reversed = true;
	b.si->stutterConfig.reversed = false;
	CHECK(a.si->stutterConfig.reversed);
	CHECK(!b.si->stutterConfig.reversed);
}

// =============================================================================
// Filter config constants (compile-time correctness)
// =============================================================================

TEST_GROUP(MCAFilterConstants){};

TEST(MCAFilterConstants, NumLPFModesCorrect) {
	// LPF modes: TRANSISTOR_12DB(0), TRANSISTOR_24DB(1), TRANSISTOR_24DB_DRIVE(2), SVF_BAND(3), SVF_NOTCH(4) = 5
	CHECK_EQUAL(5, kNumLPFModes);
}

TEST(MCAFilterConstants, NumHPFModesCorrect) {
	// HPF modes: SVF_BAND(3), SVF_NOTCH(4), HPLADDER(5) => OFF(6) - SVF_BAND(3) = 3
	CHECK_EQUAL(3, kNumHPFModes);
}

TEST(MCAFilterConstants, FirstHPFModeValue) {
	CHECK_EQUAL(3, kFirstHPFMode); // SVF_BAND
}

TEST(MCAFilterConstants, NumFilterRoutes) {
	CHECK_EQUAL(3, kNumFilterRoutes);
}

TEST(MCAFilterConstants, LastLpfModeIsSVFNotch) {
	CHECK(lastLpfMode == FilterMode::SVF_NOTCH);
}

TEST(MCAFilterConstants, FirstHPFModeIsSVFBand) {
	CHECK(firstHPFMode == FilterMode::SVF_BAND);
}

// =============================================================================
// ModFXType constants
// =============================================================================

TEST_GROUP(MCAModFXConstants){};

TEST(MCAModFXConstants, NumModFXTypesIs8) {
	CHECK_EQUAL(8, kNumModFXTypes);
}

TEST(MCAModFXConstants, NoneIsZero) {
	CHECK_EQUAL(0, static_cast<uint8_t>(ModFXType::NONE));
}

TEST(MCAModFXConstants, GrainIsLast) {
	CHECK_EQUAL(7, static_cast<uint8_t>(ModFXType::GRAIN));
}

TEST(MCAModFXConstants, EnumValuesContiguous) {
	// Verify all ModFXType values are contiguous 0..7
	CHECK_EQUAL(0, static_cast<uint8_t>(ModFXType::NONE));
	CHECK_EQUAL(1, static_cast<uint8_t>(ModFXType::FLANGER));
	CHECK_EQUAL(2, static_cast<uint8_t>(ModFXType::CHORUS));
	CHECK_EQUAL(3, static_cast<uint8_t>(ModFXType::PHASER));
	CHECK_EQUAL(4, static_cast<uint8_t>(ModFXType::CHORUS_STEREO));
	CHECK_EQUAL(5, static_cast<uint8_t>(ModFXType::WARBLE));
	CHECK_EQUAL(6, static_cast<uint8_t>(ModFXType::DIMENSION));
	CHECK_EQUAL(7, static_cast<uint8_t>(ModFXType::GRAIN));
}
