#pragma once

#include "clip_mocks.h"
#include "util/d_string.h"

#include <cstdint>

class Song {
public:
	void deleteClipObject(Clip* clip, bool x, InstrumentRemoval removal) {
		(void)x;
		(void)removal;
		clip->deleted = true;
	}
	void clear() {
		sessionClips.clear();
		arrangementOnlyClips.clear();
	}
	void getNoteLengthName(StringBuf& buffer, uint32_t noteLength, char const* notesString = "-notes",
	                       bool clarifyPerColumn = false) const {
		(void)buffer;
		(void)noteLength;
		(void)notesString;
		(void)clarifyPerColumn;
	}

	ClipArray sessionClips;
	ClipArray arrangementOnlyClips;
	int32_t insideWorldTickMagnitude = 1;
	int32_t insideWorldTickMagnitudeOffsetFromBPM = 0;

	// Phase 10: consequence system stubs
	int8_t swingAmount = 0;

	float getTimePerTimerTickFloat() { return 22675.7f; } // ~120 BPM
	void setTimePerTimerTick(uint64_t newTimeBig, bool doSongUpdate = true) {
		(void)newTimeBig;
		(void)doSongUpdate;
	}
	void removeYNoteFromMode(int32_t noteWithinOctave) { (void)noteWithinOctave; }
	bool isClipActive(Clip* clip) {
		(void)clip;
		return false;
	}
};

extern Song* currentSong;
extern Song* preLoadedSong;
extern Song testSong;
