#!/usr/bin/env python3
"""MIDI stress test suite for Deluge via USB MIDI.

Sends progressively harder MIDI workloads and monitors the echo
(MIDI thru) for dropped messages, stuck notes, and freezes.

Requires: USB cable from PC to Deluge, MIDI Follow enabled (ch 1),
a synth clip selected in session view.

Usage:
  python test_midi_stress.py [--port-out "Deluge 2"] [--port-in "Deluge 1"]
  python test_midi_stress.py --list-ports
  python test_midi_stress.py --test note-flood
  python test_midi_stress.py --test all
"""

import argparse
import sys
import threading
import time
from collections import defaultdict
from dataclasses import dataclass, field

import mido

# Defaults — Deluge USB MIDI
DEFAULT_OUT = "Deluge 2"
DEFAULT_IN = "Deluge 1"
CH = 0  # MIDI channel 1 (0-indexed)

# CC assignments (from MIDI learn session)
CC_PLAY = 102
CC_RECORD = 103
CC_LOOP = 104
CC_UNDO = 107
CC_REDO = 108
CC_LOAD_NEXT = 112


@dataclass
class TestResult:
    name: str
    passed: bool
    sent: int = 0
    received: int = 0
    stuck_notes: int = 0
    dropped: int = 0
    duration_s: float = 0
    details: str = ""


class MidiMonitor:
    """Background thread that captures incoming MIDI messages."""

    def __init__(self, port_name):
        self.port_name = port_name
        self.messages = []
        self.note_ons = defaultdict(int)  # note -> count of active note-ons
        self._stop = False
        self._thread = None
        self._port = None

    def start(self):
        self._port = mido.open_input(self.port_name)
        self._stop = False
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while not self._stop:
            msg = self._port.poll()
            if msg:
                self.messages.append((time.time(), msg))
                if msg.type == "note_on" and msg.velocity > 0:
                    self.note_ons[msg.note] += 1
                elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
                    self.note_ons[msg.note] -= 1
            else:
                time.sleep(0.001)

    def stop(self):
        self._stop = True
        if self._thread:
            self._thread.join(timeout=2)
        if self._port:
            self._port.close()

    def clear(self):
        self.messages.clear()
        self.note_ons.clear()

    def get_stuck_notes(self):
        """Return notes with unbalanced on/off counts."""
        return {n: c for n, c in self.note_ons.items() if c > 0}

    def count_type(self, msg_type):
        return sum(1 for _, m in self.messages if m.type == msg_type)

    def count_cc(self, cc_num=None):
        if cc_num is None:
            return sum(1 for _, m in self.messages if m.type == "control_change")
        return sum(1 for _, m in self.messages
                   if m.type == "control_change" and m.control == cc_num)


def silence(out):
    """All notes off on all channels."""
    for c in range(16):
        out.send(mido.Message("control_change", channel=c, control=123, value=0))
        out.send(mido.Message("control_change", channel=c, control=120, value=0))


def cc(out, num, val, ch=CH):
    out.send(mido.Message("control_change", channel=ch, control=num, value=val))


# ---------------------------------------------------------------------------
# Test 1: Note flood — ramp up speed
# ---------------------------------------------------------------------------
def test_note_flood(out, mon):
    """Send notes at increasing rates, check for stuck notes and echo."""
    rates = [10, 25, 50, 100, 200, 500]
    total_sent = 0
    results_detail = []

    for rate in rates:
        mon.clear()
        count = min(rate, 100)  # send up to 100 notes per rate tier
        interval = 1.0 / rate
        note_dur = interval * 0.4

        print(f"  {rate} notes/sec ({count} notes)...", end=" ", flush=True)

        for i in range(count):
            note = 36 + (i % 48)  # C2 to B5
            out.send(mido.Message("note_on", channel=CH, note=note, velocity=80))
            time.sleep(note_dur)
            out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))
            remaining = interval - note_dur
            if remaining > 0:
                time.sleep(remaining)
            total_sent += 1

        time.sleep(0.5)  # let echo drain
        stuck = mon.get_stuck_notes()
        echo_notes = mon.count_type("note_on")

        status = "OK" if not stuck else f"STUCK: {stuck}"
        print(f"echo={echo_notes}/{count*2} {status}")
        results_detail.append(f"{rate}/s: echo={echo_notes}, stuck={len(stuck)}")

        if stuck:
            silence(out)
            time.sleep(0.3)
            return TestResult(
                name="note-flood",
                passed=False,
                sent=total_sent,
                received=echo_notes,
                stuck_notes=len(stuck),
                details=f"Stuck at {rate}/s: {stuck}. " + "; ".join(results_detail),
            )

    silence(out)
    return TestResult(
        name="note-flood",
        passed=True,
        sent=total_sent,
        received=mon.count_type("note_on"),
        details="; ".join(results_detail),
    )


# ---------------------------------------------------------------------------
# Test 2: Polyphony exhaustion
# ---------------------------------------------------------------------------
def test_polyphony(out, mon):
    """Stack many simultaneous notes, then release all."""
    mon.clear()
    max_notes = 64
    notes_held = []

    print(f"  Stacking {max_notes} notes...", end=" ", flush=True)
    for i in range(max_notes):
        note = 24 + i  # C1 upward
        out.send(mido.Message("note_on", channel=CH, note=note, velocity=70))
        notes_held.append(note)
        time.sleep(0.02)

    time.sleep(1.0)
    print(f"releasing...", end=" ", flush=True)

    for note in notes_held:
        out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))
        time.sleep(0.01)

    time.sleep(1.0)
    stuck = mon.get_stuck_notes()

    if stuck:
        silence(out)
        time.sleep(0.3)

    status = "OK" if not stuck else f"STUCK: {stuck}"
    print(status)

    return TestResult(
        name="polyphony",
        passed=len(stuck) == 0,
        sent=max_notes,
        stuck_notes=len(stuck),
        details=f"Held {max_notes} simultaneous notes. {status}",
    )


# ---------------------------------------------------------------------------
# Test 3: CC parameter sweep
# ---------------------------------------------------------------------------
def test_cc_flood(out, mon):
    """Rapid CC changes on filter frequency while playing a note."""
    mon.clear()
    total_sent = 0

    # Hold a note so we hear the filter sweep
    out.send(mido.Message("note_on", channel=CH, note=60, velocity=80))
    time.sleep(0.2)

    sweeps = 5
    print(f"  {sweeps} filter sweeps (CC#74, 128 steps each)...", end=" ", flush=True)

    for s in range(sweeps):
        # Sweep up
        for val in range(0, 128, 2):
            out.send(mido.Message("control_change", channel=CH, control=74, value=val))
            total_sent += 1
            time.sleep(0.003)
        # Sweep down
        for val in range(127, -1, -2):
            out.send(mido.Message("control_change", channel=CH, control=74, value=val))
            total_sent += 1
            time.sleep(0.003)

    time.sleep(0.3)
    out.send(mido.Message("note_off", channel=CH, note=60, velocity=0))
    time.sleep(0.5)

    echo_cc = mon.count_cc(74)
    stuck = mon.get_stuck_notes()

    if stuck:
        silence(out)

    print(f"sent={total_sent} echo_cc={echo_cc} stuck={len(stuck)}")

    return TestResult(
        name="cc-flood",
        passed=len(stuck) == 0,
        sent=total_sent,
        received=echo_cc,
        stuck_notes=len(stuck),
        details=f"{sweeps} sweeps, {total_sent} CCs sent, {echo_cc} echoed",
    )


# ---------------------------------------------------------------------------
# Test 4: Transport hammering
# ---------------------------------------------------------------------------
def test_transport(out, mon):
    """Rapid play/stop toggles."""
    mon.clear()
    cycles = 50

    print(f"  {cycles} play/stop cycles...", end=" ", flush=True)

    for i in range(cycles):
        cc(out, CC_PLAY, 127)
        time.sleep(0.05)
        cc(out, CC_PLAY, 127)
        time.sleep(0.05)

    time.sleep(1.0)

    # Make sure we're stopped
    cc(out, CC_PLAY, 127)
    time.sleep(0.2)
    cc(out, CC_PLAY, 127)
    time.sleep(0.5)

    # Verify Deluge still responds — send a note
    mon.clear()
    out.send(mido.Message("note_on", channel=CH, note=60, velocity=80))
    time.sleep(0.5)
    out.send(mido.Message("note_off", channel=CH, note=60, velocity=0))
    time.sleep(0.5)

    alive = mon.count_type("note_on") > 0 or mon.count_type("note_off") > 0
    print("alive" if alive else "NO RESPONSE")

    silence(out)
    return TestResult(
        name="transport",
        passed=alive,
        sent=cycles * 2,
        details=f"{cycles} play/stop cycles, device {'responsive' if alive else 'UNRESPONSIVE'}",
    )


# ---------------------------------------------------------------------------
# Test 5: Record + notes + undo cycle
# ---------------------------------------------------------------------------
def test_record_undo(out, mon):
    """Record notes, undo, repeat — tests recording state machine."""
    mon.clear()
    cycles = 10

    print(f"  {cycles} record/undo cycles...", end=" ", flush=True)

    for i in range(cycles):
        # Start playback + recording
        cc(out, CC_PLAY, 127)
        time.sleep(0.3)
        cc(out, CC_RECORD, 127)
        time.sleep(0.3)

        # Send a few notes
        for note in [60, 64, 67]:
            out.send(mido.Message("note_on", channel=CH, note=note, velocity=80))
            time.sleep(0.1)
            out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))
            time.sleep(0.05)

        # Stop recording + playback
        cc(out, CC_RECORD, 127)
        time.sleep(0.2)
        cc(out, CC_PLAY, 127)
        time.sleep(0.3)

        # Undo
        cc(out, CC_UNDO, 127)
        time.sleep(0.3)

    time.sleep(0.5)

    # Verify alive
    mon.clear()
    out.send(mido.Message("note_on", channel=CH, note=60, velocity=80))
    time.sleep(0.5)
    out.send(mido.Message("note_off", channel=CH, note=60, velocity=0))
    time.sleep(0.5)

    alive = mon.count_type("note_on") > 0 or mon.count_type("note_off") > 0
    print("alive" if alive else "NO RESPONSE")

    silence(out)
    return TestResult(
        name="record-undo",
        passed=alive,
        sent=cycles * 3,
        details=f"{cycles} record/undo cycles, device {'responsive' if alive else 'UNRESPONSIVE'}",
    )


# ---------------------------------------------------------------------------
# Test 6: Combined load — notes + CCs + transport
# ---------------------------------------------------------------------------
def test_combined_load(out, mon):
    """Simultaneous notes, CC sweeps, and transport for 30 seconds."""
    mon.clear()
    duration = 30
    total_sent = 0

    print(f"  {duration}s combined load (notes + CCs + transport)...")

    cc(out, CC_PLAY, 127)
    time.sleep(0.5)

    start = time.time()
    note_idx = 0
    cc_val = 0
    cc_dir = 1
    transport_timer = time.time()

    while time.time() - start < duration:
        # Note burst (3 notes)
        note = 36 + (note_idx % 48)
        out.send(mido.Message("note_on", channel=CH, note=note, velocity=80))
        time.sleep(0.02)
        out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))
        total_sent += 2
        note_idx += 1

        # CC sweep step
        out.send(mido.Message("control_change", channel=CH, control=74, value=cc_val))
        total_sent += 1
        cc_val += cc_dir * 3
        if cc_val >= 127:
            cc_val = 127
            cc_dir = -1
        elif cc_val <= 0:
            cc_val = 0
            cc_dir = 1

        # Transport toggle every 5 seconds
        if time.time() - transport_timer > 5:
            cc(out, CC_PLAY, 127)
            time.sleep(0.05)
            cc(out, CC_PLAY, 127)
            total_sent += 2
            transport_timer = time.time()

        time.sleep(0.03)

        # Progress
        elapsed = time.time() - start
        if int(elapsed) % 10 == 0 and int(elapsed) > 0:
            stuck = mon.get_stuck_notes()
            if stuck:
                print(f"    {int(elapsed)}s: STUCK NOTES {stuck}")

    cc(out, CC_PLAY, 127)
    time.sleep(0.5)

    stuck = mon.get_stuck_notes()
    if stuck:
        silence(out)
        time.sleep(0.3)

    # Verify alive
    mon.clear()
    out.send(mido.Message("note_on", channel=CH, note=60, velocity=80))
    time.sleep(0.5)
    out.send(mido.Message("note_off", channel=CH, note=60, velocity=0))
    time.sleep(0.5)
    alive = mon.count_type("note_on") > 0 or mon.count_type("note_off") > 0

    total_echo = len(mon.messages)
    print(f"    sent={total_sent} stuck={len(stuck)} alive={alive}")

    silence(out)
    return TestResult(
        name="combined-load",
        passed=alive and len(stuck) == 0,
        sent=total_sent,
        stuck_notes=len(stuck),
        duration_s=duration,
        details=f"{duration}s, {total_sent} msgs, stuck={len(stuck)}, alive={alive}",
    )


# ---------------------------------------------------------------------------
# Test 7: Song swap under MIDI load
# ---------------------------------------------------------------------------
def test_song_swap(out, mon):
    """Load next song via CC while notes are active."""
    mon.clear()

    print("  Sending notes + load-next-song...", end=" ", flush=True)

    # Start playing
    cc(out, CC_PLAY, 127)
    time.sleep(0.5)

    # Send some notes
    for note in [60, 64, 67, 72]:
        out.send(mido.Message("note_on", channel=CH, note=note, velocity=80))
        time.sleep(0.1)

    # Trigger song swap while notes are held
    cc(out, CC_LOAD_NEXT, 127)
    time.sleep(3.0)

    # Release notes
    for note in [60, 64, 67, 72]:
        out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))

    silence(out)
    time.sleep(1.0)

    # Verify alive
    mon.clear()
    out.send(mido.Message("note_on", channel=CH, note=60, velocity=80))
    time.sleep(0.5)
    out.send(mido.Message("note_off", channel=CH, note=60, velocity=0))
    time.sleep(0.5)

    alive = mon.count_type("note_on") > 0 or mon.count_type("note_off") > 0
    print("alive" if alive else "NO RESPONSE")

    silence(out)
    return TestResult(
        name="song-swap",
        passed=alive,
        details=f"Song swap with held notes, device {'responsive' if alive else 'UNRESPONSIVE'}",
    )


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

ALL_TESTS = {
    "note-flood": test_note_flood,
    "polyphony": test_polyphony,
    "cc-flood": test_cc_flood,
    "transport": test_transport,
    "record-undo": test_record_undo,
    "combined-load": test_combined_load,
    "song-swap": test_song_swap,
}


def run_tests(out_port, in_port, test_names):
    out = mido.open_output(out_port)
    mon = MidiMonitor(in_port)
    mon.start()

    silence(out)
    time.sleep(0.5)

    results = []
    for name in test_names:
        func = ALL_TESTS[name]
        print(f"\n{'='*60}")
        print(f"TEST: {name}")
        print(f"{'='*60}")
        try:
            result = func(out, mon)
        except Exception as e:
            result = TestResult(name=name, passed=False, details=f"Exception: {e}")
        results.append(result)
        silence(out)
        time.sleep(1.0)

    mon.stop()
    out.close()

    # Summary
    print(f"\n{'='*60}")
    print("RESULTS")
    print(f"{'='*60}")
    for r in results:
        status = "PASS" if r.passed else "FAIL"
        print(f"  [{status}] {r.name}: {r.details}")

    passed = sum(1 for r in results if r.passed)
    failed = sum(1 for r in results if not r.passed)
    print(f"\n{passed} passed, {failed} failed out of {len(results)} tests")
    return all(r.passed for r in results)


def main():
    parser = argparse.ArgumentParser(description="MIDI stress test for Deluge")
    parser.add_argument("--port-out", default=DEFAULT_OUT, help=f"MIDI output port (default: {DEFAULT_OUT})")
    parser.add_argument("--port-in", default=DEFAULT_IN, help=f"MIDI input port (default: {DEFAULT_IN})")
    parser.add_argument("--test", default="all", help="Test name or 'all' (choices: " + ", ".join(ALL_TESTS) + ")")
    parser.add_argument("--list-ports", action="store_true", help="List MIDI ports and exit")
    args = parser.parse_args()

    if args.list_ports:
        print("Output ports:")
        for p in mido.get_output_names():
            print(f"  {p}")
        print("Input ports:")
        for p in mido.get_input_names():
            print(f"  {p}")
        return

    if args.test == "all":
        test_names = list(ALL_TESTS.keys())
    else:
        if args.test not in ALL_TESTS:
            print(f"Unknown test: {args.test}")
            print(f"Choices: {', '.join(ALL_TESTS)}")
            sys.exit(1)
        test_names = [args.test]

    ok = run_tests(args.port_out, args.port_in, test_names)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
