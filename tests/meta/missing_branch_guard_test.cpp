// Source contract tests for bugfix branches that were missing test coverage.
//
// Each test verifies that a specific guard/fix is present in the firmware
// source by reading the .cpp/.h file and checking for expected code patterns.

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

// ============================================================================
// bugfix/multisample-octave-zero (upstream #3784)
//
// getComparativeNoteNumberFromChars() rejected '0' as a valid octave digit,
// causing filenames like "C0.wav" to not be recognized as note names.
// Fix: change range check from '1'-'9' to '0'-'9'.
// ============================================================================

TEST_GROUP(OctaveZeroGuard){};

TEST(OctaveZeroGuard, acceptsOctaveZeroInNoteParsing) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "util" / "functions.cpp";
	CHECK_TEXT(fs::exists(cppPath), "functions.cpp not found — is firmware submodule checked out?");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read functions.cpp");

	// Find the function
	auto funcPos = src.find("getComparativeNoteNumberFromChars");
	CHECK_TEXT(funcPos != std::string::npos,
	           "getComparativeNoteNumberFromChars not found in functions.cpp");

	// Within the function, the octave range check must start at '0', not '1'
	// The fix changed: if (*string < '1' || ...) to: if (*string < '0' || ...)
	auto body = src.substr(funcPos, 1000);

	// The old bug was '*string < '1'' — verify it's now '*string < '0''
	CHECK_TEXT(body.find("'0'") != std::string::npos,
	           "getComparativeNoteNumberFromChars must accept octave 0 — "
	           "range check should use '0' not '1' (bugfix/multisample-octave-zero, upstream #3784)");

	// Make sure the old buggy pattern is gone
	auto bugPattern = body.find("< '1'");
	CHECK_TEXT(bugPattern == std::string::npos,
	           "getComparativeNoteNumberFromChars still has '< \\'1\\'' — "
	           "octave 0 would be rejected (bugfix/multisample-octave-zero)");
}

// ============================================================================
// bugfix/kit-midi-learn-master-level (upstream #3640)
//
// When entering SoundEditor via shortcut pad on a Kit clip with affectEntire,
// setupKitGlobalFXMenu was not set. This left newModControllable as nullptr,
// crashing on MIDI learn.
// Fix: also check editingKit() && affectEntire in potentialShortcutPadAction
// and in setup().
// ============================================================================

TEST_GROUP(KitMidiLearnGuard){};

TEST(KitMidiLearnGuard, shortcutPadChecksAffectEntire) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "sound_editor.cpp";
	CHECK_TEXT(fs::exists(cppPath), "sound_editor.cpp not found");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read sound_editor.cpp");

	// Find potentialShortcutPadAction
	auto funcPos = src.find("SoundEditor::potentialShortcutPadAction");
	CHECK_TEXT(funcPos != std::string::npos,
	           "SoundEditor::potentialShortcutPadAction not found in sound_editor.cpp");

	// Within the Kit global FX shortcut branch, affectEntire must be checked
	// alongside setupKitGlobalFXMenu
	auto body = src.substr(funcPos, 2000);
	auto kitFxPos = body.find("paramShortcutsForKitGlobalFX");
	CHECK_TEXT(kitFxPos != std::string::npos,
	           "paramShortcutsForKitGlobalFX reference not found in potentialShortcutPadAction");

	// The condition guarding the Kit global FX branch must include affectEntire
	auto guardRegion = body.substr(0, kitFxPos);
	CHECK_TEXT(guardRegion.find("affectEntire") != std::string::npos,
	           "potentialShortcutPadAction must check affectEntire alongside setupKitGlobalFXMenu "
	           "to prevent null ModControllable crash on MIDI learn (bugfix/kit-midi-learn-master-level, "
	           "upstream #3640)");
}

TEST(KitMidiLearnGuard, setupChecksAffectEntire) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "sound_editor.cpp";
	std::string src = readFile(cppPath);

	// Find SoundEditor::setup(Clip* — the actual setup function, not setupShortcuts*
	auto funcPos = src.find("SoundEditor::setup(Clip*");
	CHECK_TEXT(funcPos != std::string::npos, "SoundEditor::setup(Clip*) not found");

	// Within setup(), the Kit section must check affectEntire alongside setupKitGlobalFXMenu
	// when deciding to use the Kit's own ModControllable vs a drum's
	auto body = src.substr(funcPos, 4000);

	// Find the Kit-specific setupKitGlobalFXMenu check
	auto kitFxPos = body.find("setupKitGlobalFXMenu");
	CHECK_TEXT(kitFxPos != std::string::npos,
	           "setupKitGlobalFXMenu check not found in SoundEditor::setup");

	// The same condition line must also include affectEntire
	// (they should be on the same line or adjacent in the condition)
	auto lineEnd = body.find('\n', kitFxPos);
	auto condLine = body.substr(kitFxPos, lineEnd - kitFxPos);
	CHECK_TEXT(condLine.find("affectEntire") != std::string::npos,
	           "SoundEditor::setup must check affectEntire alongside setupKitGlobalFXMenu "
	           "when initializing newModControllable for Kit clips (bugfix/kit-midi-learn-master-level)");
}

// ============================================================================
// bugfix/midi-to-synth-crash-on-save (upstream #4014)
//
// When changing a clip from MIDI_OUT to another type, stale midiBank/midiSub/
// midiPGM values were left behind, causing crashes during save serialization.
// Fix: clear MIDI metadata in changeOutputType() when old type was MIDI_OUT.
// ============================================================================

TEST_GROUP(MidiToSynthCrashGuard){};

TEST(MidiToSynthCrashGuard, changeOutputTypeClearsMidiMetadata) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "model" / "clip" / "instrument_clip.cpp";
	CHECK_TEXT(fs::exists(cppPath), "instrument_clip.cpp not found");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read instrument_clip.cpp");

	// Find changeOutputType
	auto funcPos = src.find("InstrumentClip::changeOutputType");
	CHECK_TEXT(funcPos != std::string::npos,
	           "InstrumentClip::changeOutputType not found in instrument_clip.cpp");

	auto body = src.substr(funcPos, 3000);

	// Must check for MIDI_OUT and clear the stale metadata
	CHECK_TEXT(body.find("MIDI_OUT") != std::string::npos,
	           "changeOutputType must check for old MIDI_OUT type to clear stale metadata "
	           "(bugfix/midi-to-synth-crash-on-save, upstream #4014)");

	CHECK_TEXT(body.find("midiBank") != std::string::npos && body.find("midiSub") != std::string::npos
	               && body.find("midiPGM") != std::string::npos,
	           "changeOutputType must reset midiBank, midiSub, and midiPGM when switching away "
	           "from MIDI_OUT to prevent stale metadata crash on save");
}

// ============================================================================
// bugfix/unsaved-synth-to-kit-row
//
// Loading an unsaved (in-memory only) synth preset into a Kit row would fail
// because loadSynthToDrum() reads XML from disk. Without existsOnCard check,
// the existing drum was already removed before discovering the file is missing.
// Fix: check existsOnCard before proceeding with destructive drum removal.
// ============================================================================

TEST_GROUP(UnsavedSynthToKitRowGuard){};

TEST(UnsavedSynthToKitRowGuard, checksExistsOnCardBeforeLoad) {
	fs::path cppPath =
	    firmwareRoot() / "src" / "deluge" / "gui" / "ui" / "load" / "load_instrument_preset_ui.cpp";
	CHECK_TEXT(fs::exists(cppPath), "load_instrument_preset_ui.cpp not found");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read load_instrument_preset_ui.cpp");

	// Find performLoadSynthToKit
	auto funcPos = src.find("LoadInstrumentPresetUI::performLoadSynthToKit");
	CHECK_TEXT(funcPos != std::string::npos,
	           "LoadInstrumentPresetUI::performLoadSynthToKit not found");

	auto body = src.substr(funcPos, 2000);

	// existsOnCard check must appear before the destructive drum removal
	auto existsPos = body.find("existsOnCard");
	CHECK_TEXT(existsPos != std::string::npos,
	           "performLoadSynthToKit must check existsOnCard before loading — "
	           "unsaved presets have no file on disk, and the existing drum would "
	           "already be removed before discovering FILE_NOT_FOUND "
	           "(bugfix/unsaved-synth-to-kit-row)");

	// The check must come before setupModelStackWithTimelineCounter (which is
	// right before the destructive operations begin)
	auto modelStackPos = body.find("setupModelStackWithTimelineCounter");
	CHECK_TEXT(modelStackPos != std::string::npos, "setupModelStackWithTimelineCounter not found");
	CHECK_TEXT(existsPos < modelStackPos,
	           "existsOnCard check must appear BEFORE setupModelStackWithTimelineCounter "
	           "to prevent destructive drum removal when file doesn't exist");
}

// ============================================================================
// bugfix/stereo-spread-osc-sync (upstream #3373)
//
// When unison stereo spread was nonzero, the subtractive synth stereo
// rendering path in Voice::render() passed doOscSync=false and nullptr
// for sync arrays to renderBasicSource(), completely disabling osc sync.
// Fix: pass sync parameters to the stereo path just like the mono path.
// ============================================================================

TEST_GROUP(StereoSpreadOscSyncGuard){};

TEST(StereoSpreadOscSyncGuard, stereoRenderingPathPassesSyncParams) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "model" / "voice" / "voice.cpp";
	CHECK_TEXT(fs::exists(cppPath), "voice.cpp not found — is firmware submodule checked out?");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read voice.cpp");

	// Find the stereo source rendering loop in the subtractive synth path.
	// It's the loop that iterates sourcesToRenderInStereo after the mono rendering.
	auto stereoLoopPos = src.find("sourcesToRenderInStereo & (1 << s)");
	CHECK_TEXT(stereoLoopPos != std::string::npos,
	           "Stereo source rendering loop not found in voice.cpp");

	// The renderBasicSource call within this loop must pass osc sync parameters,
	// not hardcoded false/nullptr. Look for the sync variable names that prove
	// the fix is present.
	auto nextFunc = src.find("\nvoid ", stereoLoopPos);
	auto region = src.substr(stereoLoopPos, (nextFunc != std::string::npos) ? nextFunc - stereoLoopPos : 500);

	CHECK_TEXT(region.find("doingOscSync") != std::string::npos,
	           "Stereo rendering loop must reference doingOscSync to pass sync "
	           "parameters to renderBasicSource (bugfix/stereo-spread-osc-sync, upstream #3373)");

	CHECK_TEXT(region.find("oscSyncPos") != std::string::npos,
	           "Stereo rendering loop must pass oscSyncPos to renderBasicSource "
	           "for oscillator sync to work with stereo spread");
}

} // namespace
