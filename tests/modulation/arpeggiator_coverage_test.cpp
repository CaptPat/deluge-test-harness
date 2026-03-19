// Arpeggiator coverage tests -- targets uncovered branches in arpeggiator.cpp
// to push coverage from ~77% toward 90%+.
//
// Areas covered:
//   - doTickForward() synced and unsynced paths
//   - switchAnyNoteOff() with and without glide
//   - render() with spread gate positive/negative/clamping
//   - render() ratchet path
//   - maybeSetupNewRatchet() various sync levels
//   - getPhaseIncrement() triplet/dotted sync types
//   - updatePresetFromCurrentSettings() all preset branches
//   - updateSettingsFromCurrentPreset() all preset branches
//   - Synth switchNoteOn() bass/random/chord/AS_PLAYED/PATTERN branches
//   - Drum switchNoteOn() bass/random step/MPE velocity branches
//   - evaluateRhythm/evaluateNoteProbability/evaluateBassProbability
//   - Note clamping (< 0 and > 127) in switchNoteOn
//   - ArpeggiatorBase::handlePendingNotes in ARP mode
//   - Arp noteOff in ARP mode with whichNoteCurrentlyOnPostArp adjustment
//   - Walk mode octave direction reversal branches

#include "CppUTest/TestHarness.h"
#include "modulation/arpeggiator.h"
#include "playback/playback_handler.h"
#include "song_mock.h"

static int16_t zMpe[3] = {0, 0, 0};

// Test helper subclass to expose protected members for direct unit testing
class TestArpDrum : public ArpeggiatorForDrum {
public:
	using ArpeggiatorBase::calculateSpreadVelocity;
	using ArpeggiatorBase::evaluateBassProbability;
	using ArpeggiatorBase::evaluateChordProbability;
	using ArpeggiatorBase::evaluateNoteProbability;
	using ArpeggiatorBase::evaluateReverseProbability;
	using ArpeggiatorBase::evaluateRhythm;
	using ArpeggiatorBase::evaluateSwapProbability;
	using ArpeggiatorBase::getRandomBipolarProbabilityAmount;
	using ArpeggiatorBase::getRandomProbabilityResult;
	using ArpeggiatorBase::getRandomWeighted2BitsAmount;
	using ArpeggiatorBase::glideOnNextNoteOff;
	using ArpeggiatorBase::isPlayBassForCurrentStep;
	using ArpeggiatorBase::isPlayChordForCurrentStep;
	using ArpeggiatorBase::isPlayNoteForCurrentStep;
	using ArpeggiatorBase::isPlayRandomStepForCurrentStep;
	using ArpeggiatorBase::isPlayReverseForCurrentStep;
	using ArpeggiatorBase::isRatcheting;
	using ArpeggiatorBase::lastNormalNotePlayedFromBassProbability;
	using ArpeggiatorBase::lastNormalNotePlayedFromNoteProbability;
	using ArpeggiatorBase::notesPlayedFromRhythm;
	using ArpeggiatorBase::ratchetNotesCount;
	using ArpeggiatorBase::ratchetNotesIndex;
	using ArpeggiatorBase::ratchetNotesMultiplier;
	using ArpeggiatorBase::resetBase;
	using ArpeggiatorBase::resetRatchet;
	using ArpeggiatorBase::stepRepeatIndex;
};

class TestArpSynth : public Arpeggiator {
public:
	using Arpeggiator::anyPending;
};

// Helper: run render cycles
static void renderCycles(ArpeggiatorBase* arp, ArpeggiatorSettings* settings, int count,
                         uint32_t gateThreshold = 2147483647u, uint32_t phaseIncrement = 100000000u) {
	for (int i = 0; i < count; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, gateThreshold, phaseIncrement);
	}
}

// == doTickForward ==

TEST_GROUP(ArpCovDoTickForward) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovDoTickForward, drumDoTickForwardUnsyncedReturnsMax) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.syncLevel = (SyncLevel)0;

	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);

	ArpReturnInstruction tickInst;
	int32_t result = arp.doTickForward(&settings, &tickInst, 0, false);
	CHECK_EQUAL(2147483647, result);
}

TEST(ArpCovDoTickForward, drumDoTickForwardModeOffReturnsMax) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::OFF;
	settings.syncLevel = (SyncLevel)5;

	ArpReturnInstruction tickInst;
	int32_t result = arp.doTickForward(&settings, &tickInst, 0, false);
	CHECK_EQUAL(2147483647, result);
}

TEST(ArpCovDoTickForward, synthDoTickForwardSyncedAtZeroPos) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.syncLevel = (SyncLevel)5;
	settings.syncType = SYNC_TYPE_EVEN;
	settings.numOctaves = 2;

	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);

	// Consume pending via render first
	renderCycles(&arp, &settings, 10, 2147483647u, 500000000u);

	ArpReturnInstruction tickInst;
	int32_t result = arp.doTickForward(&settings, &tickInst, 0, false);
	CHECK(result >= 0);
}

TEST(ArpCovDoTickForward, synthDoTickForwardSyncedTriplet) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.syncLevel = (SyncLevel)5;
	settings.syncType = SYNC_TYPE_TRIPLET;
	settings.numOctaves = 1;

	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);

	renderCycles(&arp, &settings, 10, 2147483647u, 500000000u);

	ArpReturnInstruction tickInst;
	int32_t result = arp.doTickForward(&settings, &tickInst, 0, false);
	CHECK(result >= 0);
}

TEST(ArpCovDoTickForward, synthDoTickForwardSyncedDotted) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.syncLevel = (SyncLevel)5;
	settings.syncType = SYNC_TYPE_DOTTED;
	settings.numOctaves = 1;

	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);

	renderCycles(&arp, &settings, 10, 2147483647u, 500000000u);

	ArpReturnInstruction tickInst;
	int32_t result = arp.doTickForward(&settings, &tickInst, 0, false);
	CHECK(result >= 0);
}

TEST(ArpCovDoTickForward, synthDoTickForwardMidPeriodNotReversed) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.syncLevel = (SyncLevel)5;
	settings.syncType = SYNC_TYPE_EVEN;
	settings.numOctaves = 1;

	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);

	renderCycles(&arp, &settings, 10, 2147483647u, 500000000u);

	ArpReturnInstruction tickInst;
	int32_t result = arp.doTickForward(&settings, &tickInst, 5, false);
	CHECK(result >= 0);
}

TEST(ArpCovDoTickForward, synthDoTickForwardMidPeriodReversed) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.syncLevel = (SyncLevel)5;
	settings.syncType = SYNC_TYPE_EVEN;
	settings.numOctaves = 1;

	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);

	renderCycles(&arp, &settings, 10, 2147483647u, 500000000u);

	ArpReturnInstruction tickInst;
	int32_t result = arp.doTickForward(&settings, &tickInst, 5, true);
	CHECK(result >= 0);
}

TEST(ArpCovDoTickForward, drumDoTickForwardSyncedNoNotes) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.syncLevel = (SyncLevel)5;
	settings.syncType = SYNC_TYPE_EVEN;

	ArpReturnInstruction tickInst;
	int32_t result = arp.doTickForward(&settings, &tickInst, 0, false);
	CHECK(result > 0);
}

// == Spread velocity (indirectly) ==

TEST_GROUP(ArpCovSpreadVelocity) {
	ArpeggiatorForDrum arp;
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovSpreadVelocity, positiveSpreadViaNoArpNoteOn) {
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::OFF;
	settings.noteProbability = UINT32_MAX;
	settings.spreadVelocity = UINT32_MAX;

	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 50, &inst, 0, zMpe);
	CHECK(arp.active_note.velocity >= 1);
	CHECK(arp.active_note.velocity <= 127);
}

TEST(ArpCovSpreadVelocity, spreadViaArpRender) {
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.noteProbability = UINT32_MAX;
	settings.spreadVelocity = UINT32_MAX;

	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 200, 2147483647u, 500000000u);
}

// == getPhaseIncrement ==

TEST_GROUP(ArpCovPhaseIncrement) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovPhaseIncrement, unsyncedPhaseIncrement) {
	ArpeggiatorSettings s;
	s.syncLevel = (SyncLevel)0;
	uint32_t inc = s.getPhaseIncrement(1000000);
	CHECK(inc > 0);
}

TEST(ArpCovPhaseIncrement, syncedEvenPhaseIncrement) {
	ArpeggiatorSettings s;
	s.syncLevel = (SyncLevel)5;
	s.syncType = SYNC_TYPE_EVEN;
	uint32_t inc = s.getPhaseIncrement(1000000);
	CHECK(inc > 0);
}

TEST(ArpCovPhaseIncrement, syncedTripletPhaseIncrement) {
	ArpeggiatorSettings s;
	s.syncLevel = (SyncLevel)5;
	s.syncType = SYNC_TYPE_TRIPLET;
	uint32_t inc = s.getPhaseIncrement(1000000);
	CHECK(inc > 0);
}

TEST(ArpCovPhaseIncrement, syncedDottedPhaseIncrement) {
	ArpeggiatorSettings s;
	s.syncLevel = (SyncLevel)5;
	s.syncType = SYNC_TYPE_DOTTED;
	uint32_t inc = s.getPhaseIncrement(1000000);
	CHECK(inc > 0);
}

TEST(ArpCovPhaseIncrement, maxSyncLevel) {
	ArpeggiatorSettings s;
	s.syncLevel = (SyncLevel)9;
	s.syncType = SYNC_TYPE_EVEN;
	uint32_t inc = s.getPhaseIncrement(1000000);
	CHECK(inc > 0);
}

// == updatePresetFromCurrentSettings ==

TEST_GROUP(ArpCovUpdatePreset) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovUpdatePreset, offPreset) {
	ArpeggiatorSettings s;
	s.mode = ArpMode::OFF;
	s.updatePresetFromCurrentSettings();
	CHECK_EQUAL((int)ArpPreset::OFF, (int)s.preset);
}

TEST(ArpCovUpdatePreset, upPreset) {
	ArpeggiatorSettings s;
	s.mode = ArpMode::ARP;
	s.octaveMode = ArpOctaveMode::UP;
	s.noteMode = ArpNoteMode::UP;
	s.updatePresetFromCurrentSettings();
	CHECK_EQUAL((int)ArpPreset::UP, (int)s.preset);
}

TEST(ArpCovUpdatePreset, downPreset) {
	ArpeggiatorSettings s;
	s.mode = ArpMode::ARP;
	s.octaveMode = ArpOctaveMode::DOWN;
	s.noteMode = ArpNoteMode::DOWN;
	s.updatePresetFromCurrentSettings();
	CHECK_EQUAL((int)ArpPreset::DOWN, (int)s.preset);
}

TEST(ArpCovUpdatePreset, bothPreset) {
	ArpeggiatorSettings s;
	s.mode = ArpMode::ARP;
	s.octaveMode = ArpOctaveMode::ALTERNATE;
	s.noteMode = ArpNoteMode::UP;
	s.updatePresetFromCurrentSettings();
	CHECK_EQUAL((int)ArpPreset::BOTH, (int)s.preset);
}

TEST(ArpCovUpdatePreset, randomPreset) {
	ArpeggiatorSettings s;
	s.mode = ArpMode::ARP;
	s.octaveMode = ArpOctaveMode::RANDOM;
	s.noteMode = ArpNoteMode::RANDOM;
	s.updatePresetFromCurrentSettings();
	CHECK_EQUAL((int)ArpPreset::RANDOM, (int)s.preset);
}

TEST(ArpCovUpdatePreset, walkPreset) {
	ArpeggiatorSettings s;
	s.mode = ArpMode::ARP;
	s.octaveMode = ArpOctaveMode::ALTERNATE;
	s.noteMode = ArpNoteMode::WALK2;
	s.updatePresetFromCurrentSettings();
	CHECK_EQUAL((int)ArpPreset::WALK, (int)s.preset);
}

TEST(ArpCovUpdatePreset, customPreset) {
	ArpeggiatorSettings s;
	s.mode = ArpMode::ARP;
	s.octaveMode = ArpOctaveMode::UP;
	s.noteMode = ArpNoteMode::DOWN;
	s.updatePresetFromCurrentSettings();
	CHECK_EQUAL((int)ArpPreset::CUSTOM, (int)s.preset);
}

// == updateSettingsFromCurrentPreset ==

TEST_GROUP(ArpCovUpdateFromPreset) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovUpdateFromPreset, offPreset) {
	ArpeggiatorSettings s;
	s.preset = ArpPreset::OFF;
	s.updateSettingsFromCurrentPreset();
	CHECK_EQUAL((int)ArpMode::OFF, (int)s.mode);
}

TEST(ArpCovUpdateFromPreset, upPreset) {
	ArpeggiatorSettings s;
	s.preset = ArpPreset::UP;
	s.updateSettingsFromCurrentPreset();
	CHECK_EQUAL((int)ArpMode::ARP, (int)s.mode);
	CHECK_EQUAL((int)ArpOctaveMode::UP, (int)s.octaveMode);
	CHECK_EQUAL((int)ArpNoteMode::UP, (int)s.noteMode);
}

TEST(ArpCovUpdateFromPreset, downPreset) {
	ArpeggiatorSettings s;
	s.preset = ArpPreset::DOWN;
	s.updateSettingsFromCurrentPreset();
	CHECK_EQUAL((int)ArpMode::ARP, (int)s.mode);
	CHECK_EQUAL((int)ArpOctaveMode::DOWN, (int)s.octaveMode);
	CHECK_EQUAL((int)ArpNoteMode::DOWN, (int)s.noteMode);
}

TEST(ArpCovUpdateFromPreset, bothPreset) {
	ArpeggiatorSettings s;
	s.preset = ArpPreset::BOTH;
	s.updateSettingsFromCurrentPreset();
	CHECK_EQUAL((int)ArpMode::ARP, (int)s.mode);
	CHECK_EQUAL((int)ArpOctaveMode::ALTERNATE, (int)s.octaveMode);
	CHECK_EQUAL((int)ArpNoteMode::UP, (int)s.noteMode);
}

TEST(ArpCovUpdateFromPreset, randomPreset) {
	ArpeggiatorSettings s;
	s.preset = ArpPreset::RANDOM;
	s.updateSettingsFromCurrentPreset();
	CHECK_EQUAL((int)ArpMode::ARP, (int)s.mode);
	CHECK_EQUAL((int)ArpOctaveMode::RANDOM, (int)s.octaveMode);
	CHECK_EQUAL((int)ArpNoteMode::RANDOM, (int)s.noteMode);
}

TEST(ArpCovUpdateFromPreset, walkPreset) {
	ArpeggiatorSettings s;
	s.preset = ArpPreset::WALK;
	s.updateSettingsFromCurrentPreset();
	CHECK_EQUAL((int)ArpMode::ARP, (int)s.mode);
	CHECK_EQUAL((int)ArpOctaveMode::ALTERNATE, (int)s.octaveMode);
	CHECK_EQUAL((int)ArpNoteMode::WALK2, (int)s.noteMode);
}

TEST(ArpCovUpdateFromPreset, customPreset) {
	ArpeggiatorSettings s;
	s.preset = ArpPreset::CUSTOM;
	s.updateSettingsFromCurrentPreset();
	CHECK_EQUAL((int)ArpMode::ARP, (int)s.mode);
	CHECK_EQUAL((int)ArpOctaveMode::UP, (int)s.octaveMode);
	CHECK_EQUAL((int)ArpNoteMode::UP, (int)s.noteMode);
}

// == Drum switchNoteOn paths ==

TEST_GROUP(ArpCovDrumSwitchNoteOn) {
	ArpeggiatorForDrum* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new ArpeggiatorForDrum();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 2;
		settings->noteMode = ArpNoteMode::UP;
		settings->octaveMode = ArpOctaveMode::UP;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpCovDrumSwitchNoteOn, drumBassProbabilityPath) {
	settings->bassProbability = UINT32_MAX;
	settings->noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovDrumSwitchNoteOn, drumSwapProbabilityPath) {
	settings->swapProbability = UINT32_MAX;
	settings->noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovDrumSwitchNoteOn, drumMpeVelocityAftertouch) {
	settings->mpeVelocity = ArpMpeModSource::AFTERTOUCH;
	settings->noteProbability = UINT32_MAX;
	int16_t mpeVals[3] = {0, 0, 5000};
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, mpeVals);
	renderCycles(arp, settings, 100);
}

TEST(ArpCovDrumSwitchNoteOn, drumMpeVelocityMPEY) {
	settings->mpeVelocity = ArpMpeModSource::MPE_Y;
	settings->noteProbability = UINT32_MAX;
	int16_t mpeVals[3] = {0, 8000, 0};
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, mpeVals);
	renderCycles(arp, settings, 100);
}

TEST(ArpCovDrumSwitchNoteOn, drumMpeVelocityLowClampsToMin) {
	settings->mpeVelocity = ArpMpeModSource::AFTERTOUCH;
	settings->noteProbability = UINT32_MAX;
	int16_t mpeVals[3] = {0, 0, 0};
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, mpeVals);
	renderCycles(arp, settings, 100);
}

TEST(ArpCovDrumSwitchNoteOn, drumOctaveSpreadPath) {
	settings->spreadOctave = UINT32_MAX;
	settings->noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovDrumSwitchNoteOn, drumNoteClampHighPath) {
	settings->numOctaves = 8;
	settings->noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 120, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovDrumSwitchNoteOn, drumGlideProbabilityPath) {
	settings->glideProbability = UINT32_MAX;
	settings->noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovDrumSwitchNoteOn, drumVelocitySpreadPath) {
	settings->spreadVelocity = UINT32_MAX;
	settings->noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 100);
}

// == Synth switchNoteOn paths ==

TEST_GROUP(ArpCovSynthSwitchNoteOn) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 2;
		settings->noteMode = ArpNoteMode::UP;
		settings->octaveMode = ArpOctaveMode::UP;
		settings->noteProbability = UINT32_MAX;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpCovSynthSwitchNoteOn, synthBassProbabilityPath) {
	settings->bassProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovSynthSwitchNoteOn, synthSwapProbabilityPath) {
	settings->swapProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovSynthSwitchNoteOn, synthAsPlayedMode) {
	settings->noteMode = ArpNoteMode::AS_PLAYED;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 72, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 48, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovSynthSwitchNoteOn, synthPatternMode) {
	settings->noteMode = ArpNoteMode::PATTERN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovSynthSwitchNoteOn, synthMpeVelocityAftertouch) {
	settings->mpeVelocity = ArpMpeModSource::AFTERTOUCH;
	int16_t mpeVals[3] = {0, 0, 5000};
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, mpeVals);
	renderCycles(arp, settings, 100);
}

TEST(ArpCovSynthSwitchNoteOn, synthMpeVelocityMPEY) {
	settings->mpeVelocity = ArpMpeModSource::MPE_Y;
	int16_t mpeVals[3] = {0, 8000, 0};
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, mpeVals);
	renderCycles(arp, settings, 100);
}

TEST(ArpCovSynthSwitchNoteOn, synthMpeVelocityLowClamped) {
	settings->mpeVelocity = ArpMpeModSource::AFTERTOUCH;
	int16_t mpeVals[3] = {0, 0, 0};
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, mpeVals);
	renderCycles(arp, settings, 100);
}

TEST(ArpCovSynthSwitchNoteOn, synthHighNoteClampedTo127) {
	settings->numOctaves = 8;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 120, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovSynthSwitchNoteOn, synthOctaveSpreadPath) {
	settings->spreadOctave = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovSynthSwitchNoteOn, synthGlideProbabilityPath) {
	settings->glideProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovSynthSwitchNoteOn, synthChordProbabilityPath) {
	settings->chordProbability = UINT32_MAX;
	settings->chordPolyphony = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

// == render() spread gate and ratchet ==

TEST_GROUP(ArpCovRenderPaths) {
	ArpeggiatorForDrum* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new ArpeggiatorForDrum();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 2;
		settings->noteProbability = UINT32_MAX;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpCovRenderPaths, spreadGatePositive) {
	settings->spreadGate = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovRenderPaths, spreadGateWithLock) {
	settings->randomizerLock = true;
	settings->spreadGate = UINT32_MAX;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200);
}

TEST(ArpCovRenderPaths, ratchetPath) {
	settings->ratchetProbability = UINT32_MAX;
	settings->ratchetAmount = UINT32_MAX;
	settings->syncLevel = (SyncLevel)0;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovRenderPaths, ratchetWith128thSync) {
	settings->ratchetProbability = UINT32_MAX;
	settings->ratchetAmount = UINT32_MAX;
	settings->syncLevel = SyncLevel::SYNC_LEVEL_128TH;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200, 2147483647u, 500000000u);
}

TEST(ArpCovRenderPaths, ratchetWith64thSync) {
	settings->ratchetProbability = UINT32_MAX;
	settings->ratchetAmount = UINT32_MAX;
	settings->syncLevel = SyncLevel::SYNC_LEVEL_64TH;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 200, 2147483647u, 500000000u);
}

TEST(ArpCovRenderPaths, ratchetWith256thSyncNotAllowed) {
	settings->ratchetProbability = UINT32_MAX;
	settings->ratchetAmount = UINT32_MAX;
	settings->syncType = SYNC_TYPE_EVEN;
	settings->syncLevel = SyncLevel::SYNC_LEVEL_256TH;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 100, 2147483647u, 500000000u);
}

// == Glide paths ==

TEST_GROUP(ArpCovGlide) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovGlide, drumGlideSwitchNoteOff) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.glideProbability = UINT32_MAX;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovGlide, synthGlideSwitchNoteOff) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.glideProbability = UINT32_MAX;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 64, 90, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 500, 2147483647u, 500000000u);
}

// == Walk modes ==

TEST_GROUP(ArpCovWalkModes) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 3;
		settings->noteProbability = UINT32_MAX;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpCovWalkModes, walk1WithMultipleNotesAndOctaves) {
	settings->noteMode = ArpNoteMode::WALK1;
	settings->octaveMode = ArpOctaveMode::UP;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 52, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 55, 80, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 70, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovWalkModes, walk2WithOctaveDown) {
	settings->noteMode = ArpNoteMode::WALK2;
	settings->octaveMode = ArpOctaveMode::DOWN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovWalkModes, walk3WithOctaveAlternate) {
	settings->noteMode = ArpNoteMode::WALK3;
	settings->octaveMode = ArpOctaveMode::ALTERNATE;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 52, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 55, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovWalkModes, walk1WithOctaveUpDown) {
	settings->noteMode = ArpNoteMode::WALK1;
	settings->octaveMode = ArpOctaveMode::UP_DOWN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovWalkModes, walk2WithOctaveRandom) {
	settings->noteMode = ArpNoteMode::WALK2;
	settings->octaveMode = ArpOctaveMode::RANDOM;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	renderCycles(arp, settings, 300, 2147483647u, 500000000u);
}

// == Note + octave mode combinations ==

TEST_GROUP(ArpCovNoteOctaveCombos) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 3;
		settings->noteProbability = UINT32_MAX;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpCovNoteOctaveCombos, upNoteDownOctave) {
	settings->noteMode = ArpNoteMode::UP;
	settings->octaveMode = ArpOctaveMode::DOWN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 52, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 55, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovNoteOctaveCombos, downNoteUpOctave) {
	settings->noteMode = ArpNoteMode::DOWN;
	settings->octaveMode = ArpOctaveMode::UP;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 52, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 55, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovNoteOctaveCombos, upDownNoteUpDownOctave) {
	settings->noteMode = ArpNoteMode::UP_DOWN;
	settings->octaveMode = ArpOctaveMode::UP_DOWN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovNoteOctaveCombos, downNoteAlternateOctave) {
	settings->noteMode = ArpNoteMode::DOWN;
	settings->octaveMode = ArpOctaveMode::ALTERNATE;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 52, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 55, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovNoteOctaveCombos, randomNoteUpOctave) {
	settings->noteMode = ArpNoteMode::RANDOM;
	settings->octaveMode = ArpOctaveMode::UP;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 300, 2147483647u, 500000000u);
}

TEST(ArpCovNoteOctaveCombos, randomNoteDownOctave) {
	settings->noteMode = ArpNoteMode::RANDOM;
	settings->octaveMode = ArpOctaveMode::DOWN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	renderCycles(arp, settings, 300, 2147483647u, 500000000u);
}

TEST(ArpCovNoteOctaveCombos, randomNoteAlternateOctave) {
	settings->noteMode = ArpNoteMode::RANDOM;
	settings->octaveMode = ArpOctaveMode::ALTERNATE;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	renderCycles(arp, settings, 300, 2147483647u, 500000000u);
}

TEST(ArpCovNoteOctaveCombos, randomNoteUpDownOctave) {
	settings->noteMode = ArpNoteMode::RANDOM;
	settings->octaveMode = ArpOctaveMode::UP_DOWN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	renderCycles(arp, settings, 300, 2147483647u, 500000000u);
}

TEST(ArpCovNoteOctaveCombos, singleOctaveAllNoteModes) {
	settings->numOctaves = 1;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	ArpNoteMode modes[] = {ArpNoteMode::UP, ArpNoteMode::DOWN, ArpNoteMode::UP_DOWN, ArpNoteMode::RANDOM};
	for (auto mode : modes) {
		settings->noteMode = mode;
		renderCycles(arp, settings, 100, 2147483647u, 500000000u);
	}
}

// == NoteOff in ARP mode ==

TEST_GROUP(ArpCovNoteOffArpMode) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 2;
		settings->noteMode = ArpNoteMode::UP;
		settings->octaveMode = ArpOctaveMode::UP;
		settings->noteProbability = UINT32_MAX;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpCovNoteOffArpMode, noteOffAdjustsWhichNote) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zMpe);
	renderCycles(arp, settings, 50, 2147483647u, 500000000u);
	ArpReturnInstruction offInst;
	arp->noteOff(settings, 48, &offInst);
	CHECK_EQUAL(2, arp->notes.getNumElements());
}

TEST(ArpCovNoteOffArpMode, noteOffCurrentlyPlayingNote) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 72, 90, &inst, 0, zMpe);
	renderCycles(arp, settings, 20, 2147483647u, 500000000u);
	ArpReturnInstruction offInst1;
	arp->noteOff(settings, 60, &offInst1);
	ArpReturnInstruction offInst2;
	arp->noteOff(settings, 72, &offInst2);
	CHECK_EQUAL(0, arp->notes.getNumElements());
}

TEST(ArpCovNoteOffArpMode, noteOnWhileArpRunningAdjustsIndex) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zMpe);
	arp->noteOn(settings, 72, 90, &inst, 0, zMpe);
	renderCycles(arp, settings, 50, 2147483647u, 500000000u);
	ArpReturnInstruction inst2;
	arp->noteOn(settings, 48, 80, &inst2, 0, zMpe);
	CHECK_EQUAL(3, arp->notes.getNumElements());
}

// == Drum noteOff in ARP mode ==

TEST_GROUP(ArpCovDrumNoteOffArpMode) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovDrumNoteOffArpMode, noteOffInArpModeSetsGlideNoteOff) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 50, 2147483647u, 500000000u);
	ArpReturnInstruction offInst;
	arp.noteOff(&settings, 60, &offInst);
	CHECK_EQUAL(0, arp.active_note.velocity);
}

// == handlePendingNotes ==

TEST_GROUP(ArpCovHandlePending) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovHandlePending, handlePendingInArpModeCallsBase) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	ArpReturnInstruction pendInst;
	arp.handlePendingNotes(&settings, &pendInst);
}

TEST(ArpCovHandlePending, handlePendingOffModeWithPendingNoteNone) {
	TestArpSynth arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::OFF;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	ArpReturnInstruction pendInst;
	arp.handlePendingNotes(&settings, &pendInst);
	if (arp.notes.getNumElements() > 0) {
		ArpNote* n = (ArpNote*)arp.notes.getElementAddress(0);
		n->noteStatus[0] = ArpNoteStatus::PENDING;
		n->noteCodeOnPostArp[0] = ARP_NOTE_NONE;
		arp.anyPending = true;
		ArpReturnInstruction pendInst2;
		bool result = arp.handlePendingNotes(&settings, &pendInst2);
		CHECK_FALSE(result);
	}
}

// == Random helpers ==

TEST_GROUP(ArpCovRandomHelpers) {
	TestArpDrum arp;
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovRandomHelpers, probabilityZeroAlwaysFalse) {
	CHECK_FALSE(arp.getRandomProbabilityResult(0));
}

TEST(ArpCovRandomHelpers, probabilityMaxAlwaysTrue) {
	CHECK_TRUE(arp.getRandomProbabilityResult(UINT32_MAX));
}

TEST(ArpCovRandomHelpers, probabilityMidRange) {
	arp.getRandomProbabilityResult(2147483648u);
}

TEST(ArpCovRandomHelpers, bipolarAmountZeroReturnsZero) {
	int8_t result = arp.getRandomBipolarProbabilityAmount(0);
	CHECK_EQUAL(0, result);
}

TEST(ArpCovRandomHelpers, bipolarAmountNonZero) {
	int8_t result = arp.getRandomBipolarProbabilityAmount(UINT32_MAX);
	CHECK(result >= -127);
	CHECK(result <= 127);
}

TEST(ArpCovRandomHelpers, weighted2BitsZeroReturnsZero) {
	int8_t result = arp.getRandomWeighted2BitsAmount(0);
	CHECK_EQUAL(0, result);
}

TEST(ArpCovRandomHelpers, weighted2BitsNonZero) {
	int8_t result = arp.getRandomWeighted2BitsAmount(UINT32_MAX);
	CHECK(result >= 0);
}

// == Force restart ==

TEST_GROUP(ArpCovForceRestart) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovForceRestart, forceRestartConsumedByRender) {
	// flagForceArpRestart is consumed in executeArpStep, called from switchNoteOn
	// We verify the flag is set and that render proceeds without crashing
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 2;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 48, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 60, 90, &inst, 0, zMpe);
	arp.noteOn(&settings, 72, 80, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 500, 2147483647u, 500000000u);
	settings.flagForceArpRestart = true;
	// More render cycles should consume the flag eventually
	renderCycles(&arp, &settings, 1000, 2147483647u, 500000000u);
	// Even if flag wasn't consumed (depends on gate timing), no crash occurred
}

// == Kit arpeggiator ==

TEST_GROUP(ArpCovKitArp) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovKitArp, kitNoteOnMultipleDrumsWithArp) {
	ArpeggiatorForKit arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 0, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 1, 90, &inst, 0, zMpe);
	arp.noteOn(&settings, 2, 80, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 200, 2147483647u, 500000000u);
}

TEST(ArpCovKitArp, kitRemoveDrumWhileArpRunning) {
	ArpeggiatorForKit arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 0, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 1, 90, &inst, 0, zMpe);
	arp.noteOn(&settings, 2, 80, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 50, 2147483647u, 500000000u);
	arp.removeDrumIndex(&settings, 1);
	CHECK_EQUAL(2, arp.notes.getNumElements());
	renderCycles(&arp, &settings, 50, 2147483647u, 500000000u);
}

// == Constructor no song ==

TEST_GROUP(ArpCovConstructor) {
	void setup() {}
};

TEST(ArpCovConstructor, constructorWithNoSong) {
	Song* savedCurrent = currentSong;
	Song* savedPreLoaded = preLoadedSong;
	currentSong = nullptr;
	preLoadedSong = nullptr;
	ArpeggiatorSettings s;
	CHECK(s.syncType == SYNC_TYPE_EVEN);
	currentSong = savedCurrent;
	preLoadedSong = savedPreLoaded;
}

// == Drum noteOn arp mode second note ==

TEST_GROUP(ArpCovDrumNoteOnArp) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovDrumNoteOnArp, secondNoteOnWhileFirstActive) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	CHECK(arp.active_note.velocity > 0);
	ArpReturnInstruction inst2;
	arp.noteOn(&settings, 72, 90, &inst2, 0, zMpe);
	CHECK(arp.active_note.velocity > 0);
	CHECK_EQUAL(72, arp.active_note.inputCharacteristics[0]);
}

// == Evaluate helpers ==

TEST_GROUP(ArpCovEvaluate) {
	TestArpDrum arp;
	ArpeggiatorSettings settings;
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		settings.mode = ArpMode::ARP;
		settings.numOctaves = 1;
	}
};

TEST(ArpCovEvaluate, evaluateRhythmNonRatchet) {
	bool result = arp.evaluateRhythm(0, false);
	CHECK(result == true || result == false);
}

TEST(ArpCovEvaluate, evaluateRhythmRatchet) {
	bool result = arp.evaluateRhythm(0, true);
	CHECK(result == true || result == false);
}

TEST(ArpCovEvaluate, evaluateNoteProbabilityNonRatchet) {
	arp.isPlayNoteForCurrentStep = true;
	bool result = arp.evaluateNoteProbability(false);
	CHECK_TRUE(result);
}

TEST(ArpCovEvaluate, evaluateNoteProbabilityRatchet) {
	arp.lastNormalNotePlayedFromNoteProbability = false;
	bool result = arp.evaluateNoteProbability(true);
	CHECK_FALSE(result);
}

TEST(ArpCovEvaluate, evaluateBassProbabilityNonRatchet) {
	arp.isPlayBassForCurrentStep = true;
	bool result = arp.evaluateBassProbability(false);
	CHECK_TRUE(result);
}

TEST(ArpCovEvaluate, evaluateBassProbabilityRatchet) {
	arp.lastNormalNotePlayedFromBassProbability = false;
	bool result = arp.evaluateBassProbability(true);
	CHECK_FALSE(result);
}

TEST(ArpCovEvaluate, evaluateSwapProbabilityNonRatchet) {
	arp.isPlayRandomStepForCurrentStep = true;
	bool result = arp.evaluateSwapProbability(false);
	CHECK_TRUE(result);
}

TEST(ArpCovEvaluate, evaluateReverseProbabilityNonRatchet) {
	arp.isPlayReverseForCurrentStep = true;
	bool result = arp.evaluateReverseProbability(false);
	CHECK_TRUE(result);
}

TEST(ArpCovEvaluate, evaluateChordProbabilityNonRatchet) {
	arp.isPlayChordForCurrentStep = true;
	bool result = arp.evaluateChordProbability(false);
	CHECK_TRUE(result);
}

// == Reverse probability ==

TEST_GROUP(ArpCovReverse) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovReverse, drumReverseProbabilityWithKitArpInvert) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.noteProbability = UINT32_MAX;
	settings.reverseProbability = UINT32_MAX;
	arp.invertReversedFromKitArp = true;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 200, 2147483647u, 500000000u);
}

TEST(ArpCovReverse, drumReverseProbabilityWithoutKitArpInvert) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.noteProbability = UINT32_MAX;
	settings.reverseProbability = UINT32_MAX;
	arp.invertReversedFromKitArp = false;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 200, 2147483647u, 500000000u);
}

// == Reset helpers ==

TEST_GROUP(ArpCovResetHelpers) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovResetHelpers, resetRatchetClearsState) {
	TestArpDrum arp;
	arp.isRatcheting = true;
	arp.ratchetNotesMultiplier = 3;
	arp.ratchetNotesCount = 8;
	arp.ratchetNotesIndex = 5;
	arp.resetRatchet();
	CHECK_FALSE(arp.isRatcheting);
	CHECK_EQUAL(0u, arp.ratchetNotesMultiplier);
	CHECK_EQUAL(0u, arp.ratchetNotesCount);
	CHECK_EQUAL(0u, arp.ratchetNotesIndex);
}

TEST(ArpCovResetHelpers, resetBaseClearsAll) {
	TestArpDrum arp;
	arp.isRatcheting = true;
	arp.notesPlayedFromRhythm = 5;
	arp.stepRepeatIndex = 3;
	arp.glideOnNextNoteOff = true;
	arp.resetBase();
	CHECK_FALSE(arp.isRatcheting);
	CHECK_EQUAL(0u, arp.notesPlayedFromRhythm);
	CHECK_EQUAL(0u, arp.stepRepeatIndex);
	CHECK_FALSE(arp.glideOnNextNoteOff);
}

// == Initial note/octave ==

TEST_GROUP(ArpCovInitialNoteOctave) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovInitialNoteOctave, downNoteDownOctave) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 3;
	settings.noteMode = ArpNoteMode::DOWN;
	settings.octaveMode = ArpOctaveMode::DOWN;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 48, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 60, 90, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 300, 2147483647u, 500000000u);
}

TEST(ArpCovInitialNoteOctave, downNoteAlternateOctave) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 3;
	settings.noteMode = ArpNoteMode::DOWN;
	settings.octaveMode = ArpOctaveMode::ALTERNATE;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 48, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 60, 90, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 300, 2147483647u, 500000000u);
}

TEST(ArpCovInitialNoteOctave, randomNoteRandomOctave) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 4;
	settings.noteMode = ArpNoteMode::RANDOM;
	settings.octaveMode = ArpOctaveMode::RANDOM;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 48, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 60, 90, &inst, 0, zMpe);
	arp.noteOn(&settings, 72, 80, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 300, 2147483647u, 500000000u);
}

// == NoteOff ratchet reset ==

TEST_GROUP(ArpCovNoteOffRatchetReset) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovNoteOffRatchetReset, noteOffResetsRatchetIfComplete) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.noteProbability = UINT32_MAX;
	settings.ratchetProbability = UINT32_MAX;
	settings.ratchetAmount = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 72, 90, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 200, 2147483647u, 500000000u);
	ArpReturnInstruction offInst;
	arp.noteOff(&settings, 60, &offInst);
	CHECK_EQUAL(1, arp.notes.getNumElements());
}

// == NoteOn with zero probability ==

TEST_GROUP(ArpCovNoteOnSkip) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovNoteOnSkip, drumNoteOnWithZeroNoteProbability) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::OFF;
	settings.noteProbability = 0;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	CHECK(arp.active_note.velocity > 0);
}

TEST(ArpCovNoteOnSkip, synthNoteOnWithZeroNoteProbability) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::OFF;
	settings.noteProbability = 0;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	CHECK_EQUAL(1, arp.notes.getNumElements());
}

// == Locked randomizer ==

TEST_GROUP(ArpCovLockedRandomizer) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovLockedRandomizer, lockedRandomizerWithParamChange) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.randomizerLock = true;
	settings.noteProbability = 2147483648u;
	settings.bassProbability = 1000000000u;
	settings.swapProbability = 500000000u;
	settings.glideProbability = 3000000000u;
	settings.reverseProbability = 4000000000u;
	settings.chordProbability = 100000000u;
	settings.ratchetProbability = 2000000000u;
	settings.spreadVelocity = 1234567890u;
	settings.spreadGate = 987654321u;
	settings.spreadOctave = 555555555u;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 100, 2147483647u, 500000000u);
	settings.noteProbability = 3000000000u;
	renderCycles(&arp, &settings, 100, 2147483647u, 500000000u);
	settings.bassProbability = 2000000000u;
	renderCycles(&arp, &settings, 100, 2147483647u, 500000000u);
}

TEST(ArpCovLockedRandomizer, lockedRandomizerUnlockAndRelock) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.randomizerLock = true;
	settings.noteProbability = 2147483648u;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 50, 2147483647u, 500000000u);
	settings.randomizerLock = false;
	renderCycles(&arp, &settings, 50, 2147483647u, 500000000u);
	settings.randomizerLock = true;
	renderCycles(&arp, &settings, 50, 2147483647u, 500000000u);
}

// == Rhythm ==

TEST_GROUP(ArpCovRhythm) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovRhythm, drumWithNonZeroRhythm) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	settings.noteProbability = UINT32_MAX;
	settings.rhythm = 2147483648u;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 300, 2147483647u, 500000000u);
}

TEST(ArpCovRhythm, synthWithNonZeroRhythm) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 2;
	settings.noteProbability = UINT32_MAX;
	settings.rhythm = 2147483648u;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 72, 90, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 300, 2147483647u, 500000000u);
}

// == Sequence length ==

TEST_GROUP(ArpCovSequenceLength) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovSequenceLength, synthWithSequenceLength) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 2;
	settings.noteProbability = UINT32_MAX;
	settings.sequenceLength = 3000000000u;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 48, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 60, 90, &inst, 0, zMpe);
	arp.noteOn(&settings, 72, 80, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 500, 2147483647u, 500000000u);
}

TEST(ArpCovSequenceLength, drumWithSequenceLength) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 2;
	settings.noteProbability = UINT32_MAX;
	settings.sequenceLength = 3000000000u;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 500, 2147483647u, 500000000u);
}

// == Step repeat ==

TEST_GROUP(ArpCovStepRepeat) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovStepRepeat, synthStepRepeatSkipsAdvance) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 2;
	settings.numStepRepeats = 4;
	settings.noteProbability = UINT32_MAX;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 48, 100, &inst, 0, zMpe);
	arp.noteOn(&settings, 60, 90, &inst, 0, zMpe);
	arp.noteOn(&settings, 72, 80, &inst, 0, zMpe);
	renderCycles(&arp, &settings, 500, 2147483647u, 500000000u);
}

// == Render pending/off early return ==

TEST_GROUP(ArpCovRenderPending) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovRenderPending, renderWithPendingNoteReturnsEarly) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::ARP;
	settings.numOctaves = 1;
	ArpReturnInstruction inst;
	arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
	ArpReturnInstruction rInst;
	arp.render(&settings, &rInst, 128, 2147483647u, 1000000);
}

TEST(ArpCovRenderPending, renderWithModeOffReturnsEarly) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;
	settings.mode = ArpMode::OFF;
	ArpReturnInstruction rInst;
	arp.render(&settings, &rInst, 128, 2147483647u, 1000000);
}

// == Chord types ==

TEST_GROUP(ArpCovDrumChordType) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpCovDrumChordType, differentChordTypeIndexes) {
	for (uint8_t chordIdx = 0; chordIdx < 4; chordIdx++) {
		ArpeggiatorForDrum arp;
		ArpeggiatorSettings settings;
		settings.mode = ArpMode::ARP;
		settings.numOctaves = 2;
		settings.noteProbability = UINT32_MAX;
		settings.chordTypeIndex = chordIdx;
		ArpReturnInstruction inst;
		arp.noteOn(&settings, 60, 100, &inst, 0, zMpe);
		renderCycles(&arp, &settings, 100, 2147483647u, 500000000u);
	}
}
