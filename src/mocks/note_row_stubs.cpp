// Stubs for NoteRow constructor/destructor and ParamManager constructor/destructor.
// We include note_row.h (via shadow chain) but don't compile note_row.cpp
// because it has 20+ deep dependencies.

#include "model/note/note_row.h"

// ── ParamManager stubs ───────────────────────────────────────────────────
// param_manager.cpp is not compiled (needs ParamCollection subclass implementations).

ParamManager::ParamManager() : resonanceBackwardsCompatibilityProcessed(false), expressionParamSetOffset(0) {
	for (auto& s : summaries) {
		s.paramCollection = nullptr;
		for (auto& w : s.whichParamsAreAutomated) {
			w = 0;
		}
		for (auto& w : s.whichParamsAreInterpolating) {
			w = 0;
		}
	}
}

ParamManager::~ParamManager() = default;

ParamManagerForTimeline::ParamManagerForTimeline() : ticksTilNextEvent(0), ticksSkipped(0) {
}

#if ALPHA_OR_BETA_VERSION
ParamManagerForTimeline* ParamManager::toForTimeline() {
	return static_cast<ParamManagerForTimeline*>(this);
}

ParamManagerForTimeline* ParamManagerForTimeline::toForTimeline() {
	return this;
}
#endif

// ── ParamManager non-inline methods ──────────────────────────────────────

void ParamManager::notifyParamModifiedInSomeWay(ModelStackWithAutoParam const*, int32_t, bool, bool) {
	// No-op in test harness
}

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
