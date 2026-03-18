// Stubs for AudioFileHolder, AudioFile, SampleHolder, SampleHolderForVoice,
// SamplePlaybackGuide — too many deep deps to compile the real .cpp files.

#include "storage/audio/audio_file_holder.h"
#include "storage/audio/audio_file.h"
#include "model/sample/sample_holder.h"
#include "model/sample/sample_holder_for_voice.h"
#include "model/sample/sample_playback_guide.h"

// ── AudioFileHolder ─────────────────────────────────────────────────────
AudioFileHolder::AudioFileHolder() : audioFile(nullptr), audioFileType(AudioFileType::SAMPLE) {}
AudioFileHolder::~AudioFileHolder() = default;

void AudioFileHolder::setAudioFile(AudioFile* newAudioFile, bool reversed, bool manuallySelected,
                                   int32_t clusterLoadInstruction) {
	audioFile = newAudioFile;
}

Error AudioFileHolder::loadFile(bool reversed, bool manuallySelected, bool mayActuallyReadFile,
                                int32_t clusterLoadInstruction, FilePointer* filePointer,
                                bool makeWaveTableWorkAtAllCosts) {
	return Error::NONE;
}

Error AudioFileHolder::loadFileWithDir(String& /*dirPath*/, bool reversed, bool manuallySelected,
                                       bool mayActuallyReadFile, int32_t clusterLoadInstruction,
                                       FilePointer* filePointer, bool makeWaveTableWorkAtAllCosts) {
	return Error::NONE;
}

// ── AudioFile ───────────────────────────────────────────────────────────
void AudioFile::addReason() {
	numReasonsToBeLoaded++;
}

void AudioFile::removeReason(char const* errorCode) {
	numReasonsToBeLoaded--;
}

bool AudioFile::mayBeStolen(void* thingNotToStealFrom) {
	return numReasonsToBeLoaded <= 0;
}

void AudioFile::steal(char const* errorCode) {}

StealableQueue AudioFile::getAppropriateQueue() {
	return StealableQueue::NO_SONG_SAMPLE_DATA;
}

Error AudioFile::loadFile(AudioFileReader* reader, bool isAiff, bool makeWaveTableWorkAtAllCosts) {
	return Error::NONE;
}

// ── SamplePlaybackGuide ─────────────────────────────────────────────────
SamplePlaybackGuide::SamplePlaybackGuide()
    : playDirection(1), audioFileHolder(nullptr), startPlaybackAtByte(0), endPlaybackAtByte(0),
      sequenceSyncStartedAtTick(0), sequenceSyncLengthTicks(0) {}

int32_t SamplePlaybackGuide::getFinalClusterIndex(Sample* sample, bool obeyMarkers, int32_t* getEndPlaybackAtByte) {
	return 0;
}

void SamplePlaybackGuide::setupPlaybackBounds(bool reversed) {}

uint64_t SamplePlaybackGuide::getSyncedNumSamplesIn() { return 0; }

int32_t SamplePlaybackGuide::getNumSamplesLaggingBehindSync(VoiceSample* voiceSample) { return 0; }

int32_t SamplePlaybackGuide::adjustPitchToCorrectDriftFromSync(VoiceSample* voiceSample, int32_t phaseIncrement) {
	return phaseIncrement;
}

// ── SampleHolder ────────────────────────────────────────────────────────
SampleHolder::SampleHolder()
    : startPos(0), endPos(9999999), waveformViewScroll(0), waveformViewZoom(0), neutralPhaseIncrement(0) {
	audioFileType = AudioFileType::SAMPLE;
	for (auto& c : clustersForStart) {
		c = nullptr;
	}
}

SampleHolder::~SampleHolder() = default;

void SampleHolder::unassignAllClusterReasons(bool beingDestructed) {}

int64_t SampleHolder::getEndPos(bool forTimeStretching) { return endPos; }

int64_t SampleHolder::getDurationInSamples(bool forTimeStretching) { return endPos - startPos; }

void SampleHolder::beenClonedFrom(SampleHolder const* other, bool reversed) {
	startPos = other->startPos;
	endPos = other->endPos;
}

void SampleHolder::claimClusterReasons(bool reversed, int32_t clusterLoadInstruction) {}

int32_t SampleHolder::getLengthInSamplesAtSystemSampleRate(bool forTimeStretching) {
	return static_cast<int32_t>(getDurationInSamples(forTimeStretching));
}

int32_t SampleHolder::getLoopLengthAtSystemSampleRate(bool forTimeStretching) { return 0; }

void SampleHolder::setAudioFile(AudioFile* newAudioFile, bool reversed, bool manuallySelected,
                                int32_t clusterLoadInstruction) {
	audioFile = newAudioFile;
}

void SampleHolder::claimClusterReasonsForMarker(Cluster** clusters, uint32_t startPlaybackAtByte,
                                                int32_t playDirection, int32_t clusterLoadInstruction) {}

// ── SampleHolderForVoice ────────────────────────────────────────────────
SampleHolderForVoice::SampleHolderForVoice()
    : loopStartPos(0), loopEndPos(0), transpose(0), cents(0), loopLocked(false), startMSec(0), endMSec(0) {
	for (auto& c : clustersForLoopStart) {
		c = nullptr;
	}
}

SampleHolderForVoice::~SampleHolderForVoice() = default;

void SampleHolderForVoice::unassignAllClusterReasons(bool beingDestructed) {}

void SampleHolderForVoice::setCents(int32_t newCents) { cents = newCents; }

void SampleHolderForVoice::recalculateFineTuner() {}

void SampleHolderForVoice::claimClusterReasons(bool reversed, int32_t clusterLoadInstruction) {}

void SampleHolderForVoice::setTransposeAccordingToSamplePitch(bool minimizeOctaves, bool doingSingleCycle,
                                                               bool rangeCoversJustOneNote, bool thatOneNote) {}

uint32_t SampleHolderForVoice::getMSecLimit(Source* source) { return 0; }

void SampleHolderForVoice::sampleBeenSet(bool reversed, bool manuallySelected) {}
