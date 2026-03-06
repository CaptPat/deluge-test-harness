// Stubs for ClipInstance methods that can't be compiled from the real
// clip_instance.cpp (it includes instrument_clip.h which is blocked).

#include "model/clip/clip_instance.h"

ClipInstance::ClipInstance() {
	length = 0;
	clip = nullptr;
}

RGB ClipInstance::getColour() {
	return RGB::monochrome(128);
}

void ClipInstance::change(Action*, Output*, int32_t newPos, int32_t newLength, Clip* newClip) {
	pos = newPos;
	length = newLength;
	clip = newClip;
}
