#pragma once

#include "clip_mocks.h"
#include "model/fx/stutterer.h"
#include "util/d_string.h"

#include <cstdint>

class Action;
class ArpeggiatorSettings;
class Output;

// Phase 12: minimal mock for Song::globalEffectable
struct MockGlobalEffectable {
	StutterConfig stutterConfig;
};

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

	// Phase 12: param.cpp needs globalEffectable.stutterConfig
	MockGlobalEffectable globalEffectable;

	// Phase 14: Output linked-list stubs for consequence_output_existence
	Output* firstOutput = nullptr;
	void addOutput(Output* output, bool atStart = false);
	int32_t removeOutputFromMainList(Output* output);

	// Phase 11: clip consequence stubs
	Clip* currentClip = nullptr;
	Clip* getCurrentClip() { return currentClip; }
	void setCurrentClip(Clip* clip) { currentClip = clip; }
	void setClipLength(Clip* clip, int32_t length, Action* action) {
		(void)action;
		if (clip)
			clip->loopLength = length;
	}
	void doubleClipLength(Clip* clip, Action* action = nullptr) {
		(void)action;
		if (clip)
			clip->loopLength *= 2;
	}
};

extern Song* currentSong;
extern Song* preLoadedSong;
extern Song testSong;
