// Source contract test: verify that the guard against re-entrant FatFS access
// during settings menu exit is present in the firmware source.
//
// Background (upstream #3898): pressing BACK rapidly while exiting Settings
// can trigger re-entrant FatFS calls via the BACK_MENU_EXIT timer, corrupting
// shared buffers and crashing in disk_read/move_window/dir_find.
//
// The fix: buttons.cpp must not arm the BACK_MENU_EXIT timer when inCardRoutine
// is true, preventing the timer from firing during yield() inside settings saves.

#include "CppUTest/TestHarness.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path firmwareRoot() {
	fs::path p = fs::current_path();
	while (!p.empty() && !fs::exists(p / "firmware" / "src" / "deluge")) {
		auto parent = p.parent_path();
		if (parent == p)
			break;
		p = parent;
	}
	return p / "firmware";
}

std::string readFile(const fs::path& path) {
	std::ifstream f(path);
	if (!f.is_open())
		return "";
	return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

TEST_GROUP(SDRaceGuard){};

// BACK_MENU_EXIT timer must be conditional on !sdRoutineLock.
// If the timer is set unconditionally during an SD routine, the release event
// may never arrive (blocked by waitingForSDRoutineToEnd), so the timer fires
// during yield() and triggers re-entrant FatFS access.
TEST(SDRaceGuard, backTimerGuardedBySDRoutineLock) {
	fs::path buttonsPath = firmwareRoot() / "src" / "deluge" / "hid" / "buttons.cpp";
	CHECK_TEXT(fs::exists(buttonsPath), "buttons.cpp not found — is firmware submodule checked out?");

	std::string src = readFile(buttonsPath);
	CHECK_TEXT(!src.empty(), "Failed to read buttons.cpp");

	// The BACK_MENU_EXIT timer arm must be guarded by inCardRoutine.
	auto timerPos = src.find("BACK_MENU_EXIT");
	CHECK_TEXT(timerPos != std::string::npos, "BACK_MENU_EXIT timer reference not found in buttons.cpp");

	// Find the context around the first BACK_MENU_EXIT (the setTimer call)
	auto contextStart = (timerPos > 200) ? timerPos - 200 : 0;
	std::string context = src.substr(contextStart, 400);

	CHECK_TEXT(context.find("inCardRoutine") != std::string::npos,
	           "BACK_MENU_EXIT timer must be guarded by inCardRoutine to prevent "
	           "re-entrant FatFS access during settings save (upstream #3898)");
}

// After goUpOneLevel(), SoundEditor must check if it's still the current UI.
// goUpOneLevel() can call exitCompletely() when navigationDepth == 0, which
// calls close() and removes SoundEditor from the UI stack. Without this guard,
// handlePotentialParamMenuChange() and other member access runs on stale state.
TEST(SDRaceGuard, goUpOneLevelExitGuard) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "sound_editor.cpp";
	CHECK_TEXT(fs::exists(cppPath), "sound_editor.cpp not found");

	std::string src = readFile(cppPath);

	// Find the BACK button handler in buttonAction
	auto backPos = src.find("else if (b == BACK)");
	CHECK_TEXT(backPos != std::string::npos, "BACK button handler not found in sound_editor.cpp");

	// After goUpOneLevel(), there should be a getCurrentUI() != this guard
	auto goUpPos = src.find("goUpOneLevel()", backPos);
	CHECK_TEXT(goUpPos != std::string::npos, "goUpOneLevel() call not found in BACK handler");

	auto guardPos = src.find("getCurrentUI() != this", goUpPos);
	auto nextFunc = src.find("\nvoid ", goUpPos);
	CHECK_TEXT(guardPos != std::string::npos && (nextFunc == std::string::npos || guardPos < nextFunc),
	           "goUpOneLevel() must be followed by getCurrentUI() != this guard to prevent "
	           "use-after-exit when navigating out of the sound editor");
}

} // namespace
