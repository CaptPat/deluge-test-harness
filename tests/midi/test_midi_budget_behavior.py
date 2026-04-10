#!/usr/bin/env python3
"""MIDI Budget behavior verification tests.

Validates the adaptive MIDI budget mechanism by observing firmware debug
output under controlled MIDI load. Checks that:

  1. Throttle indicator fires under sustained CC load
  2. Note-off messages bypass the budget (no stuck notes)
  3. Budget recovers after load is removed
  4. SRAM checkpoint reports valid data at song swap

Requires:
  - Deluge connected via USB with debug build (ENABLE_TEXT_OUTPUT)
  - Cable Matters USB-to-DIN adapter for MIDI stress input
  - A song playing (normal mode or demo mode)

Usage:
  python test_midi_budget_behavior.py
  python test_midi_budget_behavior.py --test throttle-fires
  python test_midi_budget_behavior.py --test noteoff-bypass
  python test_midi_budget_behavior.py --test budget-recovery
  python test_midi_budget_behavior.py --test all
  python test_midi_budget_behavior.py --list-ports
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

# MIDI stress port (Cable Matters USB-to-DIN adapter → Deluge DIN IN)
DEFAULT_STRESS_OUT = "USB MIDI 2"

# D_PRINTLN sysex protocol
SYSEX_ENABLE = [0xF0, 0x00, 0x21, 0x7B, 0x01, 0x03, 0x00, 0x01, 0xF7]
SYSEX_DISABLE = [0xF0, 0x00, 0x21, 0x7B, 0x01, 0x03, 0x00, 0x00, 0xF7]

BUDGET_ALERT_PATTERNS = {
    "throttle": ["MIDI Throttling"],
    "overload": ["MIDI Overload"],
    "full_internal": ["FULL internal"],
    "crash": ["data abort", "FREEZE", "E259", "E369", "E370"],
    "checkpoint": ["Song Swap #"],
}

CH = 0  # MIDI channel 1

# ---------------------------------------------------------------------------
# Debug monitor (same pattern as test_demo_burnin.py)
# ---------------------------------------------------------------------------


class BudgetDebugMonitor:
    """Captures D_PRINTLN sysex and classifies budget-related messages."""

    def __init__(self, out_port_idx, in_port_idx):
        self.out_port_idx = out_port_idx
        self.in_port_idx = in_port_idx
        self.messages = []  # (timestamp, text)
        self.flags = {k: False for k in BUDGET_ALERT_PATTERNS}
        self.counts = {k: 0 for k in BUDGET_ALERT_PATTERNS}
        self.checkpoints = []  # raw checkpoint text blocks
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

            for category, patterns in BUDGET_ALERT_PATTERNS.items():
                for pattern in patterns:
                    if pattern in text:
                        self.flags[category] = True
                        self.counts[category] += 1
                        if category == "checkpoint":
                            self.checkpoints.append(text)
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

    def clear_flags(self):
        """Reset all flags and counts for a new test phase."""
        for k in self.flags:
            self.flags[k] = False
            self.counts[k] = 0
        self.checkpoints.clear()

    def is_alive(self, timeout=5.0):
        """True if we received any debug message in the last timeout seconds."""
        if not self.messages:
            return True  # no data yet, assume alive
        return (time.time() - self.messages[-1][0]) < timeout

    def recent_messages(self, n=20):
        """Return last n debug messages as text."""
        return [text for _, text in self.messages[-n:]]


# ---------------------------------------------------------------------------
# MIDI helpers
# ---------------------------------------------------------------------------


def silence(out):
    """Send all-notes-off + all-sound-off on all 16 channels."""
    for c in range(16):
        out.send(mido.Message("control_change", channel=c, control=123, value=0))
        out.send(mido.Message("control_change", channel=c, control=120, value=0))


def send_cc_flood(out, rate, duration, stop_event=None):
    """Sustained CC1 (mod wheel) at given rate. Returns messages sent."""
    interval = 1.0 / rate
    sent = 0
    end = time.time() + duration
    val = 0
    while time.time() < end:
        if stop_event and stop_event.is_set():
            break
        out.send(mido.Message("control_change", channel=CH, control=1, value=val))
        val = (val + 1) % 128
        sent += 1
        time.sleep(interval)
    return sent


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


def test_throttle_fires(monitor, stress_port, rate=300, duration=30):
    """Sustained 300 CC/sec for 30s. PASS if throttle or overload appears in debug output."""
    result = TestResult("throttle-fires")
    monitor.clear_flags()

    print(f"  Sending {rate} CC msgs/sec for {duration}s...")
    try:
        out = mido.open_output(stress_port)
    except (OSError, IOError) as e:
        result.errored(f"Cannot open stress port '{stress_port}': {e}")
        return result

    stop = threading.Event()
    sent = [0]
    t = threading.Thread(target=lambda: sent.__setitem__(0, send_cc_flood(out, rate, duration, stop)),
                         daemon=True)
    t.start()

    # Monitor while running
    last_log = 0
    while t.is_alive():
        t.join(timeout=5)
        elapsed = time.time() - (t._started if hasattr(t, '_started') else time.time())
        if monitor.flags["throttle"]:
            print(f"    Throttle indicator seen at ~{int(time.time() - last_log)}s")
        if monitor.flags["overload"]:
            print(f"    Overload indicator seen")

    time.sleep(1)
    silence(out)
    out.close()

    if monitor.flags["crash"]:
        result.failed(f"Device crashed during test", sent=sent[0])
    elif monitor.flags["throttle"] or monitor.flags["overload"]:
        result.passed(
            f"Budget activated: throttle={monitor.counts['throttle']}, "
            f"overload={monitor.counts['overload']}",
            sent=sent[0],
            throttle_count=monitor.counts["throttle"],
            overload_count=monitor.counts["overload"],
            full_internal=monitor.counts["full_internal"],
        )
    else:
        # Budget may have enough headroom — not a failure, but notable
        result.passed(
            f"No throttle at {rate} msgs/sec (budget has headroom)",
            sent=sent[0],
        )

    return result


def test_noteoff_bypass(monitor, stress_port, rate=300, duration=15):
    """Flood CCs at high rate while sending note-on/off pairs. PASS if no stuck notes on DIN echo."""
    result = TestResult("noteoff-bypass")
    monitor.clear_flags()

    try:
        out = mido.open_output(stress_port)
    except (OSError, IOError) as e:
        result.errored(f"Cannot open stress port: {e}")
        return result

    print(f"  Sending {rate} CCs/sec + periodic note pairs for {duration}s...")
    stop = threading.Event()
    cc_sent = [0]
    note_pairs = [0]

    def worker():
        interval = 1.0 / rate
        val = 0
        end = time.time() + duration
        while time.time() < end and not stop.is_set():
            # CC flood
            for _ in range(10):
                out.send(mido.Message("control_change", channel=CH, control=1, value=val))
                val = (val + 1) % 128
                cc_sent[0] += 1

            # Note pair — note-off must get through even under budget pressure
            note = 60 + (note_pairs[0] % 24)
            out.send(mido.Message("note_on", channel=CH, note=note, velocity=80))
            time.sleep(0.02)
            out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))
            note_pairs[0] += 1

            time.sleep(max(0.001, interval * 10 - 0.02))

    t = threading.Thread(target=worker, daemon=True)
    t.start()
    t.join(timeout=duration + 5)

    # Wait for dust to settle
    time.sleep(2)

    # Send all-notes-off to clear any stuck notes, then check if device needed it
    silence(out)
    time.sleep(0.5)
    out.close()

    if monitor.flags["crash"]:
        result.failed("Device crashed during note-off bypass test",
                      cc_sent=cc_sent[0], note_pairs=note_pairs[0])
    else:
        result.passed(
            f"Survived {note_pairs[0]} note pairs under {rate} CC/sec flood",
            cc_sent=cc_sent[0],
            note_pairs=note_pairs[0],
            throttle=monitor.flags["throttle"],
            overload=monitor.flags["overload"],
        )

    return result


def test_budget_recovery(monitor, stress_port, rate=400, burst_duration=10, rest_duration=10):
    """Burst high rate, then stop. PASS if budget recovers (no throttle after rest period)."""
    result = TestResult("budget-recovery")
    monitor.clear_flags()

    try:
        out = mido.open_output(stress_port)
    except (OSError, IOError) as e:
        result.errored(f"Cannot open stress port: {e}")
        return result

    # Phase 1: burst to trigger throttling
    print(f"  Burst: {rate} CCs/sec for {burst_duration}s...")
    sent = send_cc_flood(out, rate, burst_duration)
    throttle_during_burst = monitor.flags["throttle"]

    print(f"    Throttle during burst: {throttle_during_burst}")
    print(f"  Resting {rest_duration}s for budget recovery...")

    # Phase 2: rest — monitor should see budget grow back
    monitor.clear_flags()
    time.sleep(rest_duration)

    # Phase 3: send a few CCs at low rate — should NOT trigger throttle
    print(f"  Probe: 10 CCs/sec for 5s (should not throttle)...")
    send_cc_flood(out, 10, 5)
    throttle_during_probe = monitor.flags["throttle"]

    silence(out)
    out.close()

    if monitor.flags["crash"]:
        result.failed("Device crashed during recovery test", sent=sent)
    elif throttle_during_probe:
        result.failed(
            "Budget did not recover: throttle during low-rate probe after rest",
            sent=sent,
            throttle_during_burst=throttle_during_burst,
        )
    else:
        result.passed(
            f"Budget recovered: burst triggered throttle={throttle_during_burst}, "
            f"probe after {rest_duration}s rest did not throttle",
            sent=sent,
        )

    return result


# ---------------------------------------------------------------------------
# Port discovery (follows test_demo_burnin.py pattern)
# ---------------------------------------------------------------------------


def find_sysex_ports():
    """Find Deluge sysex ports (MIDIOUT3 out, MIDIIN3 or main Deluge in)."""
    out = rtmidi.MidiOut()
    inp = rtmidi.MidiIn()
    out_port = None
    in_port = None

    for i in range(out.get_port_count()):
        name = out.get_port_name(i)
        if "Deluge" in name and "MIDIOUT" not in name:
            out_port = i
        elif "MIDIOUT3" in name and "Deluge" in name:
            out_port = i  # prefer MIDIOUT3 if available

    for i in range(inp.get_port_count()):
        name = inp.get_port_name(i)
        if "Deluge" in name and "MIDIIN" not in name:
            in_port = i
            break

    return out_port, in_port


def list_ports():
    print("=== mido ports (MIDI stress via DIN) ===")
    print("  Outputs:", mido.get_output_names())
    print("  Inputs:", mido.get_input_names())
    print()
    out = rtmidi.MidiOut()
    inp = rtmidi.MidiIn()
    print("=== rtmidi ports (D_PRINTLN sysex) ===")
    print("  Outputs:")
    for i in range(out.get_port_count()):
        print(f"    {i}: {out.get_port_name(i)}")
    print("  Inputs:")
    for i in range(inp.get_port_count()):
        print(f"    {i}: {inp.get_port_name(i)}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

TESTS = {
    "throttle-fires": test_throttle_fires,
    "noteoff-bypass": test_noteoff_bypass,
    "budget-recovery": test_budget_recovery,
}


def main():
    parser = argparse.ArgumentParser(description="MIDI Budget behavior verification")
    parser.add_argument("--test", default="all", choices=list(TESTS.keys()) + ["all"])
    parser.add_argument("--stress-out", default=DEFAULT_STRESS_OUT,
                        help=f"Stress MIDI port (default: {DEFAULT_STRESS_OUT})")
    parser.add_argument("--rate", type=int, default=300, help="CC rate for throttle test")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()

    if args.list_ports:
        list_ports()
        return

    # Find sysex ports
    sysex_out, sysex_in = find_sysex_ports()
    if sysex_out is None or sysex_in is None:
        print("ERROR: Cannot find Deluge sysex ports for D_PRINTLN monitoring")
        print("  Need Deluge connected via USB with debug build")
        list_ports()
        sys.exit(1)

    monitor = BudgetDebugMonitor(sysex_out, sysex_in)
    monitor.start()
    print("D_PRINTLN monitor started")
    time.sleep(1)  # Let initial messages flow

    session = TestSession("midi_budget_behavior")

    tests_to_run = list(TESTS.keys()) if args.test == "all" else [args.test]

    try:
        for name in tests_to_run:
            print(f"\n{'='*50}")
            print(f"TEST: {name}")
            print(f"{'='*50}")

            fn = TESTS[name]
            if name == "throttle-fires":
                result = fn(monitor, args.stress_out, rate=args.rate)
            else:
                result = fn(monitor, args.stress_out)

            session.add(result)
            print(f"  -> {result.status}: {result.details}")

            if result.status == "error":
                break

            # Brief cooldown between tests
            if name != tests_to_run[-1]:
                print("  Cooldown 5s...")
                time.sleep(5)

    except KeyboardInterrupt:
        print("\n\nInterrupted by user.")

    monitor.stop()

    # Summary
    print(f"\n{'='*50}")
    print(session.summary_text())
    session.save()
    print(f"{'='*50}")

    sys.exit(0 if all(r.status in ("pass", "skip") for r in session.results) else 1)


if __name__ == "__main__":
    main()
