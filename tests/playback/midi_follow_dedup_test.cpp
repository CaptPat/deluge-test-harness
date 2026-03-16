// Tests for MIDI Follow note deduplication (fixes upstream #4225).
//
// When MIDI Follow routes a note to a clip's instrument, the same instrument
// must be skipped in the subsequent output loop to prevent duplicate
// recordNoteOn() calls. This test validates the skip logic in isolation.

#include "CppUTest/TestHarness.h"

#include <array>
#include <cstdint>
#include <vector>

namespace {

// Minimal Output stand-in for routing tests
struct MockRoutingOutput {
	const char* name;
	int noteCount{0};
	bool active{true};

	void recordNote(int32_t note, int32_t velocity) { noteCount++; }
};

/// Simulates the routing logic in PlaybackHandler::noteMessageReceived().
/// Returns a record of which outputs received the note.
void simulateNoteRouting(MockRoutingOutput* midiFollowOutput, std::vector<MockRoutingOutput*>& allOutputs, int32_t note,
                         int32_t velocity) {
	// Phase 1: MIDI Follow handles the note (if matched)
	if (midiFollowOutput) {
		midiFollowOutput->recordNote(note, velocity);
	}

	// Phase 2: Loop through all outputs — skip the one already handled
	for (auto* output : allOutputs) {
		if (output == midiFollowOutput) {
			continue; // The fix: skip output already handled by MIDI Follow
		}
		if (output->active) {
			output->recordNote(note, velocity);
		}
	}
}

/// Simulates the OLD (buggy) routing — no skip.
void simulateNoteRoutingBuggy(MockRoutingOutput* midiFollowOutput, std::vector<MockRoutingOutput*>& allOutputs, int32_t note,
                              int32_t velocity) {
	// Phase 1: MIDI Follow handles the note
	if (midiFollowOutput) {
		midiFollowOutput->recordNote(note, velocity);
	}

	// Phase 2: Loop through all outputs — NO skip (the bug)
	for (auto* output : allOutputs) {
		if (output->active) {
			output->recordNote(note, velocity);
		}
	}
}

} // namespace

TEST_GROUP(MidiFollowDedup){};

// ── Core deduplication ──────────────────────────────────────────────────────

TEST(MidiFollowDedup, midiFollowOutputReceivesNoteOnce) {
	MockRoutingOutput synth{"Synth"};
	std::vector<MockRoutingOutput*> outputs = {&synth};

	simulateNoteRouting(&synth, outputs, 60, 100);

	// Synth should get exactly 1 note (from MIDI Follow), not 2
	CHECK_EQUAL(1, synth.noteCount);
}

TEST(MidiFollowDedup, buggyRoutingCausesDuplicate) {
	MockRoutingOutput synth{"Synth"};
	std::vector<MockRoutingOutput*> outputs = {&synth};

	simulateNoteRoutingBuggy(&synth, outputs, 60, 100);

	// Old code: synth gets note twice (MIDI Follow + output loop)
	CHECK_EQUAL(2, synth.noteCount);
}

// ── Other outputs still receive notes ───────────────────────────────────────

TEST(MidiFollowDedup, otherOutputsStillReceiveNote) {
	MockRoutingOutput synth{"Synth"};
	MockRoutingOutput bass{"Bass"};
	std::vector<MockRoutingOutput*> outputs = {&synth, &bass};

	// MIDI Follow routes to synth, bass has its own MIDI input match
	simulateNoteRouting(&synth, outputs, 60, 100);

	CHECK_EQUAL(1, synth.noteCount); // from MIDI Follow only
	CHECK_EQUAL(1, bass.noteCount);  // from output loop
}

TEST(MidiFollowDedup, multipleOtherOutputsAllReceive) {
	MockRoutingOutput synth{"Synth"};
	MockRoutingOutput bass{"Bass"};
	MockRoutingOutput pad{"Pad"};
	std::vector<MockRoutingOutput*> outputs = {&synth, &bass, &pad};

	simulateNoteRouting(&synth, outputs, 60, 100);

	CHECK_EQUAL(1, synth.noteCount);
	CHECK_EQUAL(1, bass.noteCount);
	CHECK_EQUAL(1, pad.noteCount);
}

// ── No MIDI Follow match ────────────────────────────────────────────────────

TEST(MidiFollowDedup, noMidiFollowMatchRoutesNormally) {
	MockRoutingOutput synth{"Synth"};
	MockRoutingOutput bass{"Bass"};
	std::vector<MockRoutingOutput*> outputs = {&synth, &bass};

	// No MIDI Follow match — nullptr
	simulateNoteRouting(nullptr, outputs, 60, 100);

	CHECK_EQUAL(1, synth.noteCount);
	CHECK_EQUAL(1, bass.noteCount);
}

// ── Muted outputs ───────────────────────────────────────────────────────────

TEST(MidiFollowDedup, mutedOutputSkippedInLoop) {
	MockRoutingOutput synth{"Synth"};
	MockRoutingOutput muted{"Muted"};
	muted.active = false;
	std::vector<MockRoutingOutput*> outputs = {&synth, &muted};

	simulateNoteRouting(nullptr, outputs, 60, 100);

	CHECK_EQUAL(1, synth.noteCount);
	CHECK_EQUAL(0, muted.noteCount);
}

TEST(MidiFollowDedup, midiFollowToMutedOutputStillPlays) {
	// MIDI Follow can route to a muted output's clip (it bypasses mute check)
	MockRoutingOutput synth{"Synth"};
	synth.active = false; // muted in arrangement
	MockRoutingOutput other{"Other"};
	std::vector<MockRoutingOutput*> outputs = {&synth, &other};

	simulateNoteRouting(&synth, outputs, 60, 100);

	CHECK_EQUAL(1, synth.noteCount); // MIDI Follow delivered it
	CHECK_EQUAL(1, other.noteCount); // other got it from loop
}

// ── Multiple notes ──────────────────────────────────────────────────────────

TEST(MidiFollowDedup, multipleNotesEachRecordedOnce) {
	MockRoutingOutput synth{"Synth"};
	std::vector<MockRoutingOutput*> outputs = {&synth};

	simulateNoteRouting(&synth, outputs, 60, 100);
	simulateNoteRouting(&synth, outputs, 64, 100);
	simulateNoteRouting(&synth, outputs, 67, 100);

	CHECK_EQUAL(3, synth.noteCount); // 3 notes, each once
}

TEST(MidiFollowDedup, buggyMultipleNotesDoubled) {
	MockRoutingOutput synth{"Synth"};
	std::vector<MockRoutingOutput*> outputs = {&synth};

	simulateNoteRoutingBuggy(&synth, outputs, 60, 100);
	simulateNoteRoutingBuggy(&synth, outputs, 64, 100);
	simulateNoteRoutingBuggy(&synth, outputs, 67, 100);

	CHECK_EQUAL(6, synth.noteCount); // 3 notes × 2 = 6 (the bug)
}

// ── Edge: MIDI Follow output not in output list ─────────────────────────────

TEST(MidiFollowDedup, midiFollowOutputNotInListStillWorks) {
	// Shouldn't happen in practice, but verify no crash
	MockRoutingOutput followTarget{"FollowTarget"};
	MockRoutingOutput other{"Other"};
	std::vector<MockRoutingOutput*> outputs = {&other};

	simulateNoteRouting(&followTarget, outputs, 60, 100);

	CHECK_EQUAL(1, followTarget.noteCount); // from MIDI Follow
	CHECK_EQUAL(1, other.noteCount);        // from loop (no match to skip)
}
