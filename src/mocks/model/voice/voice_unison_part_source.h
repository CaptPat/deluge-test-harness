// Shadow header replacing firmware's model/voice/voice_unison_part_source.h
// The real header includes model/sample/sample.h which pulls in storage/audio/audio_file.h
// and the entire cluster/stealable chain. We only need the struct layout.

#pragma once

#include "definitions_cxx.hpp"
#include <cstdint>

class TimeStretcher;
class VoiceSamplePlaybackGuide;
class Source;
class Voice;
class VoiceSample;
class LivePitchShifter;
class DxVoice;

class VoiceUnisonPartSource {
public:
	VoiceUnisonPartSource() = default;
	bool noteOn(Voice* voice, Source* source, VoiceSamplePlaybackGuide* voiceSource, uint32_t samplesLate,
	            uint32_t oscPhase, bool resetEverything, SynthMode synthMode, uint8_t velocity);
	void unassign(bool deletingSong);
	bool getPitchAndSpeedParams(Source* source, VoiceSamplePlaybackGuide* voiceSource, uint32_t* phaseIncrement,
	                            uint32_t* timeStretchRatio, uint32_t* noteLengthInSamples);
	uint32_t getSpeedParamForNoSyncing(Source* source, int32_t phaseIncrement, int32_t pitchAdjustNeutralValue);

	uint32_t oscPos;
	uint32_t phaseIncrementStoredValue;
	int32_t carrierFeedback;
	bool active;
	VoiceSample* voiceSample = nullptr;
	LivePitchShifter* livePitchShifter = nullptr;
	DxVoice* dxVoice = nullptr;
};
