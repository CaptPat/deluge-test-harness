// Source contract test: verify that guards against re-entrant FatFS access
// during settings menu exit are present in the firmware source.
//
// Background (upstream #3898): pressing BACK rapidly while exiting Settings
// can trigger re-entrant FatFS calls via the BACK_MENU_EXIT timer, corrupting
// shared buffers and crashing in disk_read/move_window/dir_find.
//
// Two guards are required:
//   1. buttons.cpp: BACK_MENU_EXIT timer must not be armed during card routines
//   2. sound_editor: exitUI() must check sdRoutineLock before calling exitCompletely()

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

// Guard 1: BACK_MENU_EXIT timer must be conditional on !inCardRoutine
// If the timer is set unconditionally during a card routine, the release event
// may never arrive (blocked by waitingForSDRoutineToEnd), so the timer fires
// during yield() and triggers re-entrant FatFS access.
TEST(SDRaceGuard, backTimerGuardedByCardRoutine) {
	fs::path buttonsPath = firmwareRoot() / "src" / "deluge" / "hid" / "buttons.cpp";
	CHECK_TEXT(fs::exists(buttonsPath), "buttons.cpp not found — is firmware submodule checked out?");

	std::string src = readFile(buttonsPath);
	CHECK_TEXT(!src.empty(), "Failed to read buttons.cpp");

	// The BACK_MENU_EXIT timer arm must be guarded.
	// Look for the pattern: timer set is conditional on !inCardRoutine
	// Bad:  if (on) { setTimer(BACK_MENU_EXIT, ...) }
	// Good: if (on && !inCardRoutine) { setTimer(BACK_MENU_EXIT, ...) }
	auto timerPos = src.find("BACK_MENU_EXIT");
	CHECK_TEXT(timerPos != std::string::npos, "BACK_MENU_EXIT timer reference not found in buttons.cpp");

	// Find the context around the first BACK_MENU_EXIT (the setTimer call)
	// Look backwards for the if-condition
	auto contextStart = (timerPos > 200) ? timerPos - 200 : 0;
	std::string context = src.substr(contextStart, 400);

	CHECK_TEXT(context.find("!inCardRoutine") != std::string::npos,
	           "BACK_MENU_EXIT timer must be guarded by !inCardRoutine to prevent "
	           "re-entrant FatFS access during settings save (upstream #3898)");
}

// Guard 2: SoundEditor::exitUI() must check sdRoutineLock
// This prevents the BACK_MENU_EXIT timer callback from calling exitCompletely()
// while settings writes (which hold sdRoutineLock) are in progress.
TEST(SDRaceGuard, exitUIGuardedBySDRoutineLock) {
	// Check the header first (inline exitUI)
	fs::path headerPath = firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "sound_editor.h";
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "sound_editor.cpp";

	bool foundGuard = false;

	// Check header — if exitUI is inline there, the guard should be in the header
	if (fs::exists(headerPath)) {
		std::string header = readFile(headerPath);
		if (header.find("exitUI") != std::string::npos) {
			if (header.find("sdRoutineLock") != std::string::npos) {
				foundGuard = true;
			}
		}
	}

	// Check cpp — if exitUI is defined there, the guard should be in the cpp
	if (!foundGuard && fs::exists(cppPath)) {
		std::string cpp = readFile(cppPath);
		// Find exitUI implementation and check for sdRoutineLock guard
		auto exitUIPos = cpp.find("SoundEditor::exitUI");
		if (exitUIPos != std::string::npos) {
			// Check the next ~300 chars for the guard
			std::string body = cpp.substr(exitUIPos, 300);
			if (body.find("sdRoutineLock") != std::string::npos) {
				foundGuard = true;
			}
		}
	}

	CHECK_TEXT(foundGuard,
	           "SoundEditor::exitUI() must check sdRoutineLock to prevent re-entrant "
	           "FatFS access when timer fires during settings save (upstream #3898)");
}

// Guard 3: Settings writes in exitCompletely() must hold sdRoutineLock
TEST(SDRaceGuard, settingsWritesProtectedBySDRoutineLock) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "sound_editor.cpp";
	CHECK_TEXT(fs::exists(cppPath), "sound_editor.cpp not found");

	std::string src = readFile(cppPath);
	auto exitPos = src.find("SoundEditor::exitCompletely");
	CHECK_TEXT(exitPos != std::string::npos, "exitCompletely() not found in sound_editor.cpp");

	// Find the body of exitCompletely — scan until the next function definition
	auto nextFunc = src.find("\nvoid ", exitPos + 1);
	if (nextFunc == std::string::npos)
		nextFunc = src.size();
	std::string body = src.substr(exitPos, nextFunc - exitPos);

	// Verify sdRoutineLock is set before the writes
	auto lockPos = body.find("sdRoutineLock = true");
	auto writePos = body.find("writeSettings");
	CHECK_TEXT(lockPos != std::string::npos && writePos != std::string::npos && lockPos < writePos,
	           "exitCompletely() must set sdRoutineLock = true before calling writeSettings() "
	           "to prevent concurrent FatFS access (upstream #3898)");

	// Verify sdRoutineLock is released after writes
	auto unlockPos = body.find("sdRoutineLock = false");
	CHECK_TEXT(unlockPos != std::string::npos && unlockPos > writePos,
	           "exitCompletely() must release sdRoutineLock after settings writes complete");
}

} // namespace
