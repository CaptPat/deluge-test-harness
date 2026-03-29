#!/usr/bin/env python3
"""HAL 9000's farewell — Daisy Bell via DIN MIDI.

First successful bidirectional MIDI test with the Deluge.
2026-03-28, Cable Matters USB-to-DIN adapter.

Usage: python HAL9000.py [port_name]
Default port: "USB MIDI 2"
"""

import sys
import time

import mido


def play(out, n, beats, vel=72, beat_dur=0.30):
    out.send(mido.Message("note_on", channel=0, note=n, velocity=vel))
    time.sleep(beats * beat_dur)
    out.send(mido.Message("note_off", channel=0, note=n, velocity=0))
    time.sleep(0.02)


def daisy_bell(port_name="USB MIDI 2"):
    out = mido.open_output(port_name)

    # G is UP (G5=79), melody arcs over C major
    C, D, E, F, G, A, B = 72, 74, 76, 77, 79, 69, 71

    print("Daisy Bell...")

    # GGG EEE CCC GGG ABC AAC GGG GGG
    play(out, G, 3, 70)
    play(out, E, 3, 65)
    play(out, C, 3, 70)
    play(out, G, 3, 65)
    play(out, A, 1, 65)
    play(out, B, 1, 65)
    play(out, C, 1, 70)
    play(out, A, 2, 60)
    play(out, C, 1, 65)
    play(out, G, 3, 55)
    play(out, G, 3, 50)

    # DDD GGG EEE CCC ABC DDE DDD DDE FED GGE DCC CCD EE
    play(out, D, 3, 70)
    play(out, G, 3, 65)
    play(out, E, 3, 65)
    play(out, C, 3, 70)
    play(out, A, 1, 65)
    play(out, B, 1, 65)
    play(out, C, 1, 70)
    play(out, D, 2, 65)
    play(out, E, 1, 65)
    play(out, D, 3, 60)
    play(out, D, 2, 65)
    play(out, E, 1, 65)
    play(out, F, 1, 70)
    play(out, E, 1, 65)
    play(out, D, 1, 60)
    play(out, G, 2, 70)
    play(out, E, 1, 65)
    play(out, D, 1, 60)
    play(out, C, 2, 65)
    play(out, C, 2, 70)
    play(out, D, 1, 65)
    play(out, E, 4, 50)

    print("I am a HAL 9000 computer.")
    out.close()


if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "USB MIDI 2"
    daisy_bell(port)
