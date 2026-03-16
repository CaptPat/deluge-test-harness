// Link-time stubs for the non-inline ModelStack functions from model/model_stack.h.
// These are never called in Phase 2C tests (we only use inline builder methods),
// but must exist to satisfy the linker since model_stack.cpp is not compiled.

#include "model/model_stack.h"
#include "model/note/note_row.h"
#include "processing/sound/sound.h"
#include "clip_mocks.h"
#include <cstring>

// ── ModelStackWithTimelineCounter ────────────────────────────────────────

ModelStackWithThreeMainThings*
ModelStackWithTimelineCounter::addNoteRowAndExtraStuff(int32_t noteRowIndex, NoteRow* newNoteRow) const {
	// Stub: set up with nulls for the "extra stuff" (ModControllable, ParamManager)
	auto* toReturn = (ModelStackWithThreeMainThings*)this;
	((ModelStackWithNoteRow*)toReturn)->noteRowId = noteRowIndex;
	((ModelStackWithNoteRow*)toReturn)->setNoteRow(newNoteRow);
	toReturn->modControllable = nullptr;
	toReturn->paramManager = nullptr;
	return toReturn;
}

// ── ModelStackWithNoteRowId ──────────────────────────────────────────────

ModelStackWithNoteRow* ModelStackWithNoteRowId::automaticallyAddNoteRowFromId() const {
	auto* toReturn = (ModelStackWithNoteRow*)this;
	NoteRow* nr = nullptr;
	// Phase 15: try to look up NoteRow from InstrumentClip mock
	if (auto* tc = getTimelineCounterAllowNull()) {
		auto* ic = static_cast<InstrumentClip*>(tc);
		nr = ic->getNoteRowFromId(noteRowId);
	}
	toReturn->setNoteRow(nr);
	return toReturn;
}

// ── ModelStackWithNoteRow ────────────────────────────────────────────────

int32_t ModelStackWithNoteRow::getLoopLength() const {
	NoteRow* nr = getNoteRow();
	if (nr && nr->loopLengthIfIndependent) {
		return nr->loopLengthIfIndependent;
	}
	return 0;
}

int32_t ModelStackWithNoteRow::getRepeatCount() const {
	return 0;
}

int32_t ModelStackWithNoteRow::getLastProcessedPos() const {
	return 0;
}

int32_t ModelStackWithNoteRow::getLivePos() const {
	return 0;
}

bool ModelStackWithNoteRow::isCurrentlyPlayingReversed() const {
	return false;
}

int32_t ModelStackWithNoteRow::getPosAtWhichPlaybackWillCut() const {
	return 2147483647; // INT32_MAX — never cuts
}

ModelStackWithThreeMainThings* ModelStackWithNoteRow::addOtherTwoThingsAutomaticallyGivenNoteRow() const {
	auto* toReturn = (ModelStackWithThreeMainThings*)this;
	toReturn->modControllable = nullptr;
	toReturn->paramManager = nullptr;
	return toReturn;
}

// ── ModelStackWithThreeMainThings ────────────────────────────────────────

ModelStackWithAutoParam* ModelStackWithThreeMainThings::getUnpatchedAutoParamFromId(int32_t newParamId) {
	auto* toReturn = (ModelStackWithAutoParam*)this;
	toReturn->paramCollection = nullptr;
	toReturn->summary = nullptr;
	toReturn->paramId = newParamId;
	toReturn->autoParam = nullptr;
	return toReturn;
}

ModelStackWithAutoParam* ModelStackWithThreeMainThings::getPatchedAutoParamFromId(int32_t newParamId) {
	auto* toReturn = (ModelStackWithAutoParam*)this;
	toReturn->paramCollection = nullptr;
	toReturn->summary = nullptr;
	toReturn->paramId = newParamId;
	toReturn->autoParam = nullptr;
	return toReturn;
}

ModelStackWithAutoParam* ModelStackWithThreeMainThings::getPatchCableAutoParamFromId(int32_t newParamId) {
	auto* toReturn = (ModelStackWithAutoParam*)this;
	toReturn->paramCollection = nullptr;
	toReturn->summary = nullptr;
	toReturn->paramId = newParamId;
	toReturn->autoParam = nullptr;
	return toReturn;
}

ModelStackWithAutoParam* ModelStackWithThreeMainThings::getExpressionAutoParamFromID(int32_t newParamId) {
	auto* toReturn = (ModelStackWithAutoParam*)this;
	toReturn->paramCollection = nullptr;
	toReturn->summary = nullptr;
	toReturn->paramId = newParamId;
	toReturn->autoParam = nullptr;
	return toReturn;
}

// ── ModelStackWithParamId ────────────────────────────────────────────────

bool ModelStackWithParamId::isParam(deluge::modulation::params::Kind kind, deluge::modulation::params::ParamType id) {
	return false;
}

// ── ModelStackWithSoundFlags ─────────────────────────────────────────────
// Real implementations — needed for Sound::noteOn() which checks sources before proceeding.

bool ModelStackWithSoundFlags::checkSourceEverActiveDisregardingMissingSample(int32_t s) {
	int32_t flagValue = soundFlags[SOUND_FLAG_SOURCE_0_ACTIVE_DISREGARDING_MISSING_SAMPLE + s];
	if (flagValue == FLAG_TBD) {
		flagValue = ((Sound*)modControllable)
		                ->isSourceActiveEverDisregardingMissingSample(s, (ParamManagerForTimeline*)paramManager);
		soundFlags[SOUND_FLAG_SOURCE_0_ACTIVE_DISREGARDING_MISSING_SAMPLE + s] = flagValue;
	}
	return flagValue;
}

bool ModelStackWithSoundFlags::checkSourceEverActive(int32_t s) {
	int32_t flagValue = soundFlags[SOUND_FLAG_SOURCE_0_ACTIVE + s];
	if (flagValue == FLAG_TBD) {
		flagValue = checkSourceEverActiveDisregardingMissingSample(s);
		if (flagValue) {
			Sound* sound = (Sound*)modControllable;
			flagValue =
			    sound->synthMode == SynthMode::FM
			    || (sound->sources[s].oscType != OscType::SAMPLE && sound->sources[s].oscType != OscType::WAVETABLE)
			    || sound->sources[s].hasAtLeastOneAudioFileLoaded();
		}
		soundFlags[SOUND_FLAG_SOURCE_0_ACTIVE + s] = flagValue;
	}
	return flagValue;
}

// ── Free functions ───────────────────────────────────────────────────────

void copyModelStack(void* newMemory, void const* oldMemory, int32_t size) {
	memcpy(newMemory, oldMemory, size);
}

ModelStackWithThreeMainThings* getModelStackFromSoundDrum(void* memory, SoundDrum* soundDrum) {
	auto* toReturn = (ModelStackWithThreeMainThings*)memory;
	toReturn->song = nullptr;
	toReturn->setTimelineCounter(nullptr);
	toReturn->setNoteRow(nullptr);
	toReturn->noteRowId = 0;
	toReturn->modControllable = nullptr;
	toReturn->paramManager = nullptr;
	return toReturn;
}
