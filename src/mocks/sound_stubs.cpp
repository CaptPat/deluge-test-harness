// Stub implementations for Sound class.
// The real sound.cpp pulls in half the codebase (sound_editor.h, kit.h, instrument_clip.h, etc).
// We implement the constructor/destructor and critical methods; rendering is stubbed.

#include "processing/sound/sound.h"
#include "processing/engines/audio_engine.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"

namespace params = deluge::modulation::params;

static constexpr Patcher::Config kPatcherConfigForSound = {
    .firstParam = params::FIRST_GLOBAL,
    .firstNonVolumeParam = params::FIRST_GLOBAL_NON_VOLUME,
    .firstHybridParam = params::FIRST_GLOBAL_HYBRID,
    .firstExpParam = params::FIRST_GLOBAL_EXP,
    .endParams = params::kNumParams,
    .globality = GLOBALITY_GLOBAL,
};

Sound::Sound() : patcher(kPatcherConfigForSound, globalSourceValues, paramFinalValues) {
	unpatchedParamKind_ = params::Kind::UNPATCHED_SOUND;
	oscRetriggerPhase.fill(0xFFFFFFFF);
	modFXType_ = ModFXType::NONE;
	lpfMode = FilterMode::TRANSISTOR_24DB;
	postReverbVolumeLastTime = -1;

	// ModKnob defaults
	modKnobs[0][1].paramDescriptor.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	modKnobs[0][0].paramDescriptor.setToHaveParamOnly(params::LOCAL_PAN);
	modKnobs[1][1].paramDescriptor.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	modKnobs[1][0].paramDescriptor.setToHaveParamOnly(params::LOCAL_LPF_RESONANCE);
	modKnobs[2][1].paramDescriptor.setToHaveParamOnly(params::LOCAL_ENV_0_ATTACK);
	modKnobs[2][0].paramDescriptor.setToHaveParamOnly(params::LOCAL_ENV_0_RELEASE);
	modKnobs[3][1].paramDescriptor.setToHaveParamOnly(params::GLOBAL_DELAY_RATE);
	modKnobs[3][0].paramDescriptor.setToHaveParamOnly(params::GLOBAL_DELAY_FEEDBACK);
	modKnobs[4][0].paramDescriptor.setToHaveParamOnly(params::GLOBAL_REVERB_AMOUNT);
	modKnobs[5][1].paramDescriptor.setToHaveParamOnly(params::GLOBAL_LFO_FREQ_1);
	modKnobs[4][1].paramDescriptor.setToHaveParamAndSource(params::GLOBAL_VOLUME_POST_REVERB_SEND,
	                                                       PatchSource::SIDECHAIN);
	modKnobs[5][0].paramDescriptor.setToHaveParamAndSource(params::LOCAL_PITCH_ADJUST, PatchSource::LFO_GLOBAL_1);
	modKnobs[6][1].paramDescriptor.setToHaveParamOnly(params::UNPATCHED_START + params::UNPATCHED_STUTTER_RATE);
	modKnobs[6][0].paramDescriptor.setToHaveParamOnly(params::UNPATCHED_START + params::UNPATCHED_PORTAMENTO);
	modKnobs[7][1].paramDescriptor.setToHaveParamOnly(params::UNPATCHED_START
	                                                  + params::UNPATCHED_SAMPLE_RATE_REDUCTION);
	modKnobs[7][0].paramDescriptor.setToHaveParamOnly(params::UNPATCHED_START + params::UNPATCHED_BITCRUSHING);
	expressionSourcesChangedAtSynthLevel.reset();
	paramLPF.p = PARAM_LPF_OFF;
	AudioEngine::sounds.push_back(this);
}

// ── Rendering stubs ────────────────────────────────────────────────────
void Sound::render(ModelStackWithThreeMainThings*, std::span<StereoSample>, int32_t*, int32_t, int32_t, bool, int32_t,
                   SampleRecorder*) {}
void Sound::setSkippingRendering(bool newSkipping) { skippingRendering = newSkipping; }

// ── Voice management stubs ─────────────────────────────────────────────
void Sound::voiceUnassigned(ModelStackWithSoundFlags*) {}
void Sound::noteOn(ModelStackWithThreeMainThings*, ArpeggiatorBase*, int32_t, int16_t const*, uint32_t, int32_t,
                   uint32_t, int32_t, int32_t) {}
void Sound::noteOff(ModelStackWithThreeMainThings*, ArpeggiatorBase*, int32_t) {}
void Sound::allNotesOff(ModelStackWithThreeMainThings*, ArpeggiatorBase*) {}
void Sound::noteOnPostArpeggiator(ModelStackWithSoundFlags*, int32_t, int32_t, int32_t, int16_t const*, uint32_t,
                                  int32_t, uint32_t, int32_t) {}
void Sound::noteOffPostArpeggiator(ModelStackWithSoundFlags*, int32_t) {}
void Sound::killAllVoices() { voices_.clear(); }
bool Sound::anyNoteIsOn() { return !voices_.empty(); }
bool Sound::allowNoteTails(ModelStackWithSoundFlags*, bool) { return true; }
void Sound::prepareForHibernation() { killAllVoices(); }

void Sound::freeActiveVoice(const ActiveVoice& /*voice*/, ModelStackWithSoundFlags*, bool) {}
const Sound::ActiveVoice& Sound::acquireVoice() noexcept(false) {
	auto voice = AudioEngine::VoicePool::get().acquire(*this);
	voices_.push_back(std::move(voice));
	return voices_.back();
}
const Sound::ActiveVoice& Sound::stealOneActiveVoice() { return voices_.front(); }
void Sound::terminateOneActiveVoice() {}
void Sound::forceReleaseOneActiveVoice() {}
const Sound::ActiveVoice& Sound::getLowestPriorityVoice() const { return voices_.front(); }
void Sound::checkVoiceExists(const ActiveVoice&, const char*) const {}

// ── Param / patching stubs ────────────────────────────────────────────
void Sound::patchedParamPresetValueChanged(uint8_t, ModelStackWithSoundFlags*, int32_t, int32_t) {}
PatchCableAcceptance Sound::maySourcePatchToParam(PatchSource, uint8_t, ParamManager*) {
	return PatchCableAcceptance::EDITABLE;
}
void Sound::recalculatePatchingToParam(uint8_t, ParamManagerForTimeline*) {}
void Sound::initParams(ParamManager* paramManager) { ModControllableAudio::initParams(paramManager); }
Error Sound::createParamManagerForLoading(ParamManagerForTimeline*) { return Error::NONE; }
void Sound::ensureInaccessibleParamPresetValuesWithoutKnobsAreZero(Song*) {}
void Sound::ensureInaccessibleParamPresetValuesWithoutKnobsAreZero(ModelStackWithThreeMainThings*) {}
void Sound::ensureInaccessibleParamPresetValuesWithoutKnobsAreZeroWithMinimalDetails(ParamManager*) {}
void Sound::ensureParamPresetValueWithoutKnobIsZero(ModelStackWithAutoParam*) {}
void Sound::ensureParamPresetValueWithoutKnobIsZeroWithMinimalDetails(ParamManager*, int32_t) {}
void Sound::possiblySetupDefaultExpressionPatching(ParamManager*) {}
void Sound::compensateVolumeForResonance(ModelStackWithThreeMainThings*) {}

// ── File I/O stubs ──────────────────────────────────────────────────────
Error Sound::readFromFile(Deserializer&, ModelStackWithModControllable*, int32_t, ArpeggiatorSettings*) {
	return Error::NONE;
}
void Sound::writeToFile(Serializer&, bool, ParamManager*, ArpeggiatorSettings*, const char*) {}
void Sound::writeParamsToFile(Serializer&, ParamManager*, bool) {}
void Sound::readParamsFromFile(Deserializer&, ParamManagerForTimeline*, int32_t) {}
bool Sound::readParamTagFromFile(Deserializer&, char const*, ParamManagerForTimeline*, int32_t) { return false; }
void Sound::doneReadingFromFile() {}

// ── Misc stubs ──────────────────────────────────────────────────────────
void Sound::notifyValueChangeViaLPF(int32_t, bool, ModelStackWithThreeMainThings const*, int32_t, int32_t, bool) {}
ModFXType Sound::getModFXType() { return modFXType_; }
bool Sound::setModFXType(ModFXType newType) {
	modFXType_ = newType;
	return true;
}
bool Sound::hasFilters() { return true; }
void Sound::resyncGlobalLFOs() {}
int8_t Sound::getKnobPos(uint8_t, ParamManagerForTimeline*, uint32_t, TimelineCounter*) { return 0; }
int32_t Sound::getKnobPosBig(int32_t, ParamManagerForTimeline*, uint32_t, TimelineCounter*) { return 0; }
bool Sound::learnKnob(MIDICable*, ParamDescriptor, uint8_t, uint8_t, uint8_t, Song*) { return false; }
void Sound::sampleZoneChanged(MarkerType, int32_t, ModelStackWithSoundFlags*) {}
void Sound::setNumUnison(int32_t newNum, ModelStackWithSoundFlags*) { numUnison = static_cast<uint8_t>(newNum); }
void Sound::setUnisonDetune(int32_t newAmount, ModelStackWithSoundFlags*) {
	unisonDetune = static_cast<int8_t>(newAmount);
}
void Sound::setUnisonStereoSpread(int32_t newAmount) { unisonStereoSpread = static_cast<uint8_t>(newAmount); }
void Sound::setModulatorTranspose(int32_t m, int32_t value, ModelStackWithSoundFlags*) {
	modulatorTranspose[m] = static_cast<int16_t>(value);
}
void Sound::setModulatorCents(int32_t m, int32_t value, ModelStackWithSoundFlags*) {
	modulatorCents[m] = static_cast<int8_t>(value);
}
bool Sound::isSourceActiveCurrently(int32_t, ParamManagerForTimeline*) { return true; }
bool Sound::isSourceActiveEverDisregardingMissingSample(int32_t, ParamManager*) { return true; }
bool Sound::isSourceActiveEver(int32_t, ParamManager*) { return true; }
bool Sound::isNoiseActiveEver(ParamManagerForTimeline*) { return false; }
bool Sound::envelopeHasSustainCurrently(int32_t, ParamManagerForTimeline*) { return true; }
bool Sound::envelopeHasSustainEver(int32_t, ParamManagerForTimeline*) { return true; }
bool Sound::renderingOscillatorSyncCurrently(ParamManagerForTimeline*) { return false; }
bool Sound::renderingOscillatorSyncEver(ParamManager*) { return false; }
void Sound::setupAsBlankSynth(ParamManager*, bool) {}
void Sound::setupAsDefaultSynth(ParamManager*) {}
void Sound::setupAsSample(ParamManagerForTimeline*) {}
void Sound::recalculateAllVoicePhaseIncrements(ModelStackWithSoundFlags*) {}
void Sound::retriggerVoicesForTransposeChange(ModelStackWithSoundFlags*) {}
Error Sound::loadAllAudioFiles(bool) { return Error::NONE; }
void Sound::modButtonAction(uint8_t, bool, ParamManagerForTimeline*) {}
bool Sound::modEncoderButtonAction(uint8_t, bool, ModelStackWithThreeMainThings*) { return false; }
void Sound::polyphonicExpressionEventOnChannelOrNote(int32_t, int32_t, int32_t, MIDICharacteristic) {}
int16_t Sound::getMaxOscTranspose(InstrumentClip*) { return 96; }
int16_t Sound::getMinOscTranspose() { return -96; }
void Sound::setSynthMode(SynthMode value, Song*) { synthMode = value; }
int32_t Sound::hasAnyTimeStretchSyncing(ParamManagerForTimeline*, bool, int32_t) { return 0; }
int32_t Sound::hasCutOrLoopModeSamples(ParamManagerForTimeline*, int32_t, bool*) { return 0; }
bool Sound::hasCutModeSamples(ParamManagerForTimeline*) { return false; }
bool Sound::allowsVeryLateNoteStart(InstrumentClip*, ParamManagerForTimeline*) { return false; }
void Sound::fastReleaseAllVoices(ModelStackWithSoundFlags*) { killAllVoices(); }
void Sound::reassessRenderSkippingStatus(ModelStackWithSoundFlags*, bool) {}
void Sound::getThingWithMostReverb(Sound**, ParamManager**, GlobalEffectableForClip**, int32_t*,
                                   ParamManagerForTimeline*) {}
void Sound::detachSourcesFromAudioFiles() {}
void Sound::wontBeRenderedForAWhile() {}
ModelStackWithAutoParam* Sound::getParamFromModEncoder(int32_t, ModelStackWithThreeMainThings*, bool) {
	return nullptr;
}
ModelStackWithAutoParam* Sound::getParamFromMIDIKnob(MIDIKnob&, ModelStackWithThreeMainThings*) { return nullptr; }
void Sound::deleteMultiRange(int32_t, int32_t) {}
uint32_t Sound::getSyncedLFOPhaseIncrement(const LFOConfig&) { return 0; }
void Sound::process_postarp_notes(ModelStackWithSoundFlags*, ArpeggiatorSettings*, ArpReturnInstruction) {}
