// Source contract test: verify that guards against re-entrant FatFS access
// during settings menu exit are present in the firmware source.
//
// Background (upstream #3898): pressing BACK rapidly while exiting Settings
// can trigger re-entrant FatFS calls via the BACK_MENU_EXIT timer, corrupting
// shared buffers and crashing in disk_read/move_window/dir_find.
//
// The fix (bugfix/settings-exit-sd-race): exitCompletely() defers settings
// writes via SDCardOps::requestSettingsWrite(), which auto-defers if the
// SD card lock is held.

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

// exitCompletely() must defer settings writes via SDCardOps::requestSettingsWrite()
// instead of writing directly to the SD card (which can re-enter FatFS).
TEST(SDRaceGuard, settingsWriteDeferredViaSdCardOps) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "sound_editor.cpp";
	CHECK_TEXT(fs::exists(cppPath), "sound_editor.cpp not found — is firmware submodule checked out?");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read sound_editor.cpp");

	// exitCompletely() must use requestSettingsWrite to defer the settings write
	auto exitPos = src.find("SoundEditor::exitCompletely()");
	CHECK_TEXT(exitPos != std::string::npos, "SoundEditor::exitCompletely() not found in sound_editor.cpp");

	auto body = src.substr(exitPos, 500);
	CHECK_TEXT(body.find("requestSettingsWrite") != std::string::npos,
	           "exitCompletely() must use SDCardOps::requestSettingsWrite() to defer settings writes "
	           "to prevent re-entrant FatFS access (bugfix/settings-exit-sd-race, upstream #3898)");
}

// After goUpOneLevel(), SoundEditor must check if it's still the current UI.
// goUpOneLevel() can call exitCompletely() when navigationDepth == 0, which
// calls close() and removes SoundEditor from the UI stack. Without this guard,
// subsequent member access runs on stale state.
TEST(SDRaceGuard, goUpOneLevelExitGuard) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "sound_editor.cpp";
	CHECK_TEXT(fs::exists(cppPath), "sound_editor.cpp not found");

	std::string src = readFile(cppPath);

	// Find goUpOneLevel() call
	auto goUpPos = src.find("goUpOneLevel()");
	CHECK_TEXT(goUpPos != std::string::npos, "goUpOneLevel() call not found in sound_editor.cpp");

	// After goUpOneLevel(), there should be a getCurrentUI() != this guard
	auto guardPos = src.find("getCurrentUI() != this", goUpPos);
	auto nextFunc = src.find("\nvoid ", goUpPos);
	CHECK_TEXT(guardPos != std::string::npos && (nextFunc == std::string::npos || guardPos < nextFunc),
	           "goUpOneLevel() must be followed by getCurrentUI() != this guard to prevent "
	           "use-after-exit when navigating out of the sound editor (bugfix/settings-menu-exit-crash)");
}

} // namespace
