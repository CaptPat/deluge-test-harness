// Link-time stubs for arpeggiator.cpp serialization dependencies.
// String conversion functions are real implementations (copied from functions.cpp)
// to support round-trip serialization testing.

#include "definitions_cxx.hpp"
#include "util/firmware_version.h"

#include <cstring>

// ── song_firmware_version global ────────────────────────────────────────
FirmwareVersion song_firmware_version = FirmwareVersion::community({0, 0, 0});

// ── Old arp mode conversions (stubs — old format not tested) ────────────
ArpMode oldModeToArpMode(OldArpMode) { return ArpMode::OFF; }
ArpNoteMode oldModeToArpNoteMode(OldArpMode) { return ArpNoteMode::UP; }
ArpOctaveMode oldModeToArpOctaveMode(OldArpMode) { return ArpOctaveMode::UP; }

char const* oldArpModeToString(OldArpMode) { return "off"; }
OldArpMode stringToOldArpMode(char const*) { return OldArpMode::OFF; }

// ── ArpMode ─────────────────────────────────────────────────────────────
char const* arpModeToString(ArpMode mode) {
	switch (mode) {
	case ArpMode::ARP:
		return "arp";
	default:
		return "off";
	}
}

ArpMode stringToArpMode(char const* string) {
	if (!strcmp(string, "arp")) {
		return ArpMode::ARP;
	}
	return ArpMode::OFF;
}

// ── ArpNoteMode ─────────────────────────────────────────────────────────
char const* arpNoteModeToString(ArpNoteMode mode) {
	switch (mode) {
	case ArpNoteMode::DOWN:
		return "down";
	case ArpNoteMode::UP_DOWN:
		return "upDown";
	case ArpNoteMode::AS_PLAYED:
		return "asPlayed";
	case ArpNoteMode::RANDOM:
		return "random";
	case ArpNoteMode::WALK1:
		return "walk1";
	case ArpNoteMode::WALK2:
		return "walk2";
	case ArpNoteMode::WALK3:
		return "walk3";
	case ArpNoteMode::PATTERN:
		return "pattern";
	default:
		return "up";
	}
}

ArpNoteMode stringToArpNoteMode(char const* string) {
	if (!strcmp(string, "down")) {
		return ArpNoteMode::DOWN;
	}
	else if (!strcmp(string, "upDown")) {
		return ArpNoteMode::UP_DOWN;
	}
	else if (!strcmp(string, "asPlayed")) {
		return ArpNoteMode::AS_PLAYED;
	}
	else if (!strcmp(string, "walk1")) {
		return ArpNoteMode::WALK1;
	}
	else if (!strcmp(string, "walk2")) {
		return ArpNoteMode::WALK2;
	}
	else if (!strcmp(string, "walk3")) {
		return ArpNoteMode::WALK3;
	}
	else if (!strcmp(string, "pattern")) {
		return ArpNoteMode::PATTERN;
	}
	else if (!strcmp(string, "random")) {
		return ArpNoteMode::RANDOM;
	}
	return ArpNoteMode::UP;
}

// ── ArpOctaveMode ───────────────────────────────────────────────────────
char const* arpOctaveModeToString(ArpOctaveMode mode) {
	switch (mode) {
	case ArpOctaveMode::DOWN:
		return "down";
	case ArpOctaveMode::UP_DOWN:
		return "upDown";
	case ArpOctaveMode::ALTERNATE:
		return "alt";
	case ArpOctaveMode::RANDOM:
		return "random";
	default:
		return "up";
	}
}

ArpOctaveMode stringToArpOctaveMode(char const* string) {
	if (!strcmp(string, "down")) {
		return ArpOctaveMode::DOWN;
	}
	else if (!strcmp(string, "upDown")) {
		return ArpOctaveMode::RANDOM;
	}
	else if (!strcmp(string, "alt")) {
		return ArpOctaveMode::ALTERNATE;
	}
	else if (!strcmp(string, "random")) {
		return ArpOctaveMode::RANDOM;
	}
	return ArpOctaveMode::UP;
}

// ── ArpMpeModSource ─────────────────────────────────────────────────────
char const* arpMpeModSourceToString(ArpMpeModSource modSource) {
	switch (modSource) {
	case ArpMpeModSource::MPE_Y:
		return "y";
	case ArpMpeModSource::AFTERTOUCH:
		return "z";
	default:
		return "off";
	}
}

ArpMpeModSource stringToArpMpeModSource(char const* string) {
	if (!strcmp(string, "y")) {
		return ArpMpeModSource::MPE_Y;
	}
	else if (!strcmp(string, "z")) {
		return ArpMpeModSource::AFTERTOUCH;
	}
	return ArpMpeModSource::OFF;
}
