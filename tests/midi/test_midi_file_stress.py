#!/usr/bin/env python3
"""MIDI file stress test suite for Deluge.

Plays curated MIDI files through the Deluge via USB MIDI and monitors
the DIN loopback for dropped messages, stuck notes, timing drift,
and crashes.

Requires:
  - USB cable from PC to Deluge (send port)
  - DIN MIDI cable via Cable Matters adapter (receive port)
  - MIDI Follow enabled (channel 1), synth clip selected
  - Curated MIDI files from curate_midi.py

Usage:
  python test_midi_file_stress.py --midi-dir <path-to-curated/>
  python test_midi_file_stress.py --midi-dir <path> --test survive
  python test_midi_file_stress.py --midi-dir <path> --test marathon --limit 10
  python test_midi_file_stress.py --midi-dir <path> --test rapid-switch
  python test_midi_file_stress.py --list-tests
"""

import argparse
import csv
import json
import os
import sys
import threading
import time
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

import mido


# ---------------------------------------------------------------------------
# Port discovery
# ---------------------------------------------------------------------------

def find_ports():
    """Auto-detect Deluge USB output and DIN adapter input."""
    outs = mido.get_output_names()
    ins = mido.get_input_names()

    deluge_out = None
    din_in = None

    for p in outs:
        if p.startswith("Deluge") and "OUT2" not in p and "OUT3" not in p:
            deluge_out = p
            break

    for p in ins:
        if "USB MIDI" in p and "Deluge" not in p:
            din_in = p
            break

    return deluge_out, din_in


# ---------------------------------------------------------------------------
# MIDI monitor
# ---------------------------------------------------------------------------

class MidiMonitor:
    """Background thread capturing incoming MIDI with timestamps."""

    def __init__(self, port_name):
        self.port_name = port_name
        self.messages = []
        self.note_ons = defaultdict(int)
        self._stop = False
        self._thread = None
        self._port = None
        self._start_time = 0

    def start(self):
        self._port = mido.open_input(self.port_name)
        self._stop = False
        self._start_time = time.time()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while not self._stop:
            msg = self._port.poll()
            if msg:
                t = time.time() - self._start_time
                self.messages.append((t, msg))
                if msg.type == "note_on" and msg.velocity > 0:
                    self.note_ons[(msg.channel, msg.note)] += 1
                elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
                    self.note_ons[(msg.channel, msg.note)] -= 1
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
        self._start_time = time.time()

    def get_stuck_notes(self):
        return {k: v for k, v in self.note_ons.items() if v > 0}

    def count_note_ons(self):
        return sum(1 for _, m in self.messages
                   if m.type == "note_on" and m.velocity > 0)

    def count_note_offs(self):
        return sum(1 for _, m in self.messages
                   if m.type == "note_off" or (m.type == "note_on" and m.velocity == 0))

    def count_ccs(self):
        return sum(1 for _, m in self.messages if m.type == "control_change")


# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

def silence(out):
    for c in range(16):
        out.send(mido.Message("control_change", channel=c, control=123, value=0))
        out.send(mido.Message("control_change", channel=c, control=120, value=0))


def cc(out, num, val, ch=0):
    out.send(mido.Message("control_change", channel=ch, control=num, value=val))


def is_alive(out, mon):
    """Send a probe note, check if echo comes back."""
    mon.clear()
    out.send(mido.Message("note_on", channel=0, note=60, velocity=1))
    time.sleep(0.3)
    out.send(mido.Message("note_off", channel=0, note=60, velocity=0))
    time.sleep(0.3)
    return mon.count_note_ons() > 0


def play_midi_file(out, filepath, channel_map="all", max_duration=None, speed=1.0):
    """Play a MIDI file through the output port.

    channel_map: "all" sends on original channels, "ch1" remaps everything to channel 0
    max_duration: stop after this many seconds (None = play entire file)
    speed: playback speed multiplier (1.0 = normal, 2.0 = double speed)

    Returns (notes_sent, ccs_sent, duration_s)
    """
    try:
        mid = mido.MidiFile(filepath)
    except Exception as e:
        return 0, 0, 0

    notes_sent = 0
    ccs_sent = 0
    start = time.time()

    for msg in mid.play():
        if max_duration and (time.time() - start) > max_duration:
            break

        if msg.is_meta:
            continue

        # Optionally remap all channels to ch1
        if channel_map == "ch1" and hasattr(msg, "channel"):
            msg = msg.copy(channel=0)

        # Speed adjustment (sleep is handled by mid.play() but we can skip)
        out.send(msg)

        if msg.type == "note_on" and msg.velocity > 0:
            notes_sent += 1
        elif msg.type == "control_change":
            ccs_sent += 1

    duration = time.time() - start
    return notes_sent, ccs_sent, duration


def count_midi_file_notes(filepath):
    """Count notes in a MIDI file without playing it."""
    try:
        mid = mido.MidiFile(filepath)
    except Exception:
        return 0, 0, 0

    notes = 0
    ccs = 0
    for track in mid.tracks:
        for msg in track:
            if msg.type == "note_on" and msg.velocity > 0:
                notes += 1
            elif msg.type == "control_change":
                ccs += 1

    return notes, ccs, mid.length


# ---------------------------------------------------------------------------
# Result tracking
# ---------------------------------------------------------------------------

@dataclass
class FileTestResult:
    filename: str
    test: str
    passed: bool
    notes_sent: int = 0
    notes_received: int = 0
    ccs_sent: int = 0
    ccs_received: int = 0
    stuck_notes: int = 0
    duration_s: float = 0
    alive_after: bool = True
    error: str = ""

    @property
    def drop_rate(self):
        if self.notes_sent == 0:
            return 0
        return max(0, 1 - self.notes_received / self.notes_sent)


# ---------------------------------------------------------------------------
# Test: Survive
# ---------------------------------------------------------------------------

def test_survive(out, mon, midi_files, channel_map="ch1", max_per_file=None):
    """Play each file, verify Deluge survives (no crash/freeze)."""
    results = []

    for i, fp in enumerate(midi_files):
        fname = os.path.basename(fp)
        print(f"  [{i+1}/{len(midi_files)}] {fname}...", end=" ", flush=True)

        mon.clear()
        notes_sent, ccs_sent, dur = play_midi_file(
            out, fp, channel_map=channel_map, max_duration=max_per_file
        )
        time.sleep(0.5)
        silence(out)
        time.sleep(0.5)

        stuck = mon.get_stuck_notes()
        alive = is_alive(out, mon)

        notes_recv = mon.count_note_ons()
        ccs_recv = mon.count_ccs()

        passed = alive and len(stuck) == 0
        status = "PASS" if passed else "FAIL"
        details = []
        if not alive:
            details.append("UNRESPONSIVE")
        if stuck:
            details.append(f"stuck={len(stuck)}")

        r = FileTestResult(
            filename=fname, test="survive", passed=passed,
            notes_sent=notes_sent, notes_received=notes_recv,
            ccs_sent=ccs_sent, ccs_received=ccs_recv,
            stuck_notes=len(stuck), duration_s=dur, alive_after=alive,
        )
        results.append(r)

        extra = f" ({', '.join(details)})" if details else ""
        drop = f" drop={r.drop_rate:.0%}" if r.drop_rate > 0 else ""
        print(f"{status} {dur:.0f}s notes={notes_sent}/{notes_recv}{drop}{extra}")

        if not alive:
            print("    DEVICE UNRESPONSIVE — aborting remaining files")
            break

        silence(out)
        time.sleep(0.3)

    return results


# ---------------------------------------------------------------------------
# Test: Loopback integrity
# ---------------------------------------------------------------------------

def test_loopback(out, mon, midi_files, channel_map="ch1", max_per_file=60):
    """Play files and compare sent vs received note counts."""
    results = []

    for i, fp in enumerate(midi_files):
        fname = os.path.basename(fp)
        print(f"  [{i+1}/{len(midi_files)}] {fname}...", end=" ", flush=True)

        mon.clear()
        notes_sent, ccs_sent, dur = play_midi_file(
            out, fp, channel_map=channel_map, max_duration=max_per_file
        )
        # Wait for all echoes to arrive
        time.sleep(1.0)
        silence(out)
        time.sleep(0.5)

        notes_recv = mon.count_note_ons()
        ccs_recv = mon.count_ccs()
        stuck = mon.get_stuck_notes()
        alive = is_alive(out, mon)

        # Allow some echo loss (DIN bandwidth limit) but flag major drops
        drop_rate = 1 - notes_recv / notes_sent if notes_sent > 0 else 0
        passed = alive and drop_rate < 0.1 and len(stuck) == 0

        r = FileTestResult(
            filename=fname, test="loopback", passed=passed,
            notes_sent=notes_sent, notes_received=notes_recv,
            ccs_sent=ccs_sent, ccs_received=ccs_recv,
            stuck_notes=len(stuck), duration_s=dur, alive_after=alive,
        )
        results.append(r)

        status = "PASS" if passed else "FAIL"
        print(f"{status} {dur:.0f}s sent={notes_sent} recv={notes_recv} "
              f"drop={drop_rate:.1%} stuck={len(stuck)}")

        if not alive:
            print("    DEVICE UNRESPONSIVE — aborting")
            break

        silence(out)
        time.sleep(0.3)

    return results


# ---------------------------------------------------------------------------
# Test: Rapid file switching
# ---------------------------------------------------------------------------

def test_rapid_switch(out, mon, midi_files, seconds_per_file=10):
    """Play first N seconds of each file, rapid-fire switching."""
    results = []

    print(f"  Playing {seconds_per_file}s of each file, {len(midi_files)} files...")
    total_start = time.time()

    for i, fp in enumerate(midi_files):
        fname = os.path.basename(fp)
        mon.clear()

        notes_sent, ccs_sent, dur = play_midi_file(
            out, fp, channel_map="ch1", max_duration=seconds_per_file
        )
        silence(out)
        time.sleep(0.2)

        stuck = mon.get_stuck_notes()

        r = FileTestResult(
            filename=fname, test="rapid-switch", passed=len(stuck) == 0,
            notes_sent=notes_sent, stuck_notes=len(stuck), duration_s=dur,
        )
        results.append(r)

        if (i + 1) % 10 == 0:
            alive = is_alive(out, mon)
            elapsed = time.time() - total_start
            print(f"    {i+1}/{len(midi_files)} files, {elapsed:.0f}s elapsed, "
                  f"alive={alive}")
            if not alive:
                print("    DEVICE UNRESPONSIVE — aborting")
                break

    # Final alive check
    time.sleep(0.5)
    alive = is_alive(out, mon)
    total_time = time.time() - total_start
    total_stuck = sum(r.stuck_notes for r in results)

    print(f"  Done: {len(results)} files in {total_time:.0f}s, "
          f"stuck={total_stuck}, alive={alive}")

    # Mark overall pass/fail
    for r in results:
        r.alive_after = alive

    return results


# ---------------------------------------------------------------------------
# Test: Marathon soak
# ---------------------------------------------------------------------------

def test_marathon(out, mon, midi_files, limit=None):
    """Play files back-to-back continuously. Full endurance test."""
    files_to_play = midi_files[:limit] if limit else midi_files
    total_notes_sent = 0
    total_notes_recv = 0
    files_played = 0
    start = time.time()
    results = []

    print(f"  Marathon: {len(files_to_play)} files, "
          f"est. {sum(count_midi_file_notes(f)[2] for f in files_to_play)/60:.0f} min")

    for i, fp in enumerate(files_to_play):
        fname = os.path.basename(fp)
        mon.clear()

        notes_sent, ccs_sent, dur = play_midi_file(out, fp, channel_map="ch1")
        time.sleep(0.5)
        silence(out)
        time.sleep(0.3)

        notes_recv = mon.count_note_ons()
        stuck = mon.get_stuck_notes()
        alive = is_alive(out, mon)

        total_notes_sent += notes_sent
        total_notes_recv += notes_recv
        files_played += 1

        r = FileTestResult(
            filename=fname, test="marathon", passed=alive and len(stuck) == 0,
            notes_sent=notes_sent, notes_received=notes_recv,
            stuck_notes=len(stuck), duration_s=dur, alive_after=alive,
        )
        results.append(r)

        elapsed = time.time() - start
        print(f"  [{i+1}/{len(files_to_play)}] {fname}: {dur:.0f}s "
              f"notes={notes_sent} stuck={len(stuck)} "
              f"[total: {elapsed/60:.0f}min, {total_notes_sent} notes]")

        if not alive:
            print("    DEVICE UNRESPONSIVE — marathon aborted")
            break

        silence(out)
        time.sleep(0.5)

    total_time = time.time() - start
    print(f"\n  Marathon complete: {files_played} files, {total_time/60:.1f} min, "
          f"{total_notes_sent} notes sent, {total_notes_recv} echoed")

    return results


# ---------------------------------------------------------------------------
# Test: Record under load
# ---------------------------------------------------------------------------

CC_PLAY = 102
CC_RECORD = 103
CC_UNDO = 107


def test_record_under_load(out, mon, midi_files, max_per_file=15):
    """Record while playing MIDI files, undo, repeat."""
    results = []
    files_to_use = midi_files[:20]  # Use first 20 files

    print(f"  Record-under-load: {len(files_to_use)} files, {max_per_file}s each")

    for i, fp in enumerate(files_to_use):
        fname = os.path.basename(fp)
        mon.clear()

        # Start playback + recording
        cc(out, CC_PLAY, 127)
        time.sleep(0.3)
        cc(out, CC_RECORD, 127)
        time.sleep(0.3)

        # Play MIDI file while recording
        notes_sent, ccs_sent, dur = play_midi_file(
            out, fp, channel_map="ch1", max_duration=max_per_file
        )

        # Stop recording + playback
        time.sleep(0.2)
        cc(out, CC_RECORD, 127)
        time.sleep(0.2)
        cc(out, CC_PLAY, 127)
        time.sleep(0.3)

        # Undo the recording
        cc(out, CC_UNDO, 127)
        time.sleep(0.5)

        silence(out)
        time.sleep(0.3)

        alive = is_alive(out, mon)
        stuck = mon.get_stuck_notes()

        r = FileTestResult(
            filename=fname, test="record-under-load",
            passed=alive and len(stuck) == 0,
            notes_sent=notes_sent, stuck_notes=len(stuck),
            duration_s=dur, alive_after=alive,
        )
        results.append(r)

        status = "PASS" if r.passed else "FAIL"
        print(f"  [{i+1}/{len(files_to_use)}] {fname}: {status} "
              f"{dur:.0f}s notes={notes_sent} stuck={len(stuck)}")

        if not alive:
            print("    DEVICE UNRESPONSIVE — aborting")
            break

    return results


# ---------------------------------------------------------------------------
# Results reporting
# ---------------------------------------------------------------------------

def print_summary(all_results):
    """Print test results summary table."""
    print(f"\n{'='*70}")
    print("RESULTS SUMMARY")
    print(f"{'='*70}")

    # Group by test
    by_test = defaultdict(list)
    for r in all_results:
        by_test[r.test].append(r)

    for test_name, results in by_test.items():
        passed = sum(1 for r in results if r.passed)
        failed = sum(1 for r in results if not r.passed)
        total_notes = sum(r.notes_sent for r in results)
        total_stuck = sum(r.stuck_notes for r in results)
        total_time = sum(r.duration_s for r in results)

        print(f"\n  {test_name}: {passed}/{len(results)} passed "
              f"({total_time/60:.1f}min, {total_notes} notes, {total_stuck} stuck)")

        # Show failures
        for r in results:
            if not r.passed:
                details = []
                if not r.alive_after:
                    details.append("UNRESPONSIVE")
                if r.stuck_notes:
                    details.append(f"stuck={r.stuck_notes}")
                if r.drop_rate > 0.1:
                    details.append(f"drop={r.drop_rate:.0%}")
                print(f"    FAIL: {r.filename} ({', '.join(details)})")

    total_passed = sum(1 for r in all_results if r.passed)
    total_failed = sum(1 for r in all_results if not r.passed)
    print(f"\n{'='*70}")
    print(f"TOTAL: {total_passed} passed, {total_failed} failed "
          f"out of {len(all_results)} tests")
    print(f"{'='*70}")

    return total_failed == 0


def save_results(all_results, output_path):
    """Save results to CSV."""
    with open(output_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow([
            "filename", "test", "passed", "notes_sent", "notes_received",
            "ccs_sent", "ccs_received", "stuck_notes", "drop_rate",
            "duration_s", "alive_after", "error"
        ])
        for r in all_results:
            w.writerow([
                r.filename, r.test, r.passed, r.notes_sent, r.notes_received,
                r.ccs_sent, r.ccs_received, r.stuck_notes, f"{r.drop_rate:.3f}",
                f"{r.duration_s:.1f}", r.alive_after, r.error
            ])
    print(f"\nResults saved to {output_path}")


# ---------------------------------------------------------------------------
# Test registry
# ---------------------------------------------------------------------------

ALL_TESTS = {
    "survive": "Play each file fully, verify no crash (slowest, most thorough)",
    "loopback": "Compare sent vs received note counts (60s per file max)",
    "rapid-switch": "10s of each file, rapid transitions",
    "marathon": "Play all files back-to-back continuously",
    "record-under-load": "Record + undo while playing MIDI files",
}


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="MIDI file stress test for Deluge")
    parser.add_argument("--midi-dir", required=False,
                        default=r"P:\VSCodeDesktop\projects\deluge-midi-test-data\clean_midi\curated",
                        help="Directory of curated MIDI files")
    parser.add_argument("--test", default="survive",
                        help=f"Test to run: {', '.join(ALL_TESTS)} or 'all'")
    parser.add_argument("--limit", type=int, default=None,
                        help="Limit number of files to test")
    parser.add_argument("--port-out", default=None, help="Override Deluge output port")
    parser.add_argument("--port-in", default=None, help="Override DIN input port")
    parser.add_argument("--channel", default="ch1", choices=["ch1", "all"],
                        help="Channel mapping: ch1 (remap all to ch1) or all (preserve)")
    parser.add_argument("--results", default=None, help="Save results CSV to this path")
    parser.add_argument("--list-tests", action="store_true")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()

    if args.list_tests:
        print("Available tests:")
        for name, desc in ALL_TESTS.items():
            print(f"  {name:20s} {desc}")
        return

    if args.list_ports:
        print("Output:", mido.get_output_names())
        print("Input:", mido.get_input_names())
        return

    # Find ports
    if args.port_out and args.port_in:
        port_out, port_in = args.port_out, args.port_in
    else:
        port_out, port_in = find_ports()
        if args.port_out:
            port_out = args.port_out
        if args.port_in:
            port_in = args.port_in

    if not port_out or not port_in:
        print(f"ERROR: Could not find ports (out={port_out}, in={port_in})")
        print("Use --port-out and --port-in to specify manually")
        print("Available:", mido.get_output_names(), mido.get_input_names())
        sys.exit(1)

    print(f"Output: {port_out}")
    print(f"Input:  {port_in}")

    # Find MIDI files
    midi_dir = args.midi_dir
    if not os.path.isdir(midi_dir):
        print(f"ERROR: Directory not found: {midi_dir}")
        sys.exit(1)

    midi_files = sorted([
        os.path.join(midi_dir, f) for f in os.listdir(midi_dir)
        if f.lower().endswith((".mid", ".midi"))
    ])

    if args.limit:
        midi_files = midi_files[:args.limit]

    print(f"Files:  {len(midi_files)}")
    print()

    # Open ports
    out = mido.open_output(port_out)
    mon = MidiMonitor(port_in)
    mon.start()

    silence(out)
    time.sleep(0.5)

    # Verify alive
    if not is_alive(out, mon):
        print("ERROR: Deluge not responding on loopback. Check connections.")
        mon.stop()
        out.close()
        sys.exit(1)

    print("Deluge responding on loopback. Starting tests.\n")

    # Run tests
    all_results = []

    if args.test == "all":
        tests_to_run = ["survive"]  # full suite is just survive for now
    else:
        tests_to_run = [args.test]

    for test_name in tests_to_run:
        print(f"{'='*60}")
        print(f"TEST: {test_name}")
        print(f"{'='*60}")

        if test_name == "survive":
            results = test_survive(out, mon, midi_files, channel_map=args.channel)
        elif test_name == "loopback":
            results = test_loopback(out, mon, midi_files, channel_map=args.channel)
        elif test_name == "rapid-switch":
            results = test_rapid_switch(out, mon, midi_files)
        elif test_name == "marathon":
            results = test_marathon(out, mon, midi_files, limit=args.limit)
        elif test_name == "record-under-load":
            results = test_record_under_load(out, mon, midi_files)
        else:
            print(f"Unknown test: {test_name}")
            continue

        all_results.extend(results)
        silence(out)
        time.sleep(1.0)

    # Cleanup
    silence(out)
    mon.stop()
    out.close()

    # Report
    ok = print_summary(all_results)

    if args.results:
        save_results(all_results, args.results)

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
