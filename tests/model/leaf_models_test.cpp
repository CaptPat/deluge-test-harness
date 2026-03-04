#include "CppUTest/TestHarness.h"
#include "model/consequence/consequence.h"
#include "model/timeline_counter.h"
#include "model/drum/drum_name.h"
#include "model/note/copied_note_row.h"

// ── Consequence ──────────────────────────────────────────────────────────
// Consequence is abstract — create a concrete test subclass

class TestConsequence : public Consequence {
public:
	Error revert(TimeType, ModelStack*) override { return Error::NONE; }
};

TEST_GROUP(ConsequenceTest){};

TEST(ConsequenceTest, constructorInitializesType) {
	TestConsequence c;
	CHECK_EQUAL(0, c.type);
}

TEST(ConsequenceTest, destructorDoesNotCrash) {
	TestConsequence* c = new TestConsequence();
	delete c;
}

// ── TimelineCounter ──────────────────────────────────────────────────────
// TimelineCounter is abstract — create a concrete test subclass

class TestTimelineCounter : public TimelineCounter {
public:
	int32_t getLastProcessedPos() const override { return 0; }
	uint32_t getLivePos() const override { return 0; }
	int32_t getLoopLength() const override { return 0; }
	bool isPlayingAutomationNow() const override { return false; }
	bool backtrackingCouldLoopBackToEnd() const override { return false; }
	int32_t getPosAtWhichPlaybackWillCut(ModelStackWithTimelineCounter const*) const override { return 0; }
	void getActiveModControllable(ModelStackWithTimelineCounter*) override {}
	void expectEvent() override {}
	TimelineCounter* getTimelineCounterToRecordTo() override { return this; }
};

TEST_GROUP(TimelineCounterTest){};

TEST(TimelineCounterTest, defaultConstruct) {
	TestTimelineCounter tc;
	// Should construct without crash; has paramManager member
	CHECK(tc.armedForRecording);
}

TEST(TimelineCounterTest, destructDoesNotCrash) {
	TestTimelineCounter* tc = new TestTimelineCounter();
	delete tc;
}

// ── DrumName ─────────────────────────────────────────────────────────────

TEST_GROUP(DrumNameTest){};

TEST(DrumNameTest, constructorSetsName) {
	String str;
	str.set("kick");
	DrumName dn(&str);
	STRCMP_EQUAL("kick", dn.name.get());
}

TEST(DrumNameTest, constructorEmptyString) {
	String str;
	DrumName dn(&str);
	STRCMP_EQUAL("", dn.name.get());
}

TEST(DrumNameTest, destructorDoesNotCrash) {
	String str;
	str.set("snare");
	DrumName* dn = new DrumName(&str);
	delete dn;
}

// ── CopiedNoteRow ────────────────────────────────────────────────────────

TEST_GROUP(CopiedNoteRowTest){};

TEST(CopiedNoteRowTest, constructorInitializesNullptrs) {
	CopiedNoteRow cnr;
	POINTERS_EQUAL(nullptr, cnr.next);
	POINTERS_EQUAL(nullptr, cnr.notes);
}

TEST(CopiedNoteRowTest, destructorDoesNotCrash) {
	CopiedNoteRow* cnr = new CopiedNoteRow();
	delete cnr; // delugeDealloc(nullptr) is safe
}
