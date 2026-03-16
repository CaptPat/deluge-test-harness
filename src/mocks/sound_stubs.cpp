// Stub implementations for Sound class.
// The real sound.cpp pulls in half the codebase (sound_editor.h, kit.h, instrument_clip.h, etc).
// We implement the constructor/destructor and critical methods; rendering is stubbed.

#include "processing/sound/sound.h"
#include "model/model_stack.h"
#include "model/voice/voice.h"
#include "processing/engines/audio_engine.h"
#include "modulation/arpeggiator.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "storage/multi_range/multi_range.h"
#include <algorithm>
#include <ranges>

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

// ── Voice management — real implementations ────────────────────────────
void Sound::voiceUnassigned(ModelStackWithSoundFlags*) {}
void Sound::killAllVoices() { voices_.clear(); }
bool Sound::anyNoteIsOn() { return !voices_.empty(); }
bool Sound::allowNoteTails(ModelStackWithSoundFlags*, bool) { return true; }
void Sound::prepareForHibernation() { killAllVoices(); }

void Sound::freeActiveVoice(const ActiveVoice& voice, ModelStackWithSoundFlags* modelStack, bool erase) {
	if (erase) {
		for (auto it = voices_.begin(); it != voices_.end(); ++it) {
			if (it->get() == voice.get()) {
				voices_.erase(it);
				return;
			}
		}
	}
}

// Real noteOn — routes through arpeggiator, allocates voices
void Sound::noteOn(ModelStackWithThreeMainThings* modelStack, ArpeggiatorBase* arpeggiator, int32_t noteCodePreArp,
                   int16_t const* mpeValues, uint32_t sampleSyncLength, int32_t ticksLate, uint32_t samplesLate,
                   int32_t velocity, int32_t fromMIDIChannel) {
	ParamManagerForTimeline* paramManager = (ParamManagerForTimeline*)modelStack->paramManager;
	ModelStackWithSoundFlags* modelStackWithSoundFlags = modelStack->addSoundFlags();

	if (!((synthMode == SynthMode::RINGMOD) || (modelStackWithSoundFlags->checkSourceEverActive(0))
	      || (modelStackWithSoundFlags->checkSourceEverActive(1))
	      || (paramManager->getPatchedParamSet()->params[params::LOCAL_NOISE_VOLUME].containsSomething(-2147483648))))
	    [[unlikely]] {
		return;
	}

	UnpatchedParamSet* unpatchedParams = paramManager->getUnpatchedParamSet();

	// CC67 soft pedal — reduce velocity when active
	if (unpatchedParams->getValue(params::UNPATCHED_SOFT_PEDAL) >= 0) {
		velocity = std::max((velocity * 2 + 1) / 3, int32_t{1});
	}

	ArpeggiatorSettings* arpSettings = getArpSettings();
	if (arpSettings != nullptr) {
		arpSettings->updateParamsFromUnpatchedParamSet(unpatchedParams);
	}

	getArpBackInTimeAfterSkippingRendering(arpSettings);

	ArpReturnInstruction instruction;
	instruction.sampleSyncLengthOn = sampleSyncLength;

	arpeggiator->noteOn(arpSettings, noteCodePreArp, velocity, &instruction, fromMIDIChannel, mpeValues);

	if (instruction.arpNoteOn != nullptr) {
		for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
			if (instruction.arpNoteOn->noteCodeOnPostArp[n] == ARP_NOTE_NONE) {
				break;
			}

			if (AudioEngine::allowedToStartVoice()) {
				invertReversed = instruction.invertReversed;
				noteOnPostArpeggiator(modelStackWithSoundFlags, noteCodePreArp,
				                      instruction.arpNoteOn->noteCodeOnPostArp[n], instruction.arpNoteOn->velocity,
				                      mpeValues, instruction.sampleSyncLengthOn, ticksLate, samplesLate,
				                      fromMIDIChannel);
				instruction.arpNoteOn->noteStatus[n] = ArpNoteStatus::PLAYING;
			}
		}
	}
}

// Real noteOff — routes through arpeggiator
void Sound::noteOff(ModelStackWithThreeMainThings* modelStack, ArpeggiatorBase* arpeggiator, int32_t noteCode) {
	ModelStackWithSoundFlags* modelStackWithSoundFlags = modelStack->addSoundFlags();
	ArpeggiatorSettings* arpSettings = getArpSettings();

	ArpReturnInstruction instruction;
	arpeggiator->noteOff(arpSettings, noteCode, &instruction);

	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (instruction.glideNoteCodeOffPostArp[n] == ARP_NOTE_NONE) {
			break;
		}
		noteOffPostArpeggiator(modelStackWithSoundFlags, instruction.glideNoteCodeOffPostArp[n]);
	}
	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (instruction.noteCodeOffPostArp[n] == ARP_NOTE_NONE) {
			break;
		}
		noteOffPostArpeggiator(modelStackWithSoundFlags, instruction.noteCodeOffPostArp[n]);
	}

	reassessRenderSkippingStatus(modelStackWithSoundFlags);
}

void Sound::allNotesOff(ModelStackWithThreeMainThings* modelStack, ArpeggiatorBase* arpeggiator) {
	invertReversed = false;
	ModelStackWithSoundFlags* modelStackWithSoundFlags = modelStack->addSoundFlags();
	noteOffPostArpeggiator(modelStackWithSoundFlags, ALL_NOTES_OFF);
	arpeggiator->reset();
}

// Real noteOnPostArpeggiator — voice allocation and polyphony handling
void Sound::noteOnPostArpeggiator(ModelStackWithSoundFlags* modelStack, int32_t noteCodePreArp,
                                  int32_t noteCodePostArp, int32_t velocity, int16_t const* mpeValues,
                                  uint32_t sampleSyncLength, int32_t ticksLate, uint32_t samplesLate,
                                  int32_t fromMIDIChannel) {
	const ActiveVoice* voiceToReuse = nullptr;
	const ActiveVoice* voiceForLegato = nullptr;
	auto* paramManager = static_cast<ParamManagerForTimeline*>(modelStack->paramManager);

	// If not polyphonic, stop existing voices
	if (!voices_.empty() && polyphonic != PolyphonyMode::POLY) [[unlikely]] {
		for (auto it = voices_.begin(); it != voices_.end();) {
			ActiveVoice& voice = *it;
			if (polyphonic != PolyphonyMode::MONO && voice->envelopes[0].state < EnvelopeStage::RELEASE
			    && allowNoteTails(modelStack, true)) {
				voiceForLegato = &voice;
				break;
			}

			bool needs_unassign =
			    synthMode == SynthMode::FM
			    || std::ranges::any_of(std::views::iota(0, kNumSources),
			                           [&](int32_t s) {
				                           return isSourceActiveCurrently(s, paramManager)
				                                  && sources[s].oscType != OscType::SAMPLE;
			                           })
			    || (voice->envelopes[0].state != EnvelopeStage::FAST_RELEASE
			        && !voice->doFastRelease(SOFT_CULL_INCREMENT));

			if (needs_unassign) {
				if (voiceToReuse != nullptr) {
					this->freeActiveVoice(voice, modelStack, false);
					it = voices_.erase(it);
					continue;
				}
				voice->unassignStuff(false);
				voiceToReuse = &voice;
			}
			it++;
		}
	}

	if (polyphonic == PolyphonyMode::LEGATO && voiceForLegato) [[unlikely]] {
		(*voiceForLegato)->changeNoteCode(modelStack, noteCodePreArp, noteCodePostArp, fromMIDIChannel, mpeValues);
	}
	else {
		const ActiveVoice& voice = this->acquireVoice();

		reassessRenderSkippingStatus(modelStack);
		voice->randomizeOscPhases(*this);

		bool success = voice->noteOn(modelStack, noteCodePreArp, noteCodePostArp, velocity, sampleSyncLength,
		                             ticksLate, samplesLate, true, fromMIDIChannel, mpeValues);
		if (!success) {
			this->freeActiveVoice(voice, modelStack);
		}
	}

	lastNoteCode = noteCodePostArp;
}

// Real noteOffPostArpeggiator — voice release with sustain pedal support
void Sound::noteOffPostArpeggiator(ModelStackWithSoundFlags* modelStack, int32_t noteCode) {
	// MIDI output skipped in test harness

	if (voices_.empty()) {
		return;
	}

	ArpeggiatorSettings* arpSettings = getArpSettings();

	for (const ActiveVoice& voice : voices_) {
		if ((voice->noteCodeAfterArpeggiation == noteCode || noteCode == ALL_NOTES_OFF)
		    && voice->envelopes[0].state < EnvelopeStage::RELEASE) {

			if ((arpSettings != nullptr) && arpSettings->mode != ArpMode::OFF) {
				goto justSwitchOff;
			}

			// LEGATO/MONO note-stack handling skipped for simplicity in test harness
			// (would need full Arpeggiator note lookup + SoundInstrument cast)

justSwitchOff:
			voice->noteOff(modelStack, true, noteCode == ALL_NOTES_OFF);
		}
	}
}
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
// Real implementations — Sound state query methods
bool Sound::hasFilters() {
	return (lpfMode != FilterMode::OFF || hpfMode != FilterMode::OFF);
}
bool Sound::isSourceActiveCurrently(int32_t s, ParamManagerForTimeline* paramManager) {
	return (synthMode == SynthMode::RINGMOD
	        || getSmoothedPatchedParamValue(params::LOCAL_OSC_A_VOLUME + s, *paramManager) != -2147483648)
	       && (synthMode == SynthMode::FM || sources[s].oscType != OscType::SAMPLE
	           || sources[s].hasAtLeastOneAudioFileLoaded());
}

bool Sound::isSourceActiveEverDisregardingMissingSample(int32_t s, ParamManager* paramManager) {
	return (synthMode == SynthMode::RINGMOD
	        || paramManager->getPatchedParamSet()->params[params::LOCAL_OSC_A_VOLUME + s].containsSomething(-2147483648)
	        || renderingOscillatorSyncEver(paramManager));
}

bool Sound::isSourceActiveEver(int32_t s, ParamManager* paramManager) {
	return isSourceActiveEverDisregardingMissingSample(s, paramManager)
	       && (synthMode == SynthMode::FM || sources[s].oscType != OscType::SAMPLE
	           || sources[s].hasAtLeastOneAudioFileLoaded());
}

bool Sound::isNoiseActiveEver(ParamManagerForTimeline* paramManager) {
	return (synthMode != SynthMode::FM
	        && paramManager->getPatchedParamSet()->params[params::LOCAL_NOISE_VOLUME].containsSomething(-2147483648));
}

bool Sound::envelopeHasSustainCurrently(int32_t e, ParamManagerForTimeline* paramManager) {
	PatchedParamSet* patchedParams = paramManager->getPatchedParamSet();
	return (patchedParams->getValue(params::LOCAL_ENV_0_SUSTAIN + e) != -2147483648
	        || patchedParams->getValue(params::LOCAL_ENV_0_DECAY + e)
	               > patchedParams->getValue(params::LOCAL_ENV_0_RELEASE + e));
}

bool Sound::envelopeHasSustainEver(int32_t e, ParamManagerForTimeline* paramManager) {
	PatchedParamSet* patchedParams = paramManager->getPatchedParamSet();
	return (patchedParams->params[params::LOCAL_ENV_0_SUSTAIN + e].containsSomething(-2147483648)
	        || patchedParams->params[params::LOCAL_ENV_0_DECAY + e].isAutomated()
	        || patchedParams->params[params::LOCAL_ENV_0_RELEASE + e].isAutomated()
	        || patchedParams->getValue(params::LOCAL_ENV_0_DECAY + e)
	               > patchedParams->getValue(params::LOCAL_ENV_0_RELEASE + e));
}

bool Sound::renderingOscillatorSyncCurrently(ParamManagerForTimeline* paramManager) {
	if (!oscillatorSync) {
		return false;
	}
	if (synthMode == SynthMode::FM) {
		return false;
	}
	return (getSmoothedPatchedParamValue(params::LOCAL_OSC_B_VOLUME, *paramManager) != -2147483648
	        || synthMode == SynthMode::RINGMOD);
}

bool Sound::renderingOscillatorSyncEver(ParamManager* paramManager) {
	if (!oscillatorSync) {
		return false;
	}
	if (synthMode == SynthMode::FM) {
		return false;
	}
	return (paramManager->getPatchedParamSet()->params[params::LOCAL_OSC_B_VOLUME].containsSomething(-2147483648)
	        || synthMode == SynthMode::RINGMOD);
}
void Sound::setupAsBlankSynth(ParamManager*, bool) {}
void Sound::setupAsDefaultSynth(ParamManager*) {}
void Sound::setupAsSample(ParamManagerForTimeline*) {}
void Sound::recalculateAllVoicePhaseIncrements(ModelStackWithSoundFlags*) {}
// Real implementation — regression test for fix/multisample-transpose-retrigger
void Sound::retriggerVoicesForTransposeChange(ModelStackWithSoundFlags* modelStack) {
	if (voices_.empty() || modelStack == nullptr) {
		return;
	}

	// Check if any source uses multisamples (multiple zones)
	bool hasMultisamples = false;
	for (int32_t s = 0; s < kNumSources; s++) {
		if (synthMode != SynthMode::FM && sources[s].oscType == OscType::SAMPLE
		    && sources[s].ranges.getNumElements() > 1) {
			hasMultisamples = true;
			break;
		}
	}

	// No multisamples — existing behavior is correct
	if (!hasMultisamples) {
		recalculateAllVoicePhaseIncrements(modelStack);
		return;
	}

	// For each voice, check if the multisample zone changed and re-trigger if so
	for (auto it = voices_.begin(); it != voices_.end();) {
		const ActiveVoice& voice = *it;

		bool rangeChanged = false;
		for (int32_t s = 0; s < kNumSources; s++) {
			if (synthMode != SynthMode::FM && sources[s].oscType == OscType::SAMPLE
			    && sources[s].ranges.getNumElements() > 1) {
				MultiRange* newRange = sources[s].getRange(voice->noteCodeAfterArpeggiation + transpose);
				if (newRange && newRange->getAudioFileHolder() != voice->guides[s].audioFileHolder) {
					rangeChanged = true;
					break;
				}
			}
		}

		if (!rangeChanged) {
			voice->calculatePhaseIncrements(modelStack);
			++it;
			continue;
		}

		// Zone changed — re-trigger the voice with recovered parameters
		int32_t notePreArp = voice->inputCharacteristics[util::to_underlying(MIDICharacteristic::NOTE)];
		int32_t notePostArp = voice->noteCodeAfterArpeggiation;
		int32_t midiChannel = voice->inputCharacteristics[util::to_underlying(MIDICharacteristic::CHANNEL)];

		int32_t velSrc = voice->sourceValues[util::to_underlying(PatchSource::VELOCITY)];
		int32_t velRecovered = std::clamp((velSrc / 33554432) + 64, (int32_t)0, (int32_t)127);
		uint8_t velocity = (velSrc >= 2147483647) ? 128 : static_cast<uint8_t>(velRecovered);

		int16_t mpeValues[kNumExpressionDimensions];
		for (int32_t m = 0; m < kNumExpressionDimensions; m++) {
			mpeValues[m] = static_cast<int16_t>(voice->localExpressionSourceValuesBeforeSmoothing[m] >> 16);
		}

		uint32_t syncLength = voice->guides[0].sequenceSyncLengthTicks;

		bool success = voice->noteOn(modelStack, notePreArp, notePostArp, velocity, syncLength, 0, 0, true, midiChannel,
		                             mpeValues);
		if (!success) {
			freeActiveVoice(voice, modelStack, false);
			it = voices_.erase(it);
			continue;
		}
		++it;
	}
}
Error Sound::loadAllAudioFiles(bool) { return Error::NONE; }
void Sound::modButtonAction(uint8_t, bool, ParamManagerForTimeline*) {}
bool Sound::modEncoderButtonAction(uint8_t, bool, ModelStackWithThreeMainThings*) { return false; }
void Sound::polyphonicExpressionEventOnChannelOrNote(int32_t, int32_t, int32_t, MIDICharacteristic) {}
int16_t Sound::getMaxOscTranspose(InstrumentClip*) { return 96; }
int16_t Sound::getMinOscTranspose() { return -96; }
// Real implementation — the fix for #4232 (patch cables broken after FM switch)
// requires filter modes to be updated BEFORE setupPatchingForAllParamManagers().
void Sound::setSynthMode(SynthMode value, Song* song) {
	killAllVoices();

	SynthMode oldSynthMode = synthMode;
	synthMode = value;

	// Update filter modes BEFORE setting up patching, so patch cables correctly
	// reflect the new synth mode's filter state (fixes #4232)
	if (synthMode == SynthMode::FM && oldSynthMode != SynthMode::FM) {
		lpfMode = FilterMode::OFF;
		hpfMode = FilterMode::OFF;
	}
	else if (synthMode != SynthMode::FM && oldSynthMode == SynthMode::FM) {
		if (lpfMode == FilterMode::OFF) {
			lpfMode = FilterMode::TRANSISTOR_24DB;
		}
		if (hpfMode == FilterMode::OFF) {
			hpfMode = FilterMode::HPLADDER;
		}
	}

	setupPatchingForAllParamManagers(song);

	// Change mod knob functions over. Switching *to* FM...
	if (synthMode == SynthMode::FM && oldSynthMode != SynthMode::FM) {
		for (int32_t f = 0; f < kNumModButtons; f++) {
			if (modKnobs[f][0].paramDescriptor.isJustAParam() && modKnobs[f][1].paramDescriptor.isJustAParam()) {
				int32_t p0 = modKnobs[f][0].paramDescriptor.getJustTheParam();
				int32_t p1 = modKnobs[f][1].paramDescriptor.getJustTheParam();

				if ((p0 == params::LOCAL_LPF_RESONANCE || p0 == params::LOCAL_HPF_RESONANCE
				     || p0 == params::UNPATCHED_START + params::UNPATCHED_BASS)
				    && (p1 == params::LOCAL_LPF_FREQ || p1 == params::LOCAL_HPF_FREQ
				        || p1 == params::UNPATCHED_START + params::UNPATCHED_TREBLE)) {
					modKnobs[f][0].paramDescriptor.setToHaveParamOnly(params::LOCAL_MODULATOR_1_VOLUME);
					modKnobs[f][1].paramDescriptor.setToHaveParamOnly(params::LOCAL_MODULATOR_0_VOLUME);
				}
			}
		}
	}

	// ... and switching *from* FM...
	if (synthMode != SynthMode::FM && oldSynthMode == SynthMode::FM) {
		for (int32_t f = 0; f < kNumModButtons; f++) {
			if (modKnobs[f][0].paramDescriptor.isSetToParamWithNoSource(params::LOCAL_MODULATOR_1_VOLUME)
			    && modKnobs[f][1].paramDescriptor.isSetToParamWithNoSource(params::LOCAL_MODULATOR_0_VOLUME)) {
				modKnobs[f][0].paramDescriptor.setToHaveParamOnly(params::LOCAL_LPF_RESONANCE);
				modKnobs[f][1].paramDescriptor.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
			}
		}
	}
}
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

// Private methods needed by noteOn
void Sound::getArpBackInTimeAfterSkippingRendering(ArpeggiatorSettings* arpSettings) {
	if (skippingRendering) {
		if (arpSettings != nullptr && arpSettings->mode != ArpMode::OFF) {
			uint32_t phaseIncrement =
			    arpSettings->getPhaseIncrement(paramFinalValues[params::GLOBAL_ARP_RATE - params::FIRST_GLOBAL]);
			getArp()->gatePos +=
			    (phaseIncrement >> 8) * (AudioEngine::audioSampleTimer - timeStartedSkippingRenderingArp);
			timeStartedSkippingRenderingArp = AudioEngine::audioSampleTimer;
		}
	}
}

void Sound::stopSkippingRendering(ArpeggiatorSettings*) {}
void Sound::startSkippingRendering(ModelStackWithSoundFlags*) {}

void Sound::deleteMultiRange(int32_t, int32_t) {}
uint32_t Sound::getSyncedLFOPhaseIncrement(const LFOConfig&) { return 0; }
void Sound::process_postarp_notes(ModelStackWithSoundFlags*, ArpeggiatorSettings*, ArpReturnInstruction) {}
