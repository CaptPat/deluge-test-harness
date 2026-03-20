// Source contract test: verify that ModelStackWithNoteRow methods guard against
// unsafe Clip* casts when the timeline counter is a Song.
//
// Background (upstream #4020): when recording in arrangement mode and adjusting
// song-level parameters (performance mode effects, delay, etc.), Song is the
// timeline counter. Several ModelStackWithNoteRow methods cast getTimelineCounter()
// to Clip* to access Clip-specific members (currentlyPlayingReversed, repeatCount,
// sequenceDirectionMode). Song is NOT a Clip — this is undefined behavior that
// reads garbage memory, causing E445 crashes and other unpredictable failures.
//
// The fix (bugfix/arrangement-record-song-cast-crash): check getTimelineCounter()
// == currentSong before casting to Clip*, returning safe defaults for Song.

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

} // namespace

TEST_GROUP(SongCastGuard){};

// isCurrentlyPlayingReversed() must check for Song before casting to Clip*
TEST(SongCastGuard, isCurrentlyPlayingReversedGuardsSongCast) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "model" / "model_stack.cpp";
	CHECK_TEXT(fs::exists(cppPath), "model_stack.cpp not found — is firmware submodule checked out?");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read model_stack.cpp");

	auto funcPos = src.find("ModelStackWithNoteRow::isCurrentlyPlayingReversed()");
	CHECK_TEXT(funcPos != std::string::npos,
	           "isCurrentlyPlayingReversed() not found in model_stack.cpp");

	// The guard must appear before the first (Clip*) cast in this function
	auto body = src.substr(funcPos, 600);
	auto guardPos = body.find("currentSong");
	auto castPos = body.find("(Clip*)");
	CHECK_TEXT(guardPos != std::string::npos,
	           "isCurrentlyPlayingReversed() must check getTimelineCounter() == currentSong "
	           "before casting to Clip* (bugfix/arrangement-record-song-cast-crash, upstream #4020)");
	CHECK_TEXT(guardPos < castPos,
	           "currentSong guard must appear BEFORE the (Clip*) cast in isCurrentlyPlayingReversed()");
}

// getRepeatCount() must check for Song before casting to Clip*
TEST(SongCastGuard, getRepeatCountGuardsSongCast) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "model" / "model_stack.cpp";
	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read model_stack.cpp");

	auto funcPos = src.find("ModelStackWithNoteRow::getRepeatCount()");
	CHECK_TEXT(funcPos != std::string::npos, "getRepeatCount() not found in model_stack.cpp");

	auto body = src.substr(funcPos, 400);
	auto guardPos = body.find("currentSong");
	CHECK_TEXT(guardPos != std::string::npos,
	           "getRepeatCount() must check getTimelineCounter() == currentSong "
	           "before casting to Clip* (bugfix/arrangement-record-song-cast-crash, upstream #4020)");
}
