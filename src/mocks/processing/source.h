// Shadow header replacing firmware's processing/source.h
// The real source.cpp pulls in sample_browser.h, sound_editor.h, audio_file_manager.h, etc.
// We provide the full class layout with stub methods so Sound can embed Source[2] by value.

#pragma once

#include "definitions_cxx.hpp"
#include "model/sample/sample_controls.h"
#include "storage/multi_range/multi_range_array.h"
#include "util/d_string.h"
#include "util/packed_filenames.h"
#include "util/phase_increment_fine_tuner.h"

class Sound;
class ParamManagerForTimeline;
class WaveTable;
class SampleHolder;
class DxPatch;

class Source {
public:
	Source();
	~Source();

	SampleControls sampleControls;

	OscType oscType;

	int16_t transpose;
	int8_t cents;
	PhaseIncrementFineTuner fineTuner;

	MultiRangeArray ranges;

	String dirPath; // Shared directory prefix for multisample ranges (with trailing slash).
	                // Empty when ranges have mixed directories (full paths stored per holder).

	PackedFilenames packedNames; // Packed filenames for all ranges (single allocation).
	                             // When populated, holder filePaths are empty.

	DxPatch* dxPatch;
	bool dxPatchChanged = false;
	SampleRepeatMode repeatMode;

	int8_t timeStretchAmount;

	int16_t defaultRangeI; // -1 means none yet

	// Stub declarations — implemented in source_stubs.cpp
	bool renderInStereo(Sound* s, SampleHolder* sampleHolder = nullptr);
	void setCents(int32_t newCents);
	void recalculateFineTuner();
	int32_t getLengthInSamplesAtSystemSampleRate(int32_t note, bool forTimeStretching = false);
	void detachAllAudioFiles();
	Error loadAllSamples(bool mayActuallyReadFiles);
	void setReversed(bool newReversed);
	int32_t getRangeIndex(int32_t note);
	MultiRange* getRange(int32_t note);
	MultiRange* getOrCreateFirstRange();
	bool hasAtLeastOneAudioFileLoaded();
	void doneReadingFromFile(Sound* sound);
	bool hasAnyLoopEndPoint();
	void setOscType(OscType newType);
	void revertToFullPaths();

	/// Get filename for range at index. Returns packed name or holder filePath.
	const char* getFilename(int32_t rangeIndex);

	/// Reconstruct full path (dirPath + filename) into out. Returns Error on alloc failure.
	Error getFullPath(int32_t rangeIndex, String& out);

	/// Whether filenames are stored in packed buffer (vs per-holder Strings).
	bool isPackedMode() const { return !packedNames.isEmpty(); }

	/// Move filenames from packed buffer to per-holder filePaths. Clears packed buffer.
	void unpackFilenames();

	DxPatch* ensureDxPatch();

private:
	void destructAllMultiRanges();
};
