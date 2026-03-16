// Stub implementations for Instrument, MelodicInstrument, and SoundInstrument.
// Provides minimal definitions so SoundInstrument is constructible.

#include "processing/sound/sound_instrument.h"
#include "model/instrument/melodic_instrument.h"
#include "model/model_stack.h"
#include "model/voice/voice.h"

// ── Instrument stubs ───────────────────────────────────────────────────
void Instrument::beenEdited(bool) {}
void Instrument::deleteAnyInstancesOfClip(InstrumentClip*) {}
bool Instrument::writeDataToFile(Serializer&, Clip*, Song*) { return false; }
bool Instrument::readTagFromFile(Deserializer&, char const*) { return false; }
Clip* Instrument::createNewClipForArrangementRecording(ModelStack*) { return nullptr; }
Error Instrument::setupDefaultAudioFileDir() { return Error::NONE; }

// ── MelodicInstrument stubs ────────────────────────────────────────────
bool MelodicInstrument::writeMelodicInstrumentAttributesToFile(Serializer&, Clip*, Song*) { return false; }
void MelodicInstrument::writeMelodicInstrumentTagsToFile(Serializer&, Clip*, Song*) {}
bool MelodicInstrument::readTagFromFile(Deserializer&, char const*) { return false; }
void MelodicInstrument::offerReceivedNote(ModelStackWithTimelineCounter*, MIDICable&, bool, int32_t, int32_t, int32_t,
                                          bool, bool*) {}
void MelodicInstrument::receivedNote(ModelStackWithTimelineCounter*, MIDICable&, bool, int32_t, MIDIMatchType, int32_t,
                                     int32_t, bool, bool*) {}
void MelodicInstrument::offerReceivedPitchBend(ModelStackWithTimelineCounter*, MIDICable&, uint8_t, uint8_t, uint8_t,
                                               bool*) {}
void MelodicInstrument::receivedPitchBend(ModelStackWithTimelineCounter*, MIDICable&, MIDIMatchType, uint8_t, uint8_t,
                                          uint8_t, bool*) {}
void MelodicInstrument::offerReceivedCC(ModelStackWithTimelineCounter*, MIDICable&, uint8_t, uint8_t, uint8_t,
                                        bool*) {}
void MelodicInstrument::receivedCC(ModelStackWithTimelineCounter*, MIDICable&, MIDIMatchType, uint8_t, uint8_t,
                                   uint8_t, bool*) {}
void MelodicInstrument::offerReceivedAftertouch(ModelStackWithTimelineCounter*, MIDICable&, int32_t, int32_t, int32_t,
                                                bool*) {}
void MelodicInstrument::receivedAftertouch(ModelStackWithTimelineCounter*, MIDICable&, MIDIMatchType, int32_t, int32_t,
                                           int32_t, bool*) {}
bool MelodicInstrument::setActiveClip(ModelStackWithTimelineCounter*, PgmChangeSend) { return false; }
bool MelodicInstrument::isNoteRowStillAuditioningAsLinearRecordingEnded(NoteRow*) { return false; }
void MelodicInstrument::stopAnyAuditioning(ModelStack*) {}
bool MelodicInstrument::isNoteAuditioning(int32_t) { return false; }
bool MelodicInstrument::isAnyAuditioningHappening() { return false; }
void MelodicInstrument::beginAuditioningForNote(ModelStack*, int32_t, int32_t, int16_t const*, int32_t, uint32_t) {}
void MelodicInstrument::endAuditioningForNote(ModelStack*, int32_t, int32_t) {}
ModelStackWithAutoParam* MelodicInstrument::getParamToControlFromInputMIDIChannel(int32_t,
                                                                                  ModelStackWithThreeMainThings*) {
	return nullptr;
}
void MelodicInstrument::processParamFromInputMIDIChannel(int32_t, int32_t, ModelStackWithTimelineCounter*) {}
void MelodicInstrument::polyphonicExpressionEventPossiblyToRecord(ModelStackWithTimelineCounter*, int32_t, int32_t,
                                                                  int32_t, MIDICharacteristic) {}
ArpeggiatorSettings* MelodicInstrument::getArpSettings(InstrumentClip*) { return nullptr; }
void MelodicInstrument::offerBendRangeUpdate(ModelStack*, MIDICable&, int32_t, int32_t, int32_t) {}
ModelStackWithAutoParam* MelodicInstrument::getModelStackWithParam(ModelStackWithTimelineCounter*, Clip*, int32_t,
                                                                   deluge::modulation::params::Kind, bool, bool) {
	return nullptr;
}
// Real implementations — pedal voice release methods
void MelodicInstrument::releaseSustainedVoices(ModelStackWithTimelineCounter* modelStack) {
	if (type != OutputType::SYNTH) {
		return;
	}
	auto* soundInstrument = static_cast<SoundInstrument*>(this);
	ModelStackWithNoteRow* modelStackWithNoteRow = modelStack->addNoteRow(0, nullptr);
	ModelStackWithThreeMainThings* modelStackWith3Things =
	    modelStackWithNoteRow->addOtherTwoThings(toModControllable(), getParamManager(modelStack->song));
	ModelStackWithSoundFlags* modelStackWithSoundFlags = modelStackWith3Things->addSoundFlags();

	for (const auto& voice : soundInstrument->voices()) {
		if ((voice->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None) {
			voice->noteOff(modelStackWithSoundFlags, true, true);
		}
	}
}

// ── SoundInstrument stubs ──────────────────────────────────────────────
SoundInstrument::SoundInstrument() : MelodicInstrument(OutputType::SYNTH) {}

bool SoundInstrument::writeDataToFile(Serializer&, Clip*, Song*) { return false; }
Error SoundInstrument::readFromFile(Deserializer&, Song*, Clip*, int32_t) { return Error::NONE; }
void SoundInstrument::cutAllSound() { killAllVoices(); }
bool SoundInstrument::noteIsOn(int32_t, bool) { return false; }
void SoundInstrument::renderOutput(ModelStack*, std::span<StereoSample>, int32_t*, int32_t, int32_t, bool, bool) {}
Error SoundInstrument::loadAllAudioFiles(bool) { return Error::NONE; }
void SoundInstrument::resyncLFOs() {}
ModControllable* SoundInstrument::toModControllable() { return this; }
bool SoundInstrument::setActiveClip(ModelStackWithTimelineCounter*, PgmChangeSend) { return false; }
void SoundInstrument::setupPatchingForAllParamManagers(Song*) {}
void SoundInstrument::setupPatching(ModelStackWithTimelineCounter*) {}
void SoundInstrument::deleteBackedUpParamManagers(Song*) {}
void SoundInstrument::polyphonicExpressionEventOnChannelOrNote(int32_t, int32_t, int32_t, MIDICharacteristic) {}
void SoundInstrument::monophonicExpressionEvent(int32_t, int32_t) {}
void SoundInstrument::sendNote(ModelStackWithThreeMainThings*, bool, int32_t, int16_t const*, int32_t, uint8_t,
                               uint32_t, int32_t, uint32_t) {}
ArpeggiatorSettings* SoundInstrument::getArpSettings(InstrumentClip*) { return &defaultArpSettings; }
bool SoundInstrument::readTagFromFile(Deserializer&, char const*) { return false; }
void SoundInstrument::prepareForHibernationOrDeletion() {}
void SoundInstrument::compensateInstrumentVolumeForResonance(ModelStackWithThreeMainThings*) {}
void SoundInstrument::loadCrucialAudioFilesOnly() {}
void SoundInstrument::beenEdited(bool) {}
int32_t SoundInstrument::doTickForwardForArp(ModelStack*, int32_t) { return 2147483647; }
void SoundInstrument::setupWithoutActiveClip(ModelStack*) {}
void SoundInstrument::getThingWithMostReverb(Sound**, ParamManager**, GlobalEffectableForClip**, int32_t*) {}
ArpeggiatorBase* SoundInstrument::getArp() { return &arpeggiator; }
