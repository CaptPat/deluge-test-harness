#pragma once

#include "definitions_cxx.hpp"
#include "model/timeline_counter.h"

#include <cstdint>
#include <climits>
#include <vector>
#include <map>

class NoteRow;
class ModelStackWithNoteRow;

class Clip : public TimelineCounter {
public:
	Clip(int id_ = -1) : id(id_) {}
	int id;
	ClipType type = ClipType::INSTRUMENT;
	bool deleted = false;
	int32_t loopLength = 0;
	uint8_t section = 0;
	bool soloingInSessionMode = false;
	bool activeIfNoSolo = false;
	ArmState armState = ArmState::OFF;

	bool isArrangementOnlyClip() { return false; }

	// TimelineCounter pure virtual overrides
	int32_t getLastProcessedPos() const override { return 0; }
	uint32_t getLivePos() const override { return 0; }
	int32_t getLoopLength() const override { return loopLength; }
	bool isPlayingAutomationNow() const override { return false; }
	bool backtrackingCouldLoopBackToEnd() const override { return false; }
	int32_t getPosAtWhichPlaybackWillCut(ModelStackWithTimelineCounter const* ms) const override {
		(void)ms;
		return INT32_MAX;
	}
	void getActiveModControllable(ModelStackWithTimelineCounter* ms) override { (void)ms; }
	void expectEvent() override {}
	TimelineCounter* getTimelineCounterToRecordTo() override { return this; }

	// Phase 11: consequence stubs
	virtual void shiftHorizontally(ModelStackWithTimelineCounter* ms, int32_t amount, bool shiftAutomation,
	                               bool shiftSequenceAndMPE) {
		(void)ms;
		(void)amount;
		(void)shiftAutomation;
		(void)shiftSequenceAndMPE;
	}
};

class ModelStackWithTimelineCounter;

class InstrumentClip : public Clip {
public:
	// Map from noteRowId to NoteRow* for consequence tests
	std::map<int32_t, NoteRow*> noteRowMap;

	NoteRow* getNoteRowFromId(int32_t id) {
		auto it = noteRowMap.find(id);
		return (it != noteRowMap.end()) ? it->second : nullptr;
	}

	void shiftOnlyOneNoteRowHorizontally(ModelStackWithNoteRow* modelStack, int32_t shiftAmount,
	                                     bool shiftAutomation, bool shiftSequenceAndMPE) {
		(void)modelStack;
		lastShiftAmount = shiftAmount;
		lastShiftAutomation = shiftAutomation;
		lastShiftSequenceAndMPE = shiftSequenceAndMPE;
	}

	// Phase 16: consequence_instrument_clip_multiply support
	void halveNoteRowsWithIndependentLength(ModelStackWithTimelineCounter* modelStack) {
		(void)modelStack;
		halveNoteRowsCalled = true;
	}

	// Test inspection
	int32_t lastShiftAmount = 0;
	bool lastShiftAutomation = false;
	bool lastShiftSequenceAndMPE = false;
	bool halveNoteRowsCalled = false;
};

class AudioClip : public Clip {};

class ClipArray {
public:
	Clip** getElementAddress(int32_t index) { return &data[index]; }
	Clip* getClipAtIndex(int32_t index) { return data[index]; }
	int32_t getNumElements() { return static_cast<int32_t>(data.size()); }
	void deleteAtIndex(int32_t index) {
		for (int32_t i = index; i < static_cast<int32_t>(data.size()) - 1; i++) {
			data[i] = data[i + 1];
		}
		data.pop_back();
	}
	void clear() { data.clear(); }
	void push(Clip* clip) { data.push_back(clip); }

private:
	std::vector<Clip*> data;
};
