// Stubs for NoteRow constructor/destructor.
// ParamManager/ParamManagerForTimeline stubs moved to real param_manager.cpp (Phase H2).

#include "model/note/note_row.h"

// ── NoteRow stubs ────────────────────────────────────────────────────────

NoteRow::NoteRow(int16_t newY)
    : y(newY), muted(false), mutedBeforeStemExport(false), exportStem(false), loopLengthIfIndependent(0),
      lastProcessedPosIfIndependent(0), repeatCountIfIndependent(0), currentlyPlayingReversedIfIndependent(false),
      sequenceDirectionMode(SequenceDirection::FORWARD), drum(nullptr), firstOldDrumName(nullptr),
      probabilityValue(0), fillValue(0), colourOffset(0), sequenced(false), ignoreNoteOnsBefore_(0) {
}

NoteRow::~NoteRow() = default;

// Phase 11: consequence_note_row_length stubs
bool NoteRow::hasIndependentPlayPos() {
	return loopLengthIfIndependent != 0;
}

void NoteRow::setLength(ModelStackWithNoteRow* modelStack, int32_t newLength, Action* actionToRecordTo, int32_t oldPos,
                        bool hadIndependentPlayPosBefore) {
	(void)modelStack;
	(void)actionToRecordTo;
	(void)oldPos;
	(void)hadIndependentPlayPosBefore;
	loopLengthIfIndependent = newLength;
}

// Phase 15: consequence_note_row_mute stub
void NoteRow::toggleMute(ModelStackWithNoteRow* modelStack, bool clipIsActiveAndPlaybackIsOn) {
	(void)modelStack;
	(void)clipIsActiveAndPlaybackIsOn;
	muted = !muted;
}
