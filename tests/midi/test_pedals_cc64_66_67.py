#!/usr/bin/env python3
"""Hardware verification: CC64 sustain, CC66 sostenuto, CC67 soft pedal.

Requires:
- Deluge with community features CC64/CC66/CC67 enabled
- DIN MIDI via USB adapter (default: "USB MIDI 2" out, "USB MIDI 1" in)
- MIDI Follow channel A = 1
- A sustaining patch (e.g. Piano 073)

Usage: python test_pedals_cc64_66_67.py [out_port] [in_port]

Hardware verified 2026-03-28, Piano 073 preset.
"""

import sys
import time

import mido

C, E, G = 72, 76, 79
CH = 0


def play(out, vel=80):
    for n in [C, E, G]:
        out.send(mido.Message("note_on", channel=CH, note=n, velocity=vel))
        time.sleep(0.05)


def release(out):
    for n in [C, E, G]:
        out.send(mido.Message("note_off", channel=CH, note=n, velocity=0))
        time.sleep(0.05)


def cc(out, num, val):
    out.send(mido.Message("control_change", channel=CH, control=num, value=val))


def silence(out):
    for c in range(16):
        out.send(mido.Message("control_change", channel=c, control=123, value=0))
        out.send(mido.Message("control_change", channel=c, control=120, value=0))


def announce(text, wait=3):
    print(text)
    time.sleep(wait)


def test_cc64_sustain(out):
    announce(">>> CC64 SUSTAIN PEDAL <<<", 2)

    announce("BASELINE: chord, no pedal...", 1)
    play(out)
    time.sleep(0.5)
    release(out)
    announce("  (decaying naturally)", 4)
    silence(out)
    announce("", 2)

    announce("SUSTAIN: chord on, keys released, pedal holds...", 1)
    cc(out, 64, 127)
    time.sleep(0.1)
    play(out)
    time.sleep(0.5)
    release(out)
    announce("  Keys released — should STILL ring...", 3)
    announce("  ...still ringing...", 3)
    announce("  Pedal UP — notes stop NOW", 1)
    cc(out, 64, 0)
    announce("  (silence?)", 4)
    silence(out)


def test_cc66_sostenuto(out):
    announce(">>> CC66 SOSTENUTO PEDAL <<<", 2)

    announce("BASELINE: chord, no pedal...", 1)
    play(out)
    time.sleep(0.5)
    release(out)
    announce("  (decaying naturally)", 4)
    silence(out)
    announce("", 2)

    announce("SOSTENUTO: playing chord, holding keys...", 1)
    play(out)
    time.sleep(1.5)
    announce("  Sostenuto DOWN — capturing these notes", 1)
    cc(out, 66, 127)
    time.sleep(0.3)
    announce("  Releasing keys — should STILL ring...", 1)
    release(out)
    announce("  ...still ringing...", 3)
    announce("  ...still ringing...", 3)
    announce("  Sostenuto UP — notes stop NOW", 1)
    cc(out, 66, 0)
    announce("  (silence?)", 4)
    silence(out)


def test_cc67_soft_pedal(out):
    announce(">>> CC67 SOFT PEDAL <<<", 2)

    announce("LOUD: vel=100, no pedal...", 1)
    play(out, 100)
    time.sleep(2.0)
    release(out)
    announce("  (remember that volume)", 4)
    silence(out)

    announce("SOFT: same vel=100, pedal DOWN...", 1)
    cc(out, 67, 127)
    time.sleep(0.1)
    play(out, 100)
    time.sleep(2.0)
    release(out)
    announce("  (was that quieter?)", 3)
    cc(out, 67, 0)
    silence(out)

    announce("LOUD AGAIN: pedal off, vel=100...", 1)
    play(out, 100)
    time.sleep(2.0)
    release(out)
    announce("  (back to full volume?)", 3)
    silence(out)


def main():
    out_port = sys.argv[1] if len(sys.argv) > 1 else "USB MIDI 2"
    out = mido.open_output(out_port)

    try:
        test_cc64_sustain(out)
        announce("", 2)
        test_cc66_sostenuto(out)
        announce("", 2)
        test_cc67_soft_pedal(out)
    finally:
        silence(out)
        out.close()

    print()
    print(">>> ALL PEDAL TESTS COMPLETE <<<")


if __name__ == "__main__":
    main()
