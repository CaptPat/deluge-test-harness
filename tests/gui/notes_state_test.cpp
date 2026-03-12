// NotesState const-iterator regression test.
// Bug: const end() returns notes.end() + count (UB, overshoots array)
// Fix: const end() returns notes.begin() + count (matches mutable overload)

#include "CppUTest/TestHarness.h"
#include "gui/ui/keyboard/notes_state.h"

using namespace deluge::gui::ui::keyboard;

namespace {

TEST_GROUP(NotesStateIterator){};

TEST(NotesStateIterator, constEndMatchesCount) {
	NotesState ns;
	ns.enableNote(60, 100);
	ns.enableNote(64, 80);
	ns.enableNote(67, 90);

	const NotesState& cns = ns;

	// With the bug: end() = notes.end() + 3 (element 13, past 10-element array).
	// The loop would iterate far beyond valid data (UB).
	int count = 0;
	for (auto it = cns.begin(); it != cns.end(); ++it) {
		count++;
		// Safety bail — if the bug is present, we'd iterate way too many times
		if (count > 20) {
			FAIL("const iterator overshot — end() is wrong (notes.end() + count instead of begin() + count)");
		}
	}
	CHECK_EQUAL(3, count);
}

TEST(NotesStateIterator, emptyConstRange) {
	const NotesState ns;
	CHECK(ns.begin() == ns.end());
}

TEST(NotesStateIterator, constRangeFor) {
	NotesState ns;
	ns.enableNote(48, 127);
	const NotesState& cns = ns;

	int count = 0;
	for ([[maybe_unused]] const auto& note : cns) {
		count++;
		if (count > 20) {
			FAIL("const range-for overshot");
		}
	}
	CHECK_EQUAL(1, count);
}

TEST(NotesStateIterator, mutableAndConstEndAgree) {
	NotesState ns;
	ns.enableNote(60, 100);
	ns.enableNote(72, 50);

	// Mutable end() is known-correct: notes.begin() + count
	auto mutableDist = std::distance(ns.begin(), ns.end());

	const NotesState& cns = ns;
	auto constDist = std::distance(cns.begin(), cns.end());

	CHECK_EQUAL(mutableDist, constDist);
}

} // namespace
