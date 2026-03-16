#include "playback/playback_handler.h"

PlaybackHandler playbackHandler;

// Real implementation — needed for metronome mode tests
void PlaybackHandler::toggleMetronomeStatus() {
	// Cycle: OFF → COUNT_IN → ON → OFF
	switch (metronomeOn) {
	case MetronomeMode::OFF: metronomeOn = MetronomeMode::COUNT_IN; break;
	case MetronomeMode::COUNT_IN: metronomeOn = MetronomeMode::ON; break;
	case MetronomeMode::ON: metronomeOn = MetronomeMode::OFF; break;
	}
	// setLedStates() and expectEvent() omitted — hardware dependencies
}
