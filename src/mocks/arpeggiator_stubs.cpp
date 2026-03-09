// Link-time stubs for arpeggiator.cpp serialization dependencies.
// The arp engine logic (lines 1-1614) is the testable code; the serialization
// tail (lines 1615-1989) uses these symbols but we don't need real implementations.

#include "definitions_cxx.hpp"
#include "util/firmware_version.h"

// ── song_firmware_version global ────────────────────────────────────────
FirmwareVersion song_firmware_version = FirmwareVersion::community({0, 0, 0});

// ── Arp string conversion stubs ─────────────────────────────────────────
ArpMode oldModeToArpMode(OldArpMode) { return ArpMode::OFF; }
ArpNoteMode oldModeToArpNoteMode(OldArpMode) { return ArpNoteMode::UP; }
ArpOctaveMode oldModeToArpOctaveMode(OldArpMode) { return ArpOctaveMode::UP; }

char const* oldArpModeToString(OldArpMode) { return "off"; }
OldArpMode stringToOldArpMode(char const*) { return OldArpMode::OFF; }

char const* arpModeToString(ArpMode) { return "off"; }
ArpMode stringToArpMode(char const*) { return ArpMode::OFF; }

char const* arpNoteModeToString(ArpNoteMode) { return "up"; }
ArpNoteMode stringToArpNoteMode(char const*) { return ArpNoteMode::UP; }

char const* arpOctaveModeToString(ArpOctaveMode) { return "up"; }
ArpOctaveMode stringToArpOctaveMode(char const*) { return ArpOctaveMode::UP; }

char const* arpMpeModSourceToString(ArpMpeModSource) { return "off"; }
ArpMpeModSource stringToArpMpeModSource(char const*) { return ArpMpeModSource::OFF; }
