// Stub implementations for VoiceSamplePlaybackGuide and VoiceUnisonPartSource.
// SamplePlaybackGuide stubs are already in audio_file_stubs.cpp.

#include "model/sample/sample_playback_guide.h"
#include "model/voice/voice_sample_playback_guide.h"
#include "model/voice/voice_unison_part_source.h"

// ── VoiceSamplePlaybackGuide ───────────────────────────────────────────
VoiceSamplePlaybackGuide::VoiceSamplePlaybackGuide()
    : loopStartPlaybackAtByte(0), loopEndPlaybackAtByte(0), noteOffReceived(false) {}

void VoiceSamplePlaybackGuide::setupPlaybackBounds(bool) {}
bool VoiceSamplePlaybackGuide::shouldObeyLoopEndPointNow() { return false; }
int32_t VoiceSamplePlaybackGuide::getBytePosToStartPlayback(bool) { return static_cast<int32_t>(startPlaybackAtByte); }
int32_t VoiceSamplePlaybackGuide::getBytePosToEndOrLoopPlayback() { return static_cast<int32_t>(endPlaybackAtByte); }
LoopType VoiceSamplePlaybackGuide::getLoopingType(const Source&) const { return LoopType::NONE; }

// ── VoiceUnisonPartSource ──────────────────────────────────────────────
bool VoiceUnisonPartSource::noteOn(Voice*, Source*, VoiceSamplePlaybackGuide*, uint32_t, uint32_t, bool, SynthMode,
                                   uint8_t) {
	return true;
}
void VoiceUnisonPartSource::unassign(bool) {
	voiceSample = nullptr;
	livePitchShifter = nullptr;
	dxVoice = nullptr;
	active = false;
}
bool VoiceUnisonPartSource::getPitchAndSpeedParams(Source*, VoiceSamplePlaybackGuide*, uint32_t*, uint32_t*,
                                                   uint32_t*) {
	return false;
}
uint32_t VoiceUnisonPartSource::getSpeedParamForNoSyncing(Source*, int32_t, int32_t) { return 0; }
