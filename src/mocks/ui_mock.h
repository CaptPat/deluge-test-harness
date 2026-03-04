#pragma once

// Mock UI manager — tracks current active UI for test assertions.
// In the real firmware, getCurrentUI() returns the top of the UI stack.

namespace MockUI {

enum class ViewType {
	NONE,
	SESSION,
	ARRANGER,
	INSTRUMENT_CLIP,
	AUDIO_CLIP,
	AUTOMATION,
	KEYBOARD,
	SOUND_EDITOR,
	BROWSER,
};

inline ViewType currentView = ViewType::NONE;

void reset();
void setView(ViewType v);
ViewType getView();

} // namespace MockUI
