#!/usr/bin/env python3
"""Test for duplicate notes during linear recording with USB MIDI Follow.

Reproduces upstream issue SynthstromAudible/DelugeFirmware#4225:
When linear recording via USB MIDI with MIDI Follow, notes get recorded
as cluster chords instead of single notes.

SETUP (one-time on Deluge):
  1. SETTINGS > MIDI > CMD > PLAY  -> learn to CC#102 ch1
  2. SETTINGS > MIDI > CMD > RECORD -> learn to CC#103 ch1
  3. SETTINGS > MIDI > CMD > LOOP   -> learn to CC#104 ch1
  4. Enable MIDI Follow (channel A = 1)
  5. Create a new empty song with one synth clip
  6. Connect Deluge via USB to this computer

USAGE:
  python test_linear_record_duplicate_notes.py [--save-path M:/SONGS]

The test will:
  1. Start playback via CC#102
  2. Start linear recording via CC#103
  3. Send 10 notes with clear spacing
  4. Stop recording and playback
  5. Prompt to save song, then parse XML to verify note counts

If --save-path is given and a new .XML file appears, it will
automatically parse and verify. Otherwise it prints manual steps.
"""

import argparse
import glob
import os
import re
import sys
import time

import mido

# MIDI config - must match Deluge MIDI Learn setup
CH = 0  # MIDI channel (0-indexed = channel 1)
CC_PLAY = 102
CC_RECORD = 103
CC_LOOP = 104

# Test notes - spread across range to land on separate noteRows
TEST_NOTES = [60, 62, 64, 65, 67, 69, 71, 72, 74, 76]
NOTE_SPACING_S = 0.4  # seconds between notes (well above quantize threshold)
NOTE_DURATION_S = 0.15  # how long each note is held


def cc(out, num, val):
    out.send(mido.Message("control_change", channel=CH, control=num, value=val))


def silence(out):
    """All notes off + all sound off on all channels."""
    for c in range(16):
        out.send(mido.Message("control_change", channel=c, control=123, value=0))
        out.send(mido.Message("control_change", channel=c, control=120, value=0))


def send_test_notes(out, notes, spacing, duration):
    """Send a sequence of notes with controlled timing. Returns count sent."""
    sent = 0
    for note in notes:
        out.send(mido.Message("note_on", channel=CH, note=note, velocity=80))
        time.sleep(duration)
        out.send(mido.Message("note_off", channel=CH, note=note, velocity=0))
        sent += 1
        print(f"  note {sent}/{len(notes)}: {note_name(note)} (MIDI {note})")
        time.sleep(spacing - duration)
    return sent


def note_name(midi_note):
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    return f"{names[midi_note % 12]}{midi_note // 12 - 1}"


def parse_note_data(hex_str):
    """Parse Deluge noteData/noteDataWithLift hex string into note count.

    Each note is encoded as: position(4B) + length(4B) + velocity(1B) + flags(5B) = 14 bytes = 28 hex chars.
    In noteDataWithLift format: position(4B) + length(4B) + velocity(1B) + lift(1B) + flags(4B) = 14 bytes.
    """
    # Strip "0x" prefix if present
    data = hex_str.strip()
    if data.startswith("0x"):
        data = data[2:]

    if not data:
        return 0

    # Each note entry is 28 hex chars (14 bytes) in noteData,
    # or 28 hex chars in noteDataWithLift (same total size, different field layout)
    entry_size = 28
    if len(data) % entry_size != 0:
        # Try 20 hex chars (10 bytes) for older format
        entry_size = 20
        if len(data) % entry_size != 0:
            print(f"  WARNING: hex data length {len(data)} not divisible by {entry_size}")
            return -1

    return len(data) // entry_size


def find_newest_xml(path, after_time):
    """Find the newest XML file in path created after after_time."""
    pattern = os.path.join(path, "*.XML")
    candidates = []
    for f in glob.glob(pattern):
        if os.path.getmtime(f) > after_time:
            candidates.append(f)
    if not candidates:
        return None
    return max(candidates, key=os.path.getmtime)


def verify_song_xml(xml_path, expected_notes):
    """Parse a Deluge song XML and check for duplicate notes in noteRows.

    Returns (passed, details) where details is a list of (noteRow_y, note_count) tuples.
    """
    print(f"\nParsing: {xml_path}")

    with open(xml_path, "r", encoding="utf-8") as f:
        content = f.read()

    # Find all noteRows with their yNote (MIDI pitch) and note data
    # Pattern: <noteRow ... yNote="N" ... noteData="0x..." or noteDataWithLift="0x..."
    row_pattern = re.compile(
        r'<noteRow[^>]*?'
        r'(?:yNote="(-?\d+)"[^>]*?)?'
        r'(?:noteData(?:WithLift)?="(0x[0-9A-Fa-f]+)")',
        re.DOTALL
    )

    results = []
    duplicates_found = False

    for match in row_pattern.finditer(content):
        y_note = int(match.group(1)) if match.group(1) else None
        hex_data = match.group(2)
        count = parse_note_data(hex_data)

        if y_note is not None and y_note in [n for n in expected_notes]:
            results.append((y_note, count))
            status = "OK" if count == 1 else f"DUPLICATE ({count} notes!)"
            print(f"  noteRow y={y_note} ({note_name(y_note)}): {count} note(s) - {status}")
            if count > 1:
                duplicates_found = True

    if not results:
        print("  WARNING: No matching noteRows found for test notes")
        print("  (Song may not contain the recorded clip, or format differs)")
        return False, results

    passed = not duplicates_found
    return passed, results


def run_test(out_port_name, save_path=None):
    print(f"Opening MIDI output: {out_port_name}")
    out = mido.open_output(out_port_name)

    try:
        silence(out)
        time.sleep(0.5)

        # Record timestamp before we start (for finding new XML files)
        start_time = time.time()

        # 1. Start playback
        print("\n--- Starting playback (CC#102) ---")
        cc(out, CC_PLAY, 127)
        time.sleep(1.0)

        # 2. Start recording (linear record on empty clip)
        print("--- Starting linear recording (CC#103) ---")
        print("    (Deluge should show recording indicator)")
        cc(out, CC_RECORD, 127)
        time.sleep(1.0)

        # 3. Send test notes
        print(f"\n--- Sending {len(TEST_NOTES)} notes ---")
        count = send_test_notes(out, TEST_NOTES, NOTE_SPACING_S, NOTE_DURATION_S)
        time.sleep(0.5)

        # 4. Stop recording
        print("\n--- Stopping recording (CC#103) ---")
        cc(out, CC_RECORD, 127)
        time.sleep(0.5)

        # 5. Stop playback
        print("--- Stopping playback (CC#102) ---")
        cc(out, CC_PLAY, 127)
        time.sleep(0.5)

        silence(out)

        print(f"\n=== Sent {count} notes, each should appear exactly once ===")

        # 6. Verification
        if save_path:
            print(f"\n--- Save the song now (on Deluge: SAVE + select encoder) ---")
            print(f"    Watching {save_path} for new XML file...")
            print(f"    (Press Ctrl+C to skip and check manually)")

            try:
                for _ in range(60):  # wait up to 60 seconds
                    xml = find_newest_xml(save_path, start_time)
                    if xml:
                        time.sleep(1)  # let file finish writing
                        passed, results = verify_song_xml(xml, TEST_NOTES)
                        print()
                        if passed:
                            print("PASS: All notes recorded exactly once")
                        else:
                            print("FAIL: Duplicate notes detected!")
                            print("      This confirms upstream issue #4225")
                        return passed
                    time.sleep(1)
                print("\nTimeout waiting for save. Check manually.")
            except KeyboardInterrupt:
                print("\nSkipped auto-verify.")
        else:
            print("\nTo verify:")
            print("  1. Save the song on the Deluge")
            print("  2. Run: python test_linear_record_duplicate_notes.py --verify <path-to-song.XML>")
            print(f"  Expected: {count} noteRows with exactly 1 note each")
            print(f"  Bug present if: any noteRow has 2+ notes (cluster chord)")

    finally:
        silence(out)
        out.close()

    return None


def main():
    parser = argparse.ArgumentParser(description="Test linear recording for duplicate notes")
    parser.add_argument("--port", default="Deluge Port 1",
                        help="MIDI output port name (default: Deluge Port 1)")
    parser.add_argument("--save-path", default=None,
                        help="Path to Deluge SONGS folder (e.g. M:/SONGS) for auto-verify")
    parser.add_argument("--verify", default=None,
                        help="Verify an existing song XML file instead of running the test")
    parser.add_argument("--list-ports", action="store_true",
                        help="List available MIDI ports and exit")
    args = parser.parse_args()

    if args.list_ports:
        print("Available MIDI output ports:")
        for name in mido.get_output_names():
            print(f"  {name}")
        print("\nAvailable MIDI input ports:")
        for name in mido.get_input_names():
            print(f"  {name}")
        return

    if args.verify:
        passed, _ = verify_song_xml(args.verify, TEST_NOTES)
        if passed:
            print("\nPASS: All notes recorded exactly once")
        else:
            print("\nFAIL: Duplicate notes detected!")
        sys.exit(0 if passed else 1)

    result = run_test(args.port, args.save_path)
    if result is not None:
        sys.exit(0 if result else 1)


if __name__ == "__main__":
    main()
