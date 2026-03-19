// Source contract test: verify that AutomationView handles UI_MODE_QUANTIZE
// in handleAuditionPadAction() to prevent partial freeze when quantizing
// notes in Velocity View.
//
// Background (upstream #3718): Quantizing notes in Velocity View caused a
// partial freeze because AutomationView::handleAuditionPadAction() had no
// handler for UI_MODE_QUANTIZE. The pad release went through the generic
// audition path which never called commandStopQuantize(), leaving the UI
// mode permanently active and blocking most inputs.
//
// The fix (fix/velocity-view-quantize-freeze):
//   1. Add UI_MODE_QUANTIZE check to AutomationView::handleAuditionPadAction()
//   2. Move commandStopQuantize to public visibility

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

TEST_GROUP(QuantizeFreezeGuard){};

// AutomationView::handleAuditionPadAction must check UI_MODE_QUANTIZE
// and call commandStopQuantize on pad release, before the generic
// auditionPadActionUIModes branch handles it silently.
TEST(QuantizeFreezeGuard, automationViewHandlesQuantizeMode) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "views" / "automation_view.cpp";
	CHECK_TEXT(fs::exists(cppPath), "automation_view.cpp not found — is firmware submodule checked out?");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read automation_view.cpp");

	// Find handleAuditionPadAction
	auto funcPos = src.find("AutomationView::handleAuditionPadAction");
	CHECK_TEXT(funcPos != std::string::npos,
	           "AutomationView::handleAuditionPadAction not found in automation_view.cpp");

	// Find the generic auditionPadActionUIModes branch (this is the fallthrough we must beat)
	auto genericPos = src.find("auditionPadActionUIModes", funcPos);
	CHECK_TEXT(genericPos != std::string::npos,
	           "auditionPadActionUIModes branch not found in handleAuditionPadAction");

	// UI_MODE_QUANTIZE must appear BEFORE the generic branch
	auto quantizePos = src.find("UI_MODE_QUANTIZE", funcPos);
	CHECK_TEXT(quantizePos != std::string::npos && quantizePos < genericPos,
	           "UI_MODE_QUANTIZE handler must appear in handleAuditionPadAction before "
	           "the generic auditionPadActionUIModes branch (fix/velocity-view-quantize-freeze, "
	           "upstream #3718)");

	// commandStopQuantize must be called in the handler
	auto stopPos = src.find("commandStopQuantize", funcPos);
	CHECK_TEXT(stopPos != std::string::npos && stopPos < genericPos,
	           "commandStopQuantize must be called in the UI_MODE_QUANTIZE handler "
	           "to properly exit quantize mode on pad release");
}

// commandStopQuantize must be public so AutomationView can call it.
TEST(QuantizeFreezeGuard, commandStopQuantizeIsPublic) {
	fs::path hPath = firmwareRoot() / "src" / "deluge" / "gui" / "views" / "instrument_clip_view.h";
	CHECK_TEXT(fs::exists(hPath), "instrument_clip_view.h not found");

	std::string src = readFile(hPath);
	CHECK_TEXT(!src.empty(), "Failed to read instrument_clip_view.h");

	auto publicPos = src.find("public:");
	auto privatePos = src.find("private:", publicPos);
	CHECK_TEXT(publicPos != std::string::npos && privatePos != std::string::npos,
	           "Could not find public/private sections in instrument_clip_view.h");

	auto methodPos = src.find("commandStopQuantize", publicPos);
	CHECK_TEXT(methodPos != std::string::npos && methodPos < privatePos,
	           "commandStopQuantize must be in the public section of InstrumentClipView "
	           "so AutomationView can call it (fix/velocity-view-quantize-freeze)");
}

} // namespace
