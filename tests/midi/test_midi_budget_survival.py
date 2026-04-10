#!/usr/bin/env python3
"""MIDI Budget survival tests — escalating rate sweep with crash detection.

Sends progressively harder MIDI workloads and verifies the device survives
at every rate. Unlike test_rate_sweep.py (basic pass/fail), this suite
monitors D_PRINTLN for budget state, SRAM exhaustion, and crash signatures.

Results are saved as JSON with per-rate detail for regression comparison.

Requires:
  - Deluge connected via USB with debug build (ENABLE_TEXT_OUTPUT)
  - Cable Matters USB-to-DIN adapter for MIDI stress input
  - A song playing (normal mode or demo mode)

Usage:
  python test_midi_budget_survival.py
  python test_midi_budget_survival.py --rates 100,200,300,500
  python test_midi_budget_survival.py --per-rate 30
  python test_midi_budget_survival.py --list-ports
"""

import argparse
import datetime
import os
import sys
import threading
import time

import mido
import rtmidi

from deluge_test_utils import TestResult, TestSession

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

DEFAULT_STRESS_OUT = "USB MIDI 2"
DEFAULT_RATES = [50, 100, 150, 200, 250, 300, 400, 500]
DEFAULT_PER_RATE = 30  # seconds per rate step

SYSEX_ENABLE = [0xF0, 0x00, 0x21, 0x7B, 0x01, 0x03, 0x00, 0x01, 0xF7]
SYSEX_DISABLE = [0xF0, 0x00, 0x21, 0x7B, 0x01, 0x03, 0x00, 0x00, 0xF7]

CH = 0

# ---------------------------------------------------------------------------
# Debug monitor (reused from test_midi_budget_behavior.py pattern)
# ---------------------------------------------------------------------------


class SurvivalDebugMonitor:
    """Lightweight D_PRINTLN monitor focused on crash detection and budget state."""

    def __init__(self, out_port_idx, in_port_idx):
        self.out_port_idx = out_port_idx
        self.in_port_idx = in_port_idx
        self.messages = []
        self.full_internal_count = 0
        self.throttle_count = 0
        self.overload_count = 0
        self.crash_detected = False
        self._stop = False
        self._thread = None
        self._out = None
        self._inp = None

    def start(self):
        self._out = rtmidi.MidiOut()
        self._inp = rtmidi.MidiIn()
        self._out.open_port(self.out_port_idx)
        self._inp.open_port(self.in_port_idx)
        self._inp.ignore_types(sysex=False)
        self._out.send_message(SYSEX_ENABLE)
        time.sleep(0.3)
        self._stop = False
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while not self._stop:
            msg = self._inp.get_message()
            if msg is None:
                time.sleep(0.001)
                continue
            data = msg[0]
            if data[0] != 0xF0:
                continue
            text = self._decode(data)
            if text is None:
                continue

            self.messages.append((time.time(), text))

            if "FULL internal" in text:
                self.full_internal_count += 1
            if "MIDI Throttling" in text:
                self.throttle_count += 1
            if "MIDI Overload" in text:
                self.overload_count += 1
            for crash_pattern in ["data abort", "FREEZE", "E259", "E369", "E370"]:
                if crash_pattern in text:
                    self.crash_detected = True
                    break

    def _decode(self, data):
        if len(data) < 8:
            return None
        if data[1] == 0x00 and data[2] == 0x21:
            text_start = 8
        elif data[1] == 0x7D:
            text_start = 5
        else:
            return None
        if text_start == 8 and (len(data) < 9 or data[5] != 0x03 or data[6] != 0x40):
            return None
        text_bytes = bytes([b & 0x7F for b in data[text_start:-1]])
        return text_bytes.decode("ascii", errors="replace").rstrip()

    def is_alive(self, timeout=5.0):
        if not self.messages:
            return True
        return (time.time() - self.messages[-1][0]) < timeout

    def stop(self):
        self._stop = True
        if self._thread:
            self._thread.join(timeout=2)
        if self._out:
            try:
                self._out.send_message(SYSEX_DISABLE)
            except Exception:
                pass
            self._out.close_port()
        if self._inp:
            self._inp.close_port()

    def snapshot_and_reset(self):
        """Take a snapshot of current counters and reset for next rate step."""
        snap = {
            "full_internal": self.full_internal_count,
            "throttle": self.throttle_count,
            "overload": self.overload_count,
            "crash": self.crash_detected,
            "debug_msgs": len(self.messages),
        }
        self.full_internal_count = 0
        self.throttle_count = 0
        self.overload_count = 0
        self.crash_detected = False
        return snap


# ---------------------------------------------------------------------------
# MIDI helpers
# ---------------------------------------------------------------------------


def silence(out):
    for c in range(16):
        out.send(mido.Message("control_change", channel=c, control=123, value=0))
        out.send(mido.Message("control_change", channel=c, control=120, value=0))


def send_mixed_load(out, rate, duration, stop_event=None):
    """Mixed CC + note traffic at given rate. Returns (cc_sent, note_pairs)."""
    interval = 1.0 / rate
    cc_sent = 0
    note_pairs = 0
    end = time.time() + duration
    val = 0

    while time.time() < end:
        if stop_event and stop_event.is_set():
            break

        # 10 CCs per note pair
        for _ in range(10):
            if time.time() >= end:
                break
            out.send(mido.Message("control_change", channel=CH, control=1, value=val))
            val = (val + 1) % 128
            cc_sent += 1
            time.sleep(interval)

        # Note pair
        note = 60 + (note_pairs % 24)
        out.send(mido.Message("note_on", channel=CH, note=note, velocity=80))
        time.sleep(0.02)
        out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))
        note_pairs += 1

    return cc_sent, note_pairs


# ---------------------------------------------------------------------------
# Rate sweep
# ---------------------------------------------------------------------------


def run_sweep(monitor, stress_port, rates, per_rate):
    """Run progressive rate sweep. Returns list of per-rate TestResults."""
    results = []

    try:
        out = mido.open_output(stress_port)
    except (OSError, IOError) as e:
        r = TestResult(f"sweep-setup")
        r.errored(f"Cannot open stress port '{stress_port}': {e}")
        return [r]

    for rate in rates:
        print(f"\n  --- {rate} msgs/sec for {per_rate}s ---")
        result = TestResult(f"survive-{rate}")

        pre_snap = monitor.snapshot_and_reset()

        stop = threading.Event()
        counts = [0, 0]

        def worker():
            cc, notes = send_mixed_load(out, rate, per_rate, stop)
            counts[0] = cc
            counts[1] = notes

        t = threading.Thread(target=worker, daemon=True)
        start_time = time.time()
        t.start()

        # Monitor while running
        while t.is_alive():
            t.join(timeout=5)
            if monitor.crash_detected:
                stop.set()
                break

        elapsed = time.time() - start_time
        time.sleep(1)  # Let trailing debug messages arrive

        snap = monitor.snapshot_and_reset()
        actual_rate = counts[0] / elapsed if elapsed > 0 else 0

        alive = monitor.is_alive() if len(monitor.messages) > 5 else True

        if monitor.crash_detected or not alive:
            result.failed(
                f"Device crashed at {rate} msgs/sec",
                target_rate=rate,
                actual_rate=round(actual_rate),
                cc_sent=counts[0],
                note_pairs=counts[1],
                elapsed_s=round(elapsed, 1),
                **snap,
            )
            results.append(result)
            print(f"    FAIL: crash detected — stopping sweep")
            break
        else:
            result.passed(
                f"Survived {rate} msgs/sec ({round(actual_rate)} actual) for {round(elapsed)}s",
                target_rate=rate,
                actual_rate=round(actual_rate),
                cc_sent=counts[0],
                note_pairs=counts[1],
                elapsed_s=round(elapsed, 1),
                **snap,
            )
            results.append(result)
            status_parts = [f"sent={counts[0]}"]
            if snap["throttle"]:
                status_parts.append(f"throttle={snap['throttle']}")
            if snap["overload"]:
                status_parts.append(f"overload={snap['overload']}")
            if snap["full_internal"]:
                status_parts.append(f"full_internal={snap['full_internal']}")
            print(f"    PASS: {', '.join(status_parts)}")

        # Cooldown between rates
        if rate != rates[-1]:
            print(f"    Cooldown 5s...")
            silence(out)
            time.sleep(5)

    silence(out)
    out.close()
    return results


# ---------------------------------------------------------------------------
# Port discovery
# ---------------------------------------------------------------------------


def find_sysex_ports():
    out = rtmidi.MidiOut()
    inp = rtmidi.MidiIn()
    out_port = None
    in_port = None

    for i in range(out.get_port_count()):
        name = out.get_port_name(i)
        if "Deluge" in name and "MIDIOUT" not in name:
            out_port = i
        elif "MIDIOUT3" in name and "Deluge" in name:
            out_port = i

    for i in range(inp.get_port_count()):
        name = inp.get_port_name(i)
        if "Deluge" in name and "MIDIIN" not in name:
            in_port = i
            break

    return out_port, in_port


def list_ports():
    print("=== mido ports ===")
    print("  Outputs:", mido.get_output_names())
    print("  Inputs:", mido.get_input_names())
    out = rtmidi.MidiOut()
    inp = rtmidi.MidiIn()
    print("=== rtmidi ports ===")
    for i in range(out.get_port_count()):
        print(f"  Out {i}: {out.get_port_name(i)}")
    for i in range(inp.get_port_count()):
        print(f"  In  {i}: {inp.get_port_name(i)}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(description="MIDI Budget survival rate sweep")
    parser.add_argument("--stress-out", default=DEFAULT_STRESS_OUT)
    parser.add_argument("--rates", default=",".join(str(r) for r in DEFAULT_RATES),
                        help=f"Comma-separated rates (default: {DEFAULT_RATES})")
    parser.add_argument("--per-rate", type=int, default=DEFAULT_PER_RATE,
                        help=f"Seconds per rate step (default: {DEFAULT_PER_RATE})")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()

    if args.list_ports:
        list_ports()
        return

    rates = [int(r) for r in args.rates.split(",")]

    sysex_out, sysex_in = find_sysex_ports()
    if sysex_out is None or sysex_in is None:
        print("ERROR: Cannot find Deluge sysex ports")
        list_ports()
        sys.exit(1)

    monitor = SurvivalDebugMonitor(sysex_out, sysex_in)
    monitor.start()
    time.sleep(1)

    session = TestSession("midi_budget_survival")

    print(f"\n{'#'*60}")
    print(f"# MIDI BUDGET SURVIVAL SWEEP")
    print(f"# Rates: {rates} msgs/sec, {args.per_rate}s each")
    print(f"# {datetime.datetime.now()}")
    print(f"{'#'*60}")

    try:
        results = run_sweep(monitor, args.stress_out, rates, args.per_rate)
        for r in results:
            session.add(r)
    except KeyboardInterrupt:
        print("\n\nInterrupted.")

    monitor.stop()

    # Summary
    print(f"\n{'='*60}")
    print("SWEEP SUMMARY")
    print(f"{'='*60}")
    for r in session.results:
        symbol = "PASS" if r.status == "pass" else "FAIL" if r.status == "fail" else "ERR"
        print(f"  [{symbol}] {r.name}: {r.details}")

    print()
    print(session.summary_text())
    session.save()

    sys.exit(0 if all(r.status in ("pass", "skip") for r in session.results) else 1)


if __name__ == "__main__":
    main()
