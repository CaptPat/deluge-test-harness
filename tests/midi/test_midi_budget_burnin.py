#!/usr/bin/env python3
"""MIDI Budget long-duration burn-in with SRAM trend tracking.

Runs sustained MIDI load during demo mode for an extended period (30+ min),
capturing SRAM checkpoint data at each song swap. Tracks memory trends
across swaps to detect gradual SRAM leaks or fragmentation death spirals.

Designed to run unattended for hours. Results are saved as JSON with
per-checkpoint SRAM data for historical regression comparison.

Requires:
  - Deluge in demo mode (PLAY+boot or SETTINGS > Community Features > Demo Mode)
  - Debug build (ENABLE_TEXT_OUTPUT + ENABLE_ON_DEVICE_TESTS)
  - Cable Matters USB-to-DIN adapter for MIDI stress input

Usage:
  python test_midi_budget_burnin.py                    # 30 min default
  python test_midi_budget_burnin.py --duration 3600    # 1 hour
  python test_midi_budget_burnin.py --rate 200         # moderate load
  python test_midi_budget_burnin.py --list-ports
"""

import argparse
import datetime
import json
import os
import re
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
DEFAULT_DURATION = 1800  # 30 minutes
DEFAULT_RATE = 150       # moderate sustained load

SYSEX_ENABLE = [0xF0, 0x00, 0x21, 0x7B, 0x01, 0x03, 0x00, 0x01, 0xF7]
SYSEX_DISABLE = [0xF0, 0x00, 0x21, 0x7B, 0x01, 0x03, 0x00, 0x00, 0xF7]

CH = 0

# Regex to parse SRAM checkpoint output from D_PRINTLN
# Expected format:
#   === Song Swap #3 ===
#   free SRAM:  847 KB / 1068 KB (27%)
#   fragments:  23 (68% fragmented)
#   largest:    112 KB contiguous
CHECKPOINT_RE = {
    "swap_num": re.compile(r"Song Swap #(\d+)"),
    "free_sram": re.compile(r"free SRAM:\s+(\d+)\s+KB\s*/\s*(\d+)\s+KB\s*\((\d+)%\)"),
    "fragments": re.compile(r"fragments:\s+(\d+)\s+\((\d+)%\s+fragmented\)"),
    "largest": re.compile(r"largest:\s+(\d+)\s+KB"),
    "clip_time": re.compile(r"clip time:\s+target\s+(\d+)s"),
    "song_time": re.compile(r"song time:\s+target\s+(\d+)s"),
}


# ---------------------------------------------------------------------------
# Debug monitor with checkpoint parsing
# ---------------------------------------------------------------------------


class BurninDebugMonitor:
    """Captures D_PRINTLN sysex and parses SRAM checkpoint blocks."""

    def __init__(self, out_port_idx, in_port_idx, log_path):
        self.out_port_idx = out_port_idx
        self.in_port_idx = in_port_idx
        self.log_path = log_path
        self.messages = []
        self.checkpoints = []  # list of parsed checkpoint dicts
        self.full_internal_count = 0
        self.crash_detected = False
        self._checkpoint_buffer = []
        self._in_checkpoint = False
        self._stop = False
        self._thread = None
        self._out = None
        self._inp = None
        self._logfile = None

    def start(self):
        self._out = rtmidi.MidiOut()
        self._inp = rtmidi.MidiIn()
        self._out.open_port(self.out_port_idx)
        self._inp.open_port(self.in_port_idx)
        self._inp.ignore_types(sysex=False)
        self._out.send_message(SYSEX_ENABLE)
        time.sleep(0.3)

        self._logfile = open(self.log_path, "w")
        self._logfile.write(f"# MIDI budget burn-in log — {datetime.datetime.now()}\n\n")

        self._stop = False
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        print(f"  Debug log: {self.log_path}")

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

            ts = time.time()
            ts_str = time.strftime("%H:%M:%S")
            self.messages.append((ts, text))

            if self._logfile:
                self._logfile.write(f"[{ts_str}] {text}\n")
                self._logfile.flush()

            # Track alerts
            if "FULL internal" in text:
                self.full_internal_count += 1
            for pattern in ["data abort", "FREEZE", "E259", "E369", "E370"]:
                if pattern in text:
                    self.crash_detected = True

            # Checkpoint parsing state machine
            if "Song Swap #" in text:
                self._in_checkpoint = True
                self._checkpoint_buffer = [text]
            elif self._in_checkpoint:
                self._checkpoint_buffer.append(text)
                # Checkpoints end after "next cycle:" or "song time:" line
                if "next cycle:" in text or "song time:" in text or "HOLDING" in text:
                    self._parse_checkpoint()
                    self._in_checkpoint = False
                elif len(self._checkpoint_buffer) > 12:
                    # Safety: don't buffer forever
                    self._in_checkpoint = False

    def _parse_checkpoint(self):
        """Parse buffered checkpoint lines into a structured dict."""
        block = "\n".join(self._checkpoint_buffer)
        cp = {"raw": block, "timestamp": time.time()}

        for key, regex in CHECKPOINT_RE.items():
            m = regex.search(block)
            if m:
                if key == "swap_num":
                    cp["swap_num"] = int(m.group(1))
                elif key == "free_sram":
                    cp["free_kb"] = int(m.group(1))
                    cp["total_kb"] = int(m.group(2))
                    cp["free_pct"] = int(m.group(3))
                elif key == "fragments":
                    cp["fragment_count"] = int(m.group(1))
                    cp["frag_pct"] = int(m.group(2))
                elif key == "largest":
                    cp["largest_kb"] = int(m.group(1))
                elif key == "clip_time":
                    cp["clip_target_s"] = int(m.group(1))
                elif key == "song_time":
                    cp["song_target_s"] = int(m.group(1))

        self.checkpoints.append(cp)
        swap_num = cp.get("swap_num", "?")
        free_pct = cp.get("free_pct", "?")
        print(f"  [checkpoint] Swap #{swap_num}: {free_pct}% free, "
              f"{cp.get('fragment_count', '?')} fragments, "
              f"{cp.get('largest_kb', '?')} KB largest")

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

    def is_alive(self, timeout=10.0):
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
        if self._logfile:
            self._logfile.write(f"\n# Ended — {len(self.messages)} messages, "
                                f"{len(self.checkpoints)} checkpoints, "
                                f"{self.full_internal_count} FULL internal\n")
            self._logfile.close()

    def trend_analysis(self):
        """Analyze SRAM trend across checkpoints. Returns summary dict."""
        if len(self.checkpoints) < 2:
            return {"trend": "insufficient_data", "checkpoints": len(self.checkpoints)}

        frees = [cp.get("free_pct", 0) for cp in self.checkpoints if "free_pct" in cp]
        if len(frees) < 2:
            return {"trend": "insufficient_data", "checkpoints": len(self.checkpoints)}

        first_free = frees[0]
        last_free = frees[-1]
        min_free = min(frees)
        max_free = max(frees)
        delta = last_free - first_free

        if delta < -10:
            trend = "declining"  # Losing >10% across run — potential leak
        elif delta < -3:
            trend = "slightly_declining"
        elif delta > 3:
            trend = "improving"
        else:
            trend = "stable"

        return {
            "trend": trend,
            "checkpoints": len(self.checkpoints),
            "first_free_pct": first_free,
            "last_free_pct": last_free,
            "min_free_pct": min_free,
            "max_free_pct": max_free,
            "delta_pct": delta,
            "per_checkpoint": [
                {"swap": cp.get("swap_num"), "free_pct": cp.get("free_pct"),
                 "fragments": cp.get("fragment_count"), "largest_kb": cp.get("largest_kb")}
                for cp in self.checkpoints
            ],
        }


# ---------------------------------------------------------------------------
# MIDI load generator
# ---------------------------------------------------------------------------


def sustained_load(out, rate, duration, stop_event):
    """Sustained mixed CC + note load for burn-in. Returns (cc_sent, note_pairs)."""
    interval = 1.0 / rate
    cc_sent = 0
    note_pairs = 0
    end = time.time() + duration
    val = 0

    while time.time() < end and not stop_event.is_set():
        # CC traffic
        for _ in range(5):
            if stop_event.is_set() or time.time() >= end:
                break
            out.send(mido.Message("control_change", channel=CH, control=1, value=val))
            val = (val + 1) % 128
            cc_sent += 1
            time.sleep(interval)

        # Note pair
        note = 48 + (note_pairs % 36)
        out.send(mido.Message("note_on", channel=CH, note=note, velocity=80))
        time.sleep(0.03)
        out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))
        note_pairs += 1

    return cc_sent, note_pairs


def silence(out):
    for c in range(16):
        out.send(mido.Message("control_change", channel=c, control=123, value=0))
        out.send(mido.Message("control_change", channel=c, control=120, value=0))


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
    parser = argparse.ArgumentParser(description="MIDI Budget long-duration burn-in")
    parser.add_argument("--stress-out", default=DEFAULT_STRESS_OUT)
    parser.add_argument("--rate", type=int, default=DEFAULT_RATE,
                        help=f"Sustained MIDI rate (default: {DEFAULT_RATE})")
    parser.add_argument("--duration", type=int, default=DEFAULT_DURATION,
                        help=f"Total duration in seconds (default: {DEFAULT_DURATION})")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()

    if args.list_ports:
        list_ports()
        return

    sysex_out, sysex_in = find_sysex_ports()
    if sysex_out is None or sysex_in is None:
        print("ERROR: Cannot find Deluge sysex ports")
        list_ports()
        sys.exit(1)

    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = os.path.join(os.path.dirname(__file__), "results")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, f"budget_burnin_{timestamp}.log")

    monitor = BurninDebugMonitor(sysex_out, sysex_in, log_path)
    monitor.start()
    time.sleep(1)

    try:
        out = mido.open_output(args.stress_out)
    except (OSError, IOError) as e:
        print(f"ERROR: Cannot open stress port '{args.stress_out}': {e}")
        monitor.stop()
        sys.exit(1)

    session = TestSession("midi_budget_burnin")
    result = TestResult("sustained-burnin")

    duration_min = args.duration / 60
    print(f"\n{'#'*60}")
    print(f"# MIDI BUDGET BURN-IN")
    print(f"# Rate: {args.rate} msgs/sec for {duration_min:.0f} minutes")
    print(f"# {datetime.datetime.now()}")
    print(f"{'#'*60}\n")

    stop_event = threading.Event()
    counts = [0, 0]

    def worker():
        cc, notes = sustained_load(out, args.rate, args.duration, stop_event)
        counts[0] = cc
        counts[1] = notes

    stress = threading.Thread(target=worker, daemon=True)
    start_time = time.time()

    try:
        stress.start()

        # Progress reporting
        last_report = 0
        while stress.is_alive():
            stress.join(timeout=30)
            elapsed = time.time() - start_time
            elapsed_min = elapsed / 60

            if int(elapsed) // 60 > last_report:
                last_report = int(elapsed) // 60
                cp_count = len(monitor.checkpoints)
                print(f"  [{elapsed_min:.0f}m] cc={counts[0]} notes={counts[1]} "
                      f"checkpoints={cp_count} full_internal={monitor.full_internal_count}")

                if monitor.crash_detected:
                    print("  !! CRASH DETECTED — stopping")
                    stop_event.set()
                    break

    except KeyboardInterrupt:
        print("\n\nInterrupted by user.")
        stop_event.set()

    time.sleep(3)
    silence(out)
    out.close()

    elapsed = time.time() - start_time
    monitor.stop()

    # Analyze trend
    trend = monitor.trend_analysis()

    if monitor.crash_detected:
        result.failed(
            f"Device crashed during burn-in at {args.rate} msgs/sec after {elapsed/60:.1f} min",
            rate=args.rate,
            duration_s=round(elapsed),
            cc_sent=counts[0],
            note_pairs=counts[1],
            full_internal=monitor.full_internal_count,
            trend=trend,
        )
    elif trend["trend"] == "declining" and trend.get("last_free_pct", 100) < 10:
        result.failed(
            f"SRAM declining to critical levels: {trend['last_free_pct']}% free after "
            f"{len(monitor.checkpoints)} swaps",
            rate=args.rate,
            duration_s=round(elapsed),
            cc_sent=counts[0],
            note_pairs=counts[1],
            full_internal=monitor.full_internal_count,
            trend=trend,
        )
    else:
        result.passed(
            f"Survived {elapsed/60:.1f} min at {args.rate} msgs/sec. "
            f"SRAM trend: {trend['trend']} ({trend.get('first_free_pct', '?')}% → "
            f"{trend.get('last_free_pct', '?')}% across {len(monitor.checkpoints)} swaps)",
            rate=args.rate,
            duration_s=round(elapsed),
            cc_sent=counts[0],
            note_pairs=counts[1],
            full_internal=monitor.full_internal_count,
            trend=trend,
        )

    session.add(result)

    # Summary
    print(f"\n{'='*60}")
    print("BURN-IN RESULTS")
    print(f"{'='*60}")
    print(f"  Duration:      {elapsed/60:.1f} minutes")
    print(f"  Rate:          {args.rate} msgs/sec")
    print(f"  CC sent:       {counts[0]}")
    print(f"  Note pairs:    {counts[1]}")
    print(f"  FULL internal: {monitor.full_internal_count}")
    print(f"  Checkpoints:   {len(monitor.checkpoints)}")
    print(f"  SRAM trend:    {trend['trend']}")

    if trend.get("per_checkpoint"):
        print(f"\n  SRAM History:")
        for cp in trend["per_checkpoint"]:
            print(f"    Swap #{cp.get('swap', '?')}: {cp.get('free_pct', '?')}% free, "
                  f"{cp.get('fragments', '?')} fragments, {cp.get('largest_kb', '?')} KB largest")

    print(f"\n  Verdict: {result.status.upper()}")
    print(f"  {result.details}")

    print(f"\n{session.summary_text()}")
    session.save()
    print(f"  Log: {log_path}")

    sys.exit(0 if result.status == "pass" else 1)


if __name__ == "__main__":
    main()
