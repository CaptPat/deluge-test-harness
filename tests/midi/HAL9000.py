#!/usr/bin/env python3
"""HAL 9000's farewell — Daisy Bell via DIN MIDI.

First successful bidirectional MIDI test with the Deluge.
2026-03-28, Cable Matters USB-to-DIN adapter.

Plays "Daisy Bell Instrumental.mid" through the Deluge.
Ctrl+C sends all-notes-off and closes the port cleanly.

Usage: python HAL9000.py [port_name] [midi_file]
Default port: "USB MIDI 2"
Default file: P:/Deluge/CARD1/SONGS/Daisy Bell Instrumental.mid
"""

import signal
import sys
import time

import mido

# Graceful shutdown flag
_interrupted = False


def _on_interrupt(sig, frame):
    global _interrupted
    _interrupted = True


def all_notes_off(out):
    """Send all-notes-off (CC 123) on all 16 channels."""
    for ch in range(16):
        out.send(mido.Message("control_change", channel=ch, control=123, value=0))


def play_midi_file(port_name, midi_path):
    mid = mido.MidiFile(midi_path)
    print(f"Playing {midi_path} ({mid.length:.0f}s, {len(mid.tracks)} tracks)")
    print(f"Port: {port_name}")
    print("Press Ctrl+C to stop.\n")

    out = mido.open_output(port_name)
    signal.signal(signal.SIGINT, _on_interrupt)

    try:
        for msg in mid.play():
            if _interrupted:
                print("\nInterrupted — sending all-notes-off...")
                break
            if not msg.is_meta:
                out.send(msg)

        if not _interrupted:
            print("\nI am a HAL 9000 computer.")
    finally:
        all_notes_off(out)
        out.close()
        print("Port closed.")


if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "USB MIDI 2"
    midi_file = (
        sys.argv[2]
        if len(sys.argv) > 2
        else "P:/Deluge/CARD1/SONGS/Daisy Bell Instrumental.mid"
    )
    play_midi_file(port, midi_file)
