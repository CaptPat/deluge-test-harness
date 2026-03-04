#pragma once

#include <cstdint>
#include <vector>

// Minimal Clip/Song mocks for the test harness.
// These mirror the firmware's test mocks with extensions for integration testing.

enum class ClipType { INSTRUMENT, AUDIO };
enum class InstrumentRemoval { NONE };

class Clip {
public:
	Clip(int id_ = -1) : id(id_) {}
	int id;
	ClipType type = ClipType::INSTRUMENT;
	bool deleted = false;
};

class ClipArray {
public:
	Clip** getElementAddress(int32_t index) { return &data[index]; }
	Clip* getClipAtIndex(int32_t index) { return data[index]; }
	int32_t getNumElements() { return static_cast<int32_t>(data.size()); }
	void clear() { data.clear(); }
	void push(Clip* clip) { data.push_back(clip); }

private:
	std::vector<Clip*> data;
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

	ClipArray sessionClips;
	ClipArray arrangementOnlyClips;
	int32_t insideWorldTickMagnitude = 1;
};

extern Song* currentSong;
extern Song testSong;
