#!/usr/bin/env python3
"""DMA coherency verification via USB MIDI loopback.

Captures the 128-CC sequence sent by the Deluge's test_midi_dma_loopback
self-test and validates that every CC number matches its value.

This verifies that the USB MIDI DMA path produces correct data with
L2 D-cache unlocked.

Usage:
    1. Boot Deluge with RECORD held (self-test mode)
    2. Wait for self-test to complete (tests run automatically)
    3. Run: python test_dma_loopback.py

The script waits up to 10 seconds for the 128-CC sequence.

Requirements:
    - Deluge connected via USB (MIDI device visible)
    - mido + python-rtmidi installed
"""

import sys
import time

import mido

from deluge_test_utils import (
    TestSession,
    find_deluge_port,
    cc,
)

EXPECTED_CCS = 128
CHANNEL = 0  # MIDI channel 1 (0-indexed)
TIMEOUT = 10.0  # seconds


def test_deluge_tx(midi_in):
    """Capture 128 CCs from Deluge and validate CC_number == value."""
    session = TestSession("dma_loopback")
    result = session.new_test("deluge_tx_128cc")

    print(f"Listening for {EXPECTED_CCS} CCs on {midi_in.name}...")
    print("(Boot Deluge with RECORD held, wait for self-test to run midi_dma test)")

    received = {}
    start = time.time()

    while len(received) < EXPECTED_CCS and (time.time() - start) < TIMEOUT:
        msg = midi_in.poll()
        if msg is None:
            time.sleep(0.001)
            continue
        if msg.type == "control_change" and msg.channel == CHANNEL:
            received[msg.control] = msg.value

    if len(received) < EXPECTED_CCS:
        result.failed(
            f"Timeout: received {len(received)}/{EXPECTED_CCS} CCs in {TIMEOUT}s",
            received_count=len(received),
        )
    else:
        # Validate: CC number should equal value
        mismatches = []
        for cc_num in range(EXPECTED_CCS):
            if cc_num not in received:
                mismatches.append(f"CC{cc_num}: missing")
            elif received[cc_num] != cc_num:
                mismatches.append(f"CC{cc_num}: got {received[cc_num]}, expected {cc_num}")

        if mismatches:
            result.failed(
                f"{len(mismatches)} mismatches: {'; '.join(mismatches[:5])}",
                mismatch_count=len(mismatches),
            )
        else:
            result.passed(
                f"All {EXPECTED_CCS} CCs verified (CC_num == value)",
                received_count=len(received),
            )

    return session, result


def test_roundtrip(midi_in, midi_out):
    """Send 128 CCs to Deluge, capture any thru/echo back."""
    session = TestSession("dma_roundtrip")
    result = session.new_test("pc_to_deluge_128cc")

    # Drain any pending messages
    while midi_in.poll() is not None:
        pass

    print(f"Sending {EXPECTED_CCS} CCs to Deluge...")
    for cc_num in range(EXPECTED_CCS):
        cc(midi_out, cc_num, cc_num, channel=CHANNEL)
        time.sleep(0.005)

    # Wait for echoes (MIDI thru)
    print("Waiting for MIDI thru responses...")
    received = {}
    start = time.time()
    while len(received) < EXPECTED_CCS and (time.time() - start) < 3.0:
        msg = midi_in.poll()
        if msg is None:
            time.sleep(0.001)
            continue
        if msg.type == "control_change" and msg.channel == CHANNEL:
            received[msg.control] = msg.value

    if len(received) == 0:
        result.skipped("No MIDI thru received (thru may not be configured)")
    elif len(received) < EXPECTED_CCS:
        result.failed(
            f"Partial thru: {len(received)}/{EXPECTED_CCS}",
            received_count=len(received),
        )
    else:
        mismatches = []
        for cc_num in range(EXPECTED_CCS):
            if cc_num in received and received[cc_num] != cc_num:
                mismatches.append(f"CC{cc_num}: got {received[cc_num]}")
        if mismatches:
            result.failed(f"{len(mismatches)} mismatches")
        else:
            result.passed(f"Round-trip {len(received)} CCs verified")

    return session, result


def main():
    port_name = find_deluge_port("input")
    if port_name is None:
        print("ERROR: No Deluge USB MIDI port found. Is it connected?")
        sys.exit(1)

    print(f"Found Deluge MIDI: {port_name}")

    # Test 1: Capture Deluge TX (from self-test)
    with mido.open_input(port_name) as midi_in:
        session1, result1 = test_deluge_tx(midi_in)

    print()
    print(f"TX test: {result1.status} — {result1.details}")

    # Test 2: Round-trip (if output port available)
    out_port = find_deluge_port("output")
    if out_port:
        with mido.open_input(port_name) as midi_in, mido.open_output(out_port) as midi_out:
            session2, result2 = test_roundtrip(midi_in, midi_out)
        print(f"Round-trip: {result2.status} — {result2.details}")

        # Merge sessions
        session1.results.extend(session2.results)

    # Save results
    path = session1.save()
    print(f"\nResults saved to {path}")
    print(session1.summary_text())

    # Exit code
    failed = any(r.status == "fail" for r in session1.results)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
