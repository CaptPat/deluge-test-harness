// Stub implementations for ModControllableAudio.
// The real .cpp pulls in storage_manager, action_logger, root_ui, etc.
// Sound inherits from ModControllableAudio, so we need these linked.

#include "model/mod_controllable/mod_controllable_audio.h"

ModControllableAudio::ModControllableAudio() {
	bassFreq = 0;
	trebleFreq = 0;
	withoutTrebleL = 0;
	bassOnlyL = 0;
	withoutTrebleR = 0;
	bassOnlyR = 0;
	sampleRateReductionOnLastTime = false;
	clippingAmount = 0;
	lpfMode = FilterMode::TRANSISTOR_24DB;
	hpfMode = FilterMode::HPLADDER;
	filterRoute = FilterRoute::HIGH_TO_LOW;
	lowSampleRatePos = 0;
	highSampleRatePos = 0;
}

ModControllableAudio::~ModControllableAudio() = default;
void ModControllableAudio::cloneFrom(ModControllableAudio*) {}
void ModControllableAudio::processStutter(std::span<StereoSample>, ParamManager*) {}
void ModControllableAudio::processReverbSendAndVolume(std::span<StereoSample>, int32_t*, int32_t, int32_t, int32_t,
                                                      int32_t, bool) {}
void ModControllableAudio::writeAttributesToFile(Serializer&) {}
void ModControllableAudio::writeTagsToFile(Serializer&) {}
Error ModControllableAudio::readTagFromFile(Deserializer&, char const*, ParamManagerForTimeline*, int32_t,
                                            ArpeggiatorSettings*, Song*) {
	return Error::NONE;
}
void ModControllableAudio::processSRRAndBitcrushing(std::span<StereoSample>, int32_t*, ParamManager*) {}
void ModControllableAudio::writeParamAttributesToFile(Serializer&, ParamManager*, bool, int32_t*) {}
void ModControllableAudio::writeParamTagsToFile(Serializer&, ParamManager*, bool, int32_t*) {}
bool ModControllableAudio::readParamTagFromFile(Deserializer&, char const*, ParamManagerForTimeline*, int32_t) {
	return false;
}
void ModControllableAudio::initParams(ParamManager*) {}
void ModControllableAudio::wontBeRenderedForAWhile() {}
void ModControllableAudio::beginStutter(ParamManagerForTimeline*) {}
void ModControllableAudio::endStutter(ParamManagerForTimeline*) {}
bool ModControllableAudio::setModFXType(ModFXType newType) {
	modFXType_ = newType;
	return true;
}
bool ModControllableAudio::offerReceivedCCToLearnedParamsForClip(MIDICable&, uint8_t, uint8_t, uint8_t,
                                                                 ModelStackWithTimelineCounter*, int32_t) {
	return false;
}
bool ModControllableAudio::offerReceivedCCToLearnedParamsForSong(MIDICable&, uint8_t, uint8_t, uint8_t,
                                                                 ModelStackWithThreeMainThings*) {
	return false;
}
bool ModControllableAudio::offerReceivedPitchBendToLearnedParams(MIDICable&, uint8_t, uint8_t, uint8_t,
                                                                 ModelStackWithTimelineCounter*, int32_t) {
	return false;
}
bool ModControllableAudio::learnKnob(MIDICable*, ParamDescriptor, uint8_t, uint8_t, uint8_t, Song*) { return false; }
bool ModControllableAudio::unlearnKnobs(ParamDescriptor, Song*) { return false; }
bool ModControllableAudio::isBitcrushingEnabled(ParamManager*) { return false; }
bool ModControllableAudio::isSRREnabled(ParamManager*) { return false; }
bool ModControllableAudio::hasBassAdjusted(ParamManager*) { return false; }
bool ModControllableAudio::hasTrebleAdjusted(ParamManager*) { return false; }
ModelStackWithAutoParam* ModControllableAudio::getParamFromMIDIKnob(MIDIKnob&, ModelStackWithThreeMainThings*) {
	return nullptr;
}
void ModControllableAudio::processFX(std::span<StereoSample>, ModFXType, int32_t, int32_t, const Delay::State&,
                                     int32_t*, ParamManager*, bool, q31_t) {}
void ModControllableAudio::switchDelayPingPong() {}
void ModControllableAudio::switchDelayAnalog() {}
void ModControllableAudio::switchDelaySyncType() {}
void ModControllableAudio::switchDelaySyncLevel() {}
void ModControllableAudio::switchLPFMode() {}
void ModControllableAudio::switchHPFMode() {}
void ModControllableAudio::clearModFXMemory() {}
char const* ModControllableAudio::getFilterTypeDisplayName(FilterType) { return ""; }
char const* ModControllableAudio::getFilterModeDisplayName(FilterType) { return ""; }
char const* ModControllableAudio::getLPFModeDisplayName() { return ""; }
char const* ModControllableAudio::getHPFModeDisplayName() { return ""; }
char const* ModControllableAudio::getDelayTypeDisplayName() { return ""; }
char const* ModControllableAudio::getDelayPingPongStatusDisplayName() { return ""; }
char const* ModControllableAudio::getDelaySyncTypeDisplayName() { return ""; }
void ModControllableAudio::getDelaySyncLevelDisplayName(char*) {}
char const* ModControllableAudio::getSidechainDisplayName() { return ""; }
void ModControllableAudio::displayFilterSettings(bool, FilterType) {}
void ModControllableAudio::displayDelaySettings(bool) {}
void ModControllableAudio::displaySidechainAndReverbSettings(bool) {}
void ModControllableAudio::displayOtherModKnobSettings(uint8_t, bool) {}
bool ModControllableAudio::enableGrain() { return false; }
void ModControllableAudio::disableGrain() {}
void ModControllableAudio::processGrainFX(std::span<StereoSample>, int32_t, int32_t, int32_t*, UnpatchedParamSet*,
                                          bool, q31_t) {}
void ModControllableAudio::doEQ(bool, bool, int32_t*, int32_t*, int32_t, int32_t) {}
ModelStackWithThreeMainThings* ModControllableAudio::addNoteRowIndexAndStuff(ModelStackWithTimelineCounter*, int32_t) {
	return nullptr;
}
void ModControllableAudio::switchHPFModeWithOff() {}
void ModControllableAudio::switchLPFModeWithOff() {}
