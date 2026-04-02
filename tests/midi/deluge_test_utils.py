"""Shared utilities for Deluge hardware integration tests.

Provides MIDI port management, XML clip parsing, and test result tracking.
"""

import glob
import json
import os
import re
import time
from dataclasses import dataclass, field, asdict
from datetime import datetime
from pathlib import Path

import mido

# Default MIDI CC assignments for global commands (must match Deluge MIDI Learn)
CC_PLAY = 102
CC_RECORD = 103
CC_LOOP = 104
CH = 0  # MIDI channel (0-indexed = channel 1)

RESULTS_DIR = Path(__file__).parent / "results"


@dataclass
class TestResult:
    name: str
    status: str = "not_run"  # not_run, pass, fail, skip, error
    details: str = ""
    data: dict = field(default_factory=dict)
    timestamp: str = ""

    def passed(self, details="", **data):
        self.status = "pass"
        self.details = details
        self.data.update(data)
        self.timestamp = datetime.now().isoformat()

    def failed(self, details="", **data):
        self.status = "fail"
        self.details = details
        self.data.update(data)
        self.timestamp = datetime.now().isoformat()

    def skipped(self, details=""):
        self.status = "skip"
        self.details = details
        self.timestamp = datetime.now().isoformat()

    def errored(self, details=""):
        self.status = "error"
        self.details = details
        self.timestamp = datetime.now().isoformat()


class TestSession:
    """Tracks results across multiple tests and writes summary JSON."""

    def __init__(self, session_name):
        self.session_name = session_name
        self.results: list[TestResult] = []
        self.start_time = datetime.now()
        RESULTS_DIR.mkdir(exist_ok=True)

    def add(self, result: TestResult):
        self.results.append(result)
        return result

    def new_test(self, name) -> TestResult:
        r = TestResult(name=name)
        self.results.append(r)
        return r

    def save(self):
        path = RESULTS_DIR / f"{self.session_name}_{self.start_time:%Y%m%d_%H%M%S}.json"
        summary = {
            "session": self.session_name,
            "started": self.start_time.isoformat(),
            "finished": datetime.now().isoformat(),
            "total": len(self.results),
            "passed": sum(1 for r in self.results if r.status == "pass"),
            "failed": sum(1 for r in self.results if r.status == "fail"),
            "skipped": sum(1 for r in self.results if r.status == "skip"),
            "errors": sum(1 for r in self.results if r.status == "error"),
            "results": [asdict(r) for r in self.results],
        }
        path.write_text(json.dumps(summary, indent=2))
        return path

    def summary_text(self):
        p = sum(1 for r in self.results if r.status == "pass")
        f = sum(1 for r in self.results if r.status == "fail")
        s = sum(1 for r in self.results if r.status == "skip")
        e = sum(1 for r in self.results if r.status == "error")
        lines = [f"=== {self.session_name} ===", f"PASS: {p}  FAIL: {f}  SKIP: {s}  ERROR: {e}", ""]
        for r in self.results:
            icon = {"pass": "OK", "fail": "FAIL", "skip": "SKIP", "error": "ERR"}.get(r.status, "?")
            lines.append(f"  [{icon:4s}] {r.name}")
            if r.details:
                lines.append(f"         {r.details}")
        return "\n".join(lines)


def find_deluge_port(direction="output"):
    """Auto-detect a Deluge MIDI port. Returns port name or None."""
    if direction == "output":
        ports = mido.get_output_names()
    else:
        ports = mido.get_input_names()
    for p in ports:
        if "DELUGE" in p.upper() or "Deluge" in p:
            return p
    return None


def list_midi_ports():
    """Print available MIDI ports."""
    print("MIDI Output ports:")
    for p in mido.get_output_names():
        marker = " <-- Deluge?" if "DELUGE" in p.upper() or "Deluge" in p else ""
        print(f"  {p}{marker}")
    print("\nMIDI Input ports:")
    for p in mido.get_input_names():
        marker = " <-- Deluge?" if "DELUGE" in p.upper() or "Deluge" in p else ""
        print(f"  {p}{marker}")


def open_deluge_output(port_name=None):
    """Open MIDI output to Deluge. Auto-detects if port_name is None."""
    if port_name is None:
        port_name = find_deluge_port("output")
    if port_name is None:
        raise RuntimeError("No Deluge MIDI port found. Is it connected via USB?")
    print(f"MIDI output: {port_name}")
    return mido.open_output(port_name)


def open_deluge_input(port_name=None):
    """Open MIDI input from Deluge. Auto-detects if port_name is None."""
    if port_name is None:
        port_name = find_deluge_port("input")
    if port_name is None:
        raise RuntimeError("No Deluge MIDI input port found.")
    print(f"MIDI input: {port_name}")
    return mido.open_input(port_name)


def cc(out, num, val, channel=CH):
    out.send(mido.Message("control_change", channel=channel, control=num, value=val))


def silence(out):
    """All notes off + all sound off on all channels."""
    for c in range(16):
        out.send(mido.Message("control_change", channel=c, control=123, value=0))
        out.send(mido.Message("control_change", channel=c, control=120, value=0))


def note_name(midi_note):
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    return f"{names[midi_note % 12]}{midi_note // 12 - 1}"


# --- XML Parsing ---

def parse_note_data(hex_str):
    """Parse Deluge noteData hex string, return note count.

    Each note entry is 28 hex chars (14 bytes):
      position(4B) + length(4B) + velocity(1B) + flags(5B)
    """
    data = hex_str.strip()
    if data.startswith("0x"):
        data = data[2:]
    if not data:
        return 0
    # Standard entry size is 28 hex chars
    entry_size = 28
    if len(data) % entry_size != 0:
        entry_size = 20
        if len(data) % entry_size != 0:
            return -1
    return len(data) // entry_size


def extract_note_rows(xml_path):
    """Parse a Deluge song XML, return list of (yNote, note_count) for rows with notes."""
    with open(xml_path, "r", encoding="utf-8") as f:
        content = f.read()

    row_pattern = re.compile(
        r'<noteRow[^>]*?'
        r'(?:yNote="(-?\d+)"[^>]*?)?'
        r'(?:noteData(?:WithLift)?="(0x[0-9A-Fa-f]+)")',
        re.DOTALL,
    )

    results = []
    for match in row_pattern.finditer(content):
        y_note = int(match.group(1)) if match.group(1) else None
        hex_data = match.group(2)
        count = parse_note_data(hex_data)
        if count > 0:
            results.append((y_note, count))
    return results


def find_newest_xml(path, after_time):
    """Find newest XML in path modified after after_time."""
    candidates = []
    for f in glob.glob(os.path.join(path, "*.XML")):
        if os.path.getmtime(f) > after_time:
            candidates.append(f)
    if not candidates:
        return None
    return max(candidates, key=os.path.getmtime)


def wait_for_new_xml(path, after_time, timeout=120):
    """Wait for a new XML file to appear. Returns path or None."""
    print(f"Watching {path} for new XML file (timeout {timeout}s)...")
    for i in range(timeout):
        xml = find_newest_xml(path, after_time)
        if xml:
            time.sleep(1)  # let write finish
            print(f"Found: {os.path.basename(xml)}")
            return xml
        time.sleep(1)
        if i > 0 and i % 15 == 0:
            print(f"  ...still waiting ({i}s)")
    print("Timeout — no new XML file detected.")
    return None
