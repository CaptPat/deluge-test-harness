// Stub implementations for Voice class.
// The real voice.cpp is ~2500 lines of DSP rendering code.
// We provide constructor/destructor and critical lifecycle methods.

#include "model/voice/voice.h"
#include "processing/sound/sound.h"

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
}

// ── Lifecycle stubs ────────────────────────────────────────────────────
void Voice::setAsUnassigned(ModelStackWithSoundFlags*, bool) {}
void Voice::unassignStuff(bool) {}
bool Voice::render(ModelStackWithSoundFlags*, int32_t*, int32_t, bool, bool, uint32_t, bool, bool, int32_t) {
	return true; // voice still active
}
bool Voice::noteOn(ModelStackWithSoundFlags*, int32_t, int32_t, uint8_t, uint32_t, int32_t, uint32_t, bool, int32_t,
                   const int16_t*) {
	return true;
}
void Voice::noteOff(ModelStackWithSoundFlags*, bool, bool) {}
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
