// Stub implementations for Source class methods.
// The real source.cpp pulls in sample_browser.h, sound_editor.h, audio_file_manager.h, etc.
// We provide minimal stubs so Source can be embedded in Sound.

#include "processing/source.h"

Source::Source()
    : oscType(OscType::SQUARE)
    , transpose(0)
    , cents(0)
    , dxPatch(nullptr)
    , repeatMode(SampleRepeatMode::CUT)
    , timeStretchAmount(0)
    , defaultRangeI(-1) {}

Source::~Source() {
	// Don't call destructAllMultiRanges() — no real allocations in test harness
}

bool Source::renderInStereo(Sound* /*s*/, SampleHolder* /*sampleHolder*/) { return false; }
void Source::setCents(int32_t newCents) { cents = static_cast<int8_t>(newCents); }
void Source::recalculateFineTuner() {}
int32_t Source::getLengthInSamplesAtSystemSampleRate(int32_t /*note*/, bool /*forTimeStretching*/) { return 0; }
void Source::detachAllAudioFiles() {}
Error Source::loadAllSamples(bool /*mayActuallyReadFiles*/) { return Error::NONE; }
void Source::setReversed(bool /*newReversed*/) {}
int32_t Source::getRangeIndex(int32_t /*note*/) { return 0; }
MultiRange* Source::getRange(int32_t /*note*/) { return nullptr; }
MultiRange* Source::getOrCreateFirstRange() { return nullptr; }
bool Source::hasAtLeastOneAudioFileLoaded() { return false; }
void Source::doneReadingFromFile(Sound* /*sound*/) {}
bool Source::hasAnyLoopEndPoint() { return false; }
void Source::setOscType(OscType newType) { oscType = newType; }
DxPatch* Source::ensureDxPatch() { return nullptr; }
void Source::destructAllMultiRanges() {}
