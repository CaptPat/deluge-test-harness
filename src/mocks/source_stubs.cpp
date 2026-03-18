// Stub implementations for Source class methods.
// The real source.cpp pulls in sample_browser.h, sound_editor.h, audio_file_manager.h, etc.
// We provide minimal stubs so Source can be embedded in Sound.
// Methods that support packed filenames / dirPath use real implementations.

#include "processing/source.h"
#include "storage/audio/audio_file_holder.h"
#include "storage/multi_range/multi_range.h"

#include <cstring>

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
// Real implementation — needed for retriggerVoicesForTransposeChange() tests
MultiRange* Source::getRange(int32_t note) {
	if (ranges.getNumElements() == 1) {
		return ranges.getElement(0);
	}
	else if (ranges.getNumElements() == 0) {
		return nullptr;
	}
	else {
		defaultRangeI = ranges.search(note, GREATER_OR_EQUAL);
		if (defaultRangeI == ranges.getNumElements()) {
			defaultRangeI--;
		}
		return ranges.getElement(defaultRangeI);
	}
}
MultiRange* Source::getOrCreateFirstRange() { return nullptr; }
bool Source::hasAtLeastOneAudioFileLoaded() { return false; }
void Source::doneReadingFromFile(Sound* /*sound*/) {}
bool Source::hasAnyLoopEndPoint() { return false; }
void Source::setOscType(OscType newType) { oscType = newType; }
DxPatch* Source::ensureDxPatch() { return nullptr; }
void Source::destructAllMultiRanges() {}

// ── Real implementations for packed filenames / dirPath ─────────────────

const char* Source::getFilename(int32_t rangeIndex) {
	if (!packedNames.isEmpty() && rangeIndex >= 0 && rangeIndex < packedNames.getCount()) {
		return packedNames.getName(rangeIndex);
	}
	if (rangeIndex >= 0 && rangeIndex < ranges.getNumElements()) {
		return ranges.getElement(rangeIndex)->getAudioFileHolder()->filePath.get();
	}
	return "";
}

Error Source::getFullPath(int32_t rangeIndex, String& out) {
	const char* filename = getFilename(rangeIndex);
	if (!filename || !*filename) {
		out.clear();
		return Error::NONE;
	}
	if (!dirPath.isEmpty()) {
		Error error = out.set(dirPath.get());
		if (error != Error::NONE) {
			return error;
		}
		return out.concatenate(filename);
	}
	return out.set(filename);
}

void Source::revertToFullPaths() {
	if (!packedNames.isEmpty()) {
		for (int32_t e = 0; e < ranges.getNumElements() && e < packedNames.getCount(); e++) {
			AudioFileHolder* holder = ranges.getElement(e)->getAudioFileHolder();
			const char* name = packedNames.getName(e);
			if (name && *name && !dirPath.isEmpty()) {
				String fullPath;
				fullPath.set(dirPath.get());
				fullPath.concatenate(name);
				holder->filePath.set(&fullPath);
			}
			else if (name && *name) {
				holder->filePath.set(name);
			}
		}
		packedNames.clear();
	}
	else if (!dirPath.isEmpty()) {
		for (int32_t e = 0; e < ranges.getNumElements(); e++) {
			AudioFileHolder* holder = ranges.getElement(e)->getAudioFileHolder();
			if (!holder->filePath.isEmpty()) {
				String fullPath;
				fullPath.set(dirPath.get());
				fullPath.concatenate(&holder->filePath);
				holder->filePath.set(&fullPath);
			}
		}
	}
	dirPath.clear();
}

void Source::unpackFilenames() {
	if (packedNames.isEmpty()) {
		return;
	}
	for (int32_t e = 0; e < ranges.getNumElements() && e < packedNames.getCount(); e++) {
		AudioFileHolder* holder = ranges.getElement(e)->getAudioFileHolder();
		const char* name = packedNames.getName(e);
		if (name && *name) {
			holder->filePath.set(name);
		}
	}
	packedNames.clear();
}
