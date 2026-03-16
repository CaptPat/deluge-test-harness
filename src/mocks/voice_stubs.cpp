// Stub implementations for Voice class.
// The real voice.cpp is ~2500 lines of DSP rendering code.
// We provide constructor/destructor and critical lifecycle methods.

#include "model/voice/voice.h"
#include "model/model_stack.h"
#include "processing/sound/sound.h"
#include "modulation/params/param_set.h"

namespace params = deluge::modulation::params;

// Real voice constructor — needs Sound&
Voice::Voice(Sound& s) : sound(s), patcher({}, sourceValues, paramFinalValues) {
	doneFirstRender = false;
	previouslyIgnoredNoteOff = false;
	pedalState = PedalState::None;
	orderSounded = 0;
	overrideAmplitudeEnvelopeReleaseRate = 0;
	justCreated = true;
	overallOscAmplitudeLastTime = 0;
	filterGainLastTime = 0;
	portaEnvelopePos = 0;
	portaEnvelopeMaxAmplitude = 0;
	noteCodeAfterArpeggiation = 0;
	inputCharacteristics = {0, 0};
	lastSaturationTanHWorkingValue = {0, 0};
	sourceAmplitudesLastTime.fill(0);
	modulatorAmplitudeLastTime.fill(0);
	sourceWaveIndexesLastTime.fill(0);
	sourceValues.fill(0);
	paramFinalValues.fill(0);
	// Initialize envelopes to ATTACK (a freshly-sounded voice)
	for (auto& env : envelopes) {
		env.state = EnvelopeStage::ATTACK;
		env.pos = 0;
		env.lastValue = 0;
	}
	// Initialize source guides
	for (auto& g : guides) {
		g.noteOffReceived = false;
	}
}

// ── Lifecycle stubs ────────────────────────────────────────────────────
void Voice::setAsUnassigned(ModelStackWithSoundFlags*, bool) {}
void Voice::unassignStuff(bool) {}
bool Voice::render(ModelStackWithSoundFlags*, int32_t*, int32_t, bool, bool, uint32_t, bool, bool, int32_t) {
	return true; // voice still active
}
bool Voice::noteOn(ModelStackWithSoundFlags* modelStack, int32_t noteCodePreArp, int32_t noteCodePostArp,
                   uint8_t velocity, uint32_t sampleSyncLength, int32_t ticksLate, uint32_t samplesLate,
                   bool resetEnvelopes, int32_t fromMIDIChannel, const int16_t* mpeValues) {
	// Store key state so tests can observe it
	inputCharacteristics[util::to_underlying(MIDICharacteristic::NOTE)] = noteCodePreArp;
	inputCharacteristics[util::to_underlying(MIDICharacteristic::CHANNEL)] = fromMIDIChannel;
	noteCodeAfterArpeggiation = noteCodePostArp;

	// Store velocity as patch source (same as real firmware)
	sourceValues[util::to_underlying(PatchSource::VELOCITY)] =
	    (velocity == 128) ? 2147483647 : ((int32_t)velocity - 64) * 33554432;

	return true;
}
// Real noteOff with sustain/sostenuto pedal logic
void Voice::noteOff(ModelStackWithSoundFlags* modelStack, bool allowReleaseStage, bool ignoreSustain) {
	Sound& sound = *static_cast<Sound*>(modelStack->modControllable);

	// Check sustain pedal — if active, defer the note-off instead of releasing
	if (!ignoreSustain && !sound.isDrum()) {
		auto* paramManager = static_cast<ParamManagerForTimeline*>(modelStack->paramManager);
		UnpatchedParamSet* unpatchedParams = paramManager->getUnpatchedParamSet();
		int32_t sustainValue = unpatchedParams->getValue(params::UNPATCHED_SUSTAIN_PEDAL);
		if (sustainValue >= 0) {
			pedalState = pedalState | PedalState::SustainDeferred;
			return;
		}

		// Check sostenuto pedal — only holds notes that were captured when pedal went down
		if ((pedalState & PedalState::SostenutoCapture) != PedalState::None) {
			int32_t sostenutoValue = unpatchedParams->getValue(params::UNPATCHED_SOSTENUTO_PEDAL);
			if (sostenutoValue >= 0) {
				pedalState = pedalState | PedalState::SostenutoDeferred;
				return;
			}
		}
	}
	pedalState = pedalState & ~(PedalState::SustainDeferred | PedalState::SostenutoDeferred);

	for (int32_t s = 0; s < kNumSources; s++) {
		guides[s].noteOffReceived = true;
	}

	// Envelope release logic simplified for test harness
	// (real code checks allowNoteTails, release stage, etc.)
	if (allowReleaseStage) {
		envelopes[0].state = EnvelopeStage::RELEASE;
	}
	else {
		envelopes[0].state = EnvelopeStage::FAST_RELEASE;
	}
}
void Voice::calculatePhaseIncrements(ModelStackWithSoundFlags*) {}
bool Voice::sampleZoneChanged(ModelStackWithSoundFlags*, int32_t, MarkerType) { return true; }
void Voice::randomizeOscPhases(const Sound&) {}
void Voice::changeNoteCode(ModelStackWithSoundFlags*, int32_t, int32_t, int32_t, const int16_t*) {}
bool Voice::hasReleaseStage() { return true; }
uint32_t Voice::getPriorityRating() const { return orderSounded; }
void Voice::expressionEventImmediate(const Sound&, int32_t, int32_t) {}
void Voice::expressionEventSmooth(int32_t, int32_t) {}
bool Voice::doFastRelease(uint32_t) { return true; }
bool Voice::doImmediateRelease() { return false; }
bool Voice::forceNormalRelease() { return true; }
bool Voice::speedUpRelease() { return true; }
uint32_t Voice::getLocalLFOPhaseIncrement(LFO_ID, deluge::modulation::params::Local) { return 0; }

// ── Private stubs ──────────────────────────────────────────────────────
void Voice::renderBasicSource(Sound&, ParamManagerForTimeline*, int32_t, int32_t*, int32_t, bool, int32_t, bool*,
                              int32_t, bool, uint32_t*, uint32_t*, int32_t, uint32_t*, bool, int32_t) {}
bool Voice::adjustPitch(uint32_t&, int32_t) { return true; }
void Voice::renderSineWaveWithFeedback(int32_t*, int32_t, uint32_t*, int32_t, uint32_t, int32_t, int32_t*, bool,
                                       int32_t) {}
void Voice::renderFMWithFeedback(int32_t*, int32_t, int32_t*, uint32_t*, int32_t, uint32_t, int32_t, int32_t*,
                                 int32_t) {}
void Voice::renderFMWithFeedbackAdd(int32_t*, int32_t, int32_t*, uint32_t*, int32_t, uint32_t, int32_t, int32_t*,
                                    int32_t) {}
bool Voice::areAllUnisonPartsInactive(ModelStackWithSoundFlags&) const { return true; }
void Voice::setupPorta(const Sound&) {}
int32_t Voice::combineExpressionValues(const Sound&, int32_t) const { return 0; }
