// Shadow header replacing firmware's gui/views/session_view.h
// clip_minder.cpp calls sessionView.transitionToSessionView(),
// and uses currentSong + getCurrentClip() which the real header provides transitively.

#pragma once

#include "song_mock.h"

class Clip;

// Free function getCurrentClip() — delegates to currentSong->getCurrentClip()
inline Clip* getCurrentClip() {
	return currentSong ? currentSong->getCurrentClip() : nullptr;
}

class SessionView {
public:
	void transitionToSessionView() {}
};

inline SessionView sessionView;
