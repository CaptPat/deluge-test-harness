// Session mock — real implementations of pure-logic methods that need Song definition
#include "playback/mode/session.h"
#include "song_mock.h"

Session session{};

const RGB defaultClipSectionColours[] = {
    RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128),
    RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128),
    RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128),
    RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128),
};

bool Session::areAnyClipsArmed() {
	for (int32_t l = 0; l < currentSong->sessionClips.getNumElements(); l++) {
		Clip* clip = currentSong->sessionClips.getClipAtIndex(l);
		if (clip->armState != ArmState::OFF) {
			return true;
		}
	}
	return false;
}
