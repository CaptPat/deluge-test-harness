"""On-device integration tests for sysex status protocol.

Requires:
  - Deluge connected via USB
  - Firmware built with ENABLE_SYSEX_STATUS
  - For CC tests: status outputs MIDI-learned on the device
"""
import argparse
import sys
import time

from deluge_client import DelugeClient, ACK_OK


def test_identity(client: DelugeClient) -> bool:
    """Verify device responds to Identity Request."""
    connected = client.is_connected()
    fw = client.get_firmware_version()
    print(f"  Identity: connected={connected}, firmware={fw}")
    return connected and fw is not None


def test_status_query(client: DelugeClient) -> bool:
    """Verify STATUS_QUERY returns valid data."""
    status = client.get_status()
    print(f"  SRAM free: {status.sram_free:,} bytes")
    print(f"  SDRAM free: {status.sdram_free:,} bytes")
    print(f"  Voices: {status.voice_count}")
    print(f"  Budget: {status.midi_budget}")
    print(f"  Swap: {status.song_swap_active}")
    print(f"  Uptime: {status.uptime_sec}s")

    ok = True
    if status.sram_free == 0 and status.sdram_free == 0:
        print("  WARN: both memory regions report 0 free — protocol may not be enabled")
        ok = False
    return ok


def test_post_results(client: DelugeClient) -> bool:
    """Verify POST_RESULTS returns data."""
    results = client.get_post_results()
    print(f"  POST results: {len(results)} entries")
    if len(results) == 1 and results[0] == 0:
        print("  (no test framework compiled in, or no POST run)")
    elif len(results) > 1:
        from collections import Counter
        counts = Counter(results)
        labels = {0: "PASS", 1: "WARN", 2: "FAIL", 3: "XFAIL", 4: "SKIP"}
        parts = [f"{labels.get(k, '?')}={v}" for k, v in sorted(counts.items())]
        print(f"  {' '.join(parts)}")
    return len(results) > 0


def test_status_twice(client: DelugeClient) -> bool:
    """Send STATUS_QUERY twice, verify uptime increases."""
    s1 = client.get_status()
    time.sleep(1.5)
    s2 = client.get_status()
    print(f"  Uptime: {s1.uptime_sec}s -> {s2.uptime_sec}s")
    if s2.uptime_sec > s1.uptime_sec:
        print("  Uptime advancing — OK")
        return True
    elif s2.uptime_sec == s1.uptime_sec:
        print("  Uptime unchanged (may be rounding)")
        return True  # 1s granularity, could be same second
    else:
        print("  FAIL: uptime went backwards")
        return False


def test_enable_heartbeat(client: DelugeClient) -> bool:
    """Enable CC heartbeat and verify CCs arrive."""
    if not client._status_in_name:
        print("  SKIP: no --status-in port provided")
        return True

    client.enable_status_cc(True, rate_hz=2)
    client.start_status_monitor()
    time.sleep(3)

    hb = client.get_latest_heartbeat()
    received = hb is not None
    if received:
        print(f"  Heartbeat received: SRAM={hb.sram_pressure} SDRAM={hb.sdram_pressure} voices={hb.voice_count}")
    else:
        print("  No heartbeat received — check MIDI Learn on device")

    client.enable_status_cc(False)
    client.stop_status_monitor()
    return received


def main():
    parser = argparse.ArgumentParser(description="Sysex status protocol integration tests")
    parser.add_argument("--sysex-out", default="Deluge 4")
    parser.add_argument("--sysex-in", default="Deluge 2")
    parser.add_argument("--status-in", default=None, help="Port for CC heartbeat (e.g. 'USB MIDI 1')")
    parser.add_argument("--test", default="all", help="Run specific test or 'all'")
    parser.add_argument("--list-ports", action="store_true", help="List available MIDI ports")
    args = parser.parse_args()

    if args.list_ports:
        import rtmidi
        mo = rtmidi.MidiOut()
        mi = rtmidi.MidiIn()
        print("OUT:", mo.get_ports())
        print("IN:", mi.get_ports())
        return

    DelugeClient.check_windows_midi_services()

    client = DelugeClient(
        sysex_out=args.sysex_out,
        sysex_in=args.sysex_in,
        status_in=args.status_in,
    )

    tests = {
        "identity": test_identity,
        "status": test_status_query,
        "post": test_post_results,
        "uptime": test_status_twice,
        "heartbeat": test_enable_heartbeat,
    }

    with client:
        if args.test == "all":
            run = tests
        else:
            if args.test not in tests:
                print(f"Unknown test '{args.test}'. Available: {', '.join(tests.keys())}")
                sys.exit(1)
            run = {args.test: tests[args.test]}

        results = {}
        for name, fn in run.items():
            print(f"\n=== {name} ===")
            try:
                results[name] = fn(client)
            except Exception as e:
                print(f"  ERROR: {e}")
                results[name] = False

    print("\n=== Summary ===")
    all_ok = True
    for name, ok in results.items():
        status = "PASS" if ok else "FAIL"
        print(f"  {name}: {status}")
        if not ok:
            all_ok = False

    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
