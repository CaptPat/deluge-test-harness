"""DelugeClient — remote control of the Synthstrom Deluge over USB MIDI sysex.

Implements the ENABLE_SYSEX_STATUS protocol defined in sysex_status.h:
  - STATUS_QUERY (0x10) — 16-byte packed status struct
  - POST_RESULTS (0x11) — test results array
  - START_DEMO   (0x12) — initiate demo mode
  - LOAD_SONG    (0x13) — load song by filename
  - SAVE_SONG    (0x14) — save song by filename
  - ENABLE_STATUS_CC (0x15) — enable/disable CC heartbeat

Sysex format: F0 00 21 7B 01 06 [subcommand] [payload...] F7
  Manufacturer: 00 21 7B (Synthstrom)
  Device family: 01
  Command: 06 (SysexCommands::Status)

The STATUS_QUERY payload is 7-bit packed (Sequential/DSI "Packed Data Format"):
every 7 source bytes become 8 bytes in the sysex stream — a header byte
whose bits are the MSBs of the following 7 data bytes, then the 7 low-7-bit
data bytes.  See util/pack.c in the firmware source.

Usage example::

    with DelugeClient() as d:
        if not d.is_connected():
            raise RuntimeError("Deluge not found")
        status = d.get_status()
        print(status.uptime_sec, status.sram_free)

"""

from __future__ import annotations

import struct
import subprocess
import threading
import time
import warnings
from dataclasses import dataclass, field
from typing import Callable

import mido

# ---------------------------------------------------------------------------
# Sysex protocol constants (mirrors sysex_status.h and sysex.h)
# ---------------------------------------------------------------------------

_SYSEX_HEADER = [0x00, 0x21, 0x7B, 0x01]  # Synthstrom manufacturer ID + family
_STATUS_CMD = 0x06  # SysexCommands::Status

# Subcommands
_STATUS_QUERY = 0x10
_POST_RESULTS = 0x11
_START_DEMO = 0x12
_LOAD_SONG = 0x13
_SAVE_SONG = 0x14
_ENABLE_STATUS_CC = 0x15
_SET_MIDI_LEARN = 0x16

# ACK codes
ACK_OK = 0x00
ACK_BUSY = 0x01
ACK_ERROR = 0x02

# System mode values reported in STATUS_MODE CC
MODE_NORMAL = 0
MODE_DEMO = 1
MODE_SELF_TEST = 2
MODE_LOADING = 3
MODE_SAVING = 4

# MIDI Universal Identity Request (F0 7E 7F 06 01 F7)
_IDENTITY_REQUEST = [0x7E, 0x7F, 0x06, 0x01]

# Default CC transport assignments (must match Deluge MIDI Learn)
_DEFAULT_TRANSPORT = {
    "play": (0, 102),    # (channel, cc_number)
    "record": (0, 103),
    "loop": (0, 104),
}

# Default sysex port names for Windows with Deluge connected via USB
_DEFAULT_SYSEX_OUT = "Deluge 4"
_DEFAULT_SYSEX_IN = "Deluge 2"
_DEFAULT_STATUS_IN = "USB MIDI 1"   # DIN input via Cable Matters adapter


# ---------------------------------------------------------------------------
# 7-bit pack / unpack — exact Python port of firmware util/pack.c
# ---------------------------------------------------------------------------

def _pack_8bit_to_7bit(src: bytes) -> bytes:
    """Pack 8-bit bytes into 7-bit sysex-safe stream.

    Matches firmware pack_8bit_to_7bit().  Every 7 source bytes become
    8 output bytes: a header byte whose bit-N is the MSB of source byte N,
    followed by the 7 low-7-bit source bytes.  Incomplete final packets are
    supported.
    """
    src_len = len(src)
    packets = (src_len + 6) // 7
    missing = 7 * packets - src_len
    out_len = 8 * packets - missing

    dst = bytearray(out_len)
    for i in range(packets):
        ipos = 7 * i
        opos = 8 * i
        dst[opos] = 0
        for j in range(7):
            if ipos + j >= src_len:
                break
            dst[opos + 1 + j] = src[ipos + j] & 0x7F
            if src[ipos + j] & 0x80:
                dst[opos] |= (1 << j)
    return bytes(dst)


def _unpack_7bit_to_8bit(packed: list[int] | bytes, out_len: int) -> bytes:
    """Unpack 7-bit sysex-safe data back to 8-bit bytes.

    Matches firmware unpack_7bit_to_8bit().  The inverse of _pack_8bit_to_7bit.
    Each group of 8 packed bytes yields up to 7 output bytes; the first byte
    of each group carries the MSBs.
    """
    src_len = len(packed)
    packets = (src_len + 7) // 8
    missing = 8 * packets - src_len
    if missing == 7:
        packets -= 1
        missing = 0
    actual_out = 7 * packets - missing
    actual_out = min(actual_out, out_len)

    dst = bytearray(out_len)
    for i in range(packets):
        ipos = 8 * i
        opos = 7 * i
        rot_bit = 1
        high_bits = packed[ipos]
        for j in range(7):
            if j + 1 + ipos >= src_len:
                break
            if opos + j >= out_len:
                break
            dst[opos + j] = packed[ipos + 1 + j] & 0x7F
            if high_bits & rot_bit:
                dst[opos + j] |= 0x80
            rot_bit <<= 1
    return bytes(dst)


# ---------------------------------------------------------------------------
# Sysex message builders
# ---------------------------------------------------------------------------

def _build_status_sysex(subcommand: int, payload: bytes = b"") -> list[int]:
    """Build a Deluge status sysex message as a flat list of ints (for mido)."""
    body = _SYSEX_HEADER + [_STATUS_CMD, subcommand] + list(payload)
    return body  # mido.Message('sysex') wraps with F0 / F7 automatically


def _is_status_reply(msg: mido.Message, subcommand: int) -> bool:
    """Return True if msg is a sysex reply matching the given status subcommand."""
    if msg.type != "sysex":
        return False
    d = msg.data
    if len(d) < 6:
        return False
    return (
        d[0] == 0x00 and d[1] == 0x21 and d[2] == 0x7B and d[3] == 0x01
        and d[4] == _STATUS_CMD and d[5] == subcommand
    )


def _is_identity_reply(msg: mido.Message) -> bool:
    """Return True if msg is a Universal Identity Reply (F0 7E ... 06 02 ... F7)."""
    if msg.type != "sysex":
        return False
    d = msg.data
    return len(d) >= 5 and d[0] == 0x7E and d[2] == 0x06 and d[3] == 0x02


# ---------------------------------------------------------------------------
# StatusResponse dataclass
# ---------------------------------------------------------------------------

@dataclass
class StatusResponse:
    """Decoded STATUS_QUERY response.

    Fields match the 16-byte raw struct packed by handleStatusQuery()
    in sysex_status.cpp:

      bytes 0-3:  sram_free      — internal SRAM free bytes (uint32 LE)
      bytes 4-7:  sdram_free     — external SDRAM free bytes (uint32 LE)
      bytes 8-9:  voice_count    — active voices (uint16 LE, placeholder)
      byte  10:   midi_budget    — MIDI budget state (placeholder)
      byte  11:   song_swap      — 1 if song swap in progress
      bytes 12-15: uptime_sec    — uptime in seconds (uint32 LE)
    """

    sram_free: int = 0
    sdram_free: int = 0
    voice_count: int = 0
    midi_budget: int = 0
    song_swap_active: bool = False
    uptime_sec: int = 0

    @classmethod
    def from_packed(cls, payload: list[int]) -> "StatusResponse":
        """Parse from the 7-bit packed payload (bytes after the subcommand byte)."""
        raw = _unpack_7bit_to_8bit(payload, 16)
        if len(raw) < 16:
            return cls()
        sram_free, sdram_free = struct.unpack_from("<II", raw, 0)
        voice_count = struct.unpack_from("<H", raw, 8)[0]
        midi_budget = raw[10]
        song_swap = bool(raw[11])
        uptime_sec = struct.unpack_from("<I", raw, 12)[0]
        return cls(
            sram_free=sram_free,
            sdram_free=sdram_free,
            voice_count=voice_count,
            midi_budget=midi_budget,
            song_swap_active=song_swap,
            uptime_sec=uptime_sec,
        )

    def __str__(self) -> str:
        swap = " [SWAPPING]" if self.song_swap_active else ""
        return (
            f"SRAM free: {self.sram_free // 1024} kB  "
            f"SDRAM free: {self.sdram_free // 1024} kB  "
            f"voices: {self.voice_count}  "
            f"uptime: {self.uptime_sec}s{swap}"
        )


# ---------------------------------------------------------------------------
# StatusMonitor — background thread for CC heartbeat
# ---------------------------------------------------------------------------

@dataclass
class CCHeartbeat:
    """One CC heartbeat frame received from the Deluge."""

    sram_pressure: int = 0    # 0-127
    sdram_pressure: int = 0   # 0-127
    voice_count: int = 0
    midi_budget: int = 0
    song_swap: int = 0
    mode: int = MODE_NORMAL
    timestamp: float = field(default_factory=time.monotonic)

    @property
    def mode_name(self) -> str:
        names = {
            MODE_NORMAL: "normal",
            MODE_DEMO: "demo",
            MODE_SELF_TEST: "self_test",
            MODE_LOADING: "loading",
            MODE_SAVING: "saving",
        }
        return names.get(self.mode, f"unknown({self.mode})")


class _StatusMonitor:
    """Background thread that reads CC heartbeat messages from the Deluge.

    The firmware sends up to 6 CCs per heartbeat tick on the DIN port:
      STATUS_SRAM, STATUS_SDRAM, STATUS_VOICES, STATUS_BUDGET,
      STATUS_SWAP, STATUS_MODE.

    The CC numbers are whatever the user has MIDI-learned on the Deluge.
    Pass a cc_config dict mapping field names to (channel, cc_number).
    """

    def __init__(
        self,
        port_name: str,
        cc_config: dict[str, tuple[int, int]],
        on_heartbeat: Callable[[CCHeartbeat], None] | None = None,
    ) -> None:
        self._port_name = port_name
        self._cc_config = cc_config  # {"sram": (ch, cc), ...}
        self._on_heartbeat = on_heartbeat
        self._thread: threading.Thread | None = None
        self._stop_event = threading.Event()
        self._latest: CCHeartbeat | None = None
        self._lock = threading.Lock()

        # Invert config for fast lookup: (channel, cc) -> field name
        self._cc_map: dict[tuple[int, int], str] = {
            v: k for k, v in cc_config.items() if v is not None
        }

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._run, daemon=True, name="DelugeStatusMonitor")
        self._thread.start()

    def stop(self, timeout: float = 2.0) -> None:
        self._stop_event.set()
        if self._thread:
            self._thread.join(timeout=timeout)
            self._thread = None

    @property
    def latest(self) -> CCHeartbeat | None:
        with self._lock:
            return self._latest

    def _run(self) -> None:
        try:
            port = mido.open_input(self._port_name)
        except Exception as exc:
            warnings.warn(f"StatusMonitor: could not open {self._port_name!r}: {exc}")
            return

        pending = CCHeartbeat()
        last_field: str | None = None

        try:
            while not self._stop_event.is_set():
                for msg in port.iter_pending():
                    if msg.type != "control_change":
                        continue
                    key = (msg.channel, msg.control)
                    field_name = self._cc_map.get(key)
                    if field_name is None:
                        continue

                    # Detect heartbeat boundary: if we see a field we already
                    # received in this frame, flush the pending frame first.
                    if last_field is not None and field_name == list(self._cc_map.values())[0]:
                        self._flush(pending)
                        pending = CCHeartbeat()

                    if field_name == "sram":
                        pending.sram_pressure = msg.value
                    elif field_name == "sdram":
                        pending.sdram_pressure = msg.value
                    elif field_name == "voices":
                        pending.voice_count = msg.value
                    elif field_name == "budget":
                        pending.midi_budget = msg.value
                    elif field_name == "swap":
                        pending.song_swap = msg.value
                    elif field_name == "mode":
                        pending.mode = msg.value
                        # mode is always the last field — flush on receipt
                        self._flush(pending)
                        pending = CCHeartbeat()

                    last_field = field_name

                time.sleep(0.005)
        finally:
            port.close()

    def _flush(self, hb: CCHeartbeat) -> None:
        hb.timestamp = time.monotonic()
        with self._lock:
            self._latest = hb
        if self._on_heartbeat:
            try:
                self._on_heartbeat(hb)
            except Exception:
                pass


# ---------------------------------------------------------------------------
# DelugeClient
# ---------------------------------------------------------------------------

class DelugeClient:
    """Remote-control interface for the Synthstrom Deluge over USB MIDI.

    Provides sysex-based status queries and remote commands (ENABLE_SYSEX_STATUS
    firmware build), plus CC transport control (MIDI Learn assignments).

    Args:
        sysex_out: Name of the MIDI output port for sending sysex to the Deluge.
        sysex_in:  Name of the MIDI input port for receiving sysex replies.
        status_in: Name of the MIDI input port for CC heartbeat (DIN).
        transport_config: Dict mapping command names to (channel, cc) tuples.
            Recognised keys: "play", "record", "loop", "undo", "fill".
        cc_config: Dict mapping status field names to (channel, cc) tuples
            for the CC heartbeat monitor.
            Recognised keys: "sram", "sdram", "voices", "budget", "swap", "mode".
        timeout: Default timeout in seconds for sysex round-trips.

    Example::

        with DelugeClient() as d:
            print(d.get_firmware_version())
            print(d.get_status())
            d.start_status_monitor()
            d.play()
    """

    def __init__(
        self,
        sysex_out: str = _DEFAULT_SYSEX_OUT,
        sysex_in: str = _DEFAULT_SYSEX_IN,
        status_in: str = _DEFAULT_STATUS_IN,
        transport_config: dict[str, tuple[int, int]] | None = None,
        cc_config: dict[str, tuple[int, int]] | None = None,
        timeout: float = 3.0,
    ) -> None:
        self._sysex_out_name = sysex_out
        self._sysex_in_name = sysex_in
        self._status_in_name = status_in
        self._transport = transport_config or dict(_DEFAULT_TRANSPORT)
        self._cc_config = cc_config or {}
        self._timeout = timeout

        self._out: mido.ports.BaseOutput | None = None
        self._in: mido.ports.BaseInput | None = None
        self._monitor: _StatusMonitor | None = None

    # ------------------------------------------------------------------
    # Context manager
    # ------------------------------------------------------------------

    def __enter__(self) -> "DelugeClient":
        self.open()
        return self

    def __exit__(self, *_) -> None:
        self.close()

    # ------------------------------------------------------------------
    # Port lifecycle
    # ------------------------------------------------------------------

    def open(self) -> None:
        """Open MIDI ports.  Called automatically by __enter__."""
        if self._out is None:
            self._out = mido.open_output(self._sysex_out_name)
        if self._in is None:
            self._in = mido.open_input(self._sysex_in_name)
            self._in.iter_pending()  # flush stale messages

    def close(self) -> None:
        """Close all MIDI ports and stop the status monitor."""
        self.stop_status_monitor()
        if self._in is not None:
            self._in.close()
            self._in = None
        if self._out is not None:
            self._out.close()
            self._out = None

    def _ensure_open(self) -> None:
        if self._out is None or self._in is None:
            self.open()

    # ------------------------------------------------------------------
    # Low-level sysex helpers
    # ------------------------------------------------------------------

    def _send(self, data: list[int]) -> None:
        """Send a sysex message.  data should not include F0/F7."""
        self._ensure_open()
        self._out.send(mido.Message("sysex", data=data))  # type: ignore[union-attr]

    def _recv_sysex(
        self,
        predicate: Callable[[mido.Message], bool],
        timeout: float | None = None,
    ) -> mido.Message | None:
        """Poll the input port until a matching sysex arrives or timeout."""
        self._ensure_open()
        deadline = time.monotonic() + (timeout if timeout is not None else self._timeout)
        while time.monotonic() < deadline:
            for msg in self._in.iter_pending():  # type: ignore[union-attr]
                if predicate(msg):
                    return msg
            time.sleep(0.005)
        return None

    def _send_and_recv(
        self,
        data: list[int],
        predicate: Callable[[mido.Message], bool],
        timeout: float | None = None,
    ) -> mido.Message | None:
        """Flush stale input, send sysex, wait for a matching reply."""
        self._ensure_open()
        # Drain any pending stale messages before sending
        list(self._in.iter_pending())  # type: ignore[union-attr]
        self._send(data)
        return self._recv_sysex(predicate, timeout=timeout)

    # ------------------------------------------------------------------
    # Identity / connection
    # ------------------------------------------------------------------

    def is_connected(self, timeout: float = 1.5) -> bool:
        """Send a MIDI Universal Identity Request and return True if the Deluge replies.

        The Identity Request works regardless of the ENABLE_SYSEX_STATUS compile flag.
        """
        try:
            self._ensure_open()
        except Exception:
            return False
        reply = self._send_and_recv(
            _IDENTITY_REQUEST,
            _is_identity_reply,
            timeout=timeout,
        )
        return reply is not None

    def get_firmware_version(self, timeout: float = 1.5) -> str | None:
        """Return a firmware version string from the MIDI Identity Reply, or None.

        The reply payload (bytes after the F0 7E ... 06 02 header) contains:
          manufacturer ID (3 bytes) + family (2) + member (2) + version (4).
        We format the 4 version bytes as "major.minor.patch.build".
        """
        try:
            self._ensure_open()
        except Exception:
            return None
        reply = self._send_and_recv(
            _IDENTITY_REQUEST,
            _is_identity_reply,
            timeout=timeout,
        )
        if reply is None:
            return None
        d = reply.data
        # d: [7E, devID, 06, 02, mfr0, mfr1, mfr2, fam0, fam1, mem0, mem1, v0, v1, v2, v3]
        if len(d) >= 15:
            v = d[11:15]
            return f"{v[0]}.{v[1]}.{v[2]}.{v[3]}"
        # Shorter reply — just return hex
        return " ".join(f"{b:02X}" for b in d)

    # ------------------------------------------------------------------
    # Status query
    # ------------------------------------------------------------------

    def get_status(self, timeout: float | None = None) -> StatusResponse | None:
        """Send STATUS_QUERY and return a decoded StatusResponse, or None on timeout."""
        msg_data = _build_status_sysex(_STATUS_QUERY)
        reply = self._send_and_recv(
            msg_data,
            lambda m: _is_status_reply(m, _STATUS_QUERY),
            timeout=timeout,
        )
        if reply is None:
            return None
        # Payload starts after the 6-byte header (manufacturer + cmd + subcmd)
        payload = list(reply.data[6:])
        return StatusResponse.from_packed(payload)

    # ------------------------------------------------------------------
    # POST results
    # ------------------------------------------------------------------

    def get_post_results(self, timeout: float | None = None) -> list[int] | None:
        """Send POST_RESULTS query and return the raw result bytes.

        Each byte is a TestResult enum value (0-4).  Returns None on timeout,
        or an empty list if the firmware was not built with ENABLE_ON_DEVICE_TESTS.
        """
        msg_data = _build_status_sysex(_POST_RESULTS)
        reply = self._send_and_recv(
            msg_data,
            lambda m: _is_status_reply(m, _POST_RESULTS),
            timeout=timeout,
        )
        if reply is None:
            return None
        # Payload is after header bytes (manufacturer + cmd + subcmd = 6 bytes)
        return list(reply.data[6:])

    # ------------------------------------------------------------------
    # Demo mode
    # ------------------------------------------------------------------

    def start_demo(self, timeout: float | None = None) -> int:
        """Send START_DEMO and return the ACK code (ACK_OK / ACK_BUSY / ACK_ERROR).

        Returns ACK_ERROR if the reply times out.
        """
        msg_data = _build_status_sysex(_START_DEMO)
        reply = self._send_and_recv(
            msg_data,
            lambda m: _is_status_reply(m, _START_DEMO),
            timeout=timeout,
        )
        if reply is None:
            return ACK_ERROR
        d = reply.data
        # ACK reply: [..., STATUS_CMD, START_DEMO, ack_code]
        return d[6] if len(d) > 6 else ACK_ERROR

    # ------------------------------------------------------------------
    # Song load / save
    # ------------------------------------------------------------------

    def load_song(
        self,
        path: str,
        timeout: float = 30.0,
        poll_interval: float = 0.5,
    ) -> bool:
        """Send LOAD_SONG, then poll status until song_swap_active clears.

        Args:
            path: Filename as the Deluge expects it (e.g. "SONGS/MY_SONG.XML").
                  Max 100 characters.
            timeout: Total seconds to wait for the load to complete.
            poll_interval: Seconds between status polls.

        Returns:
            True if the load completed (song_swap_active went False),
            False on ACK_BUSY / ACK_ERROR / timeout.
        """
        payload = path.encode("ascii", errors="replace")[:100]
        msg_data = _build_status_sysex(_LOAD_SONG, payload)
        reply = self._send_and_recv(
            msg_data,
            lambda m: _is_status_reply(m, _LOAD_SONG),
            timeout=self._timeout,
        )
        if reply is None:
            return False
        d = reply.data
        ack = d[6] if len(d) > 6 else ACK_ERROR
        if ack != ACK_OK:
            return False

        # Poll status until song_swap_active clears
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            status = self.get_status()
            if status is not None and not status.song_swap_active:
                return True
            time.sleep(poll_interval)
        return False

    def save_song(self, path: str, timeout: float | None = None) -> bool:
        """Send SAVE_SONG and return True on ACK_OK.

        Args:
            path: Filename as the Deluge expects it (e.g. "SONGS/MY_SONG.XML").
                  Max 100 characters.
        """
        payload = path.encode("ascii", errors="replace")[:100]
        msg_data = _build_status_sysex(_SAVE_SONG, payload)
        reply = self._send_and_recv(
            msg_data,
            lambda m: _is_status_reply(m, _SAVE_SONG),
            timeout=timeout,
        )
        if reply is None:
            return False
        d = reply.data
        ack = d[6] if len(d) > 6 else ACK_ERROR
        return ack == ACK_OK

    # ------------------------------------------------------------------
    # CC heartbeat enable/disable
    # ------------------------------------------------------------------

    def enable_status_cc(self, on: bool = True, rate_hz: int = 1) -> bool:
        """Enable or disable the CC heartbeat on the Deluge.

        Args:
            on: True to enable, False to disable.
            rate_hz: Heartbeat rate in Hz (1-10).  Clamped by firmware to 1-10.

        Returns:
            True if the Deluge replied ACK_OK.
        """
        rate_hz = max(1, min(10, rate_hz))
        payload = bytes([1 if on else 0, rate_hz])
        msg_data = _build_status_sysex(_ENABLE_STATUS_CC, payload)
        reply = self._send_and_recv(
            msg_data,
            lambda m: _is_status_reply(m, _ENABLE_STATUS_CC),
        )
        if reply is None:
            return False
        d = reply.data
        ack = d[6] if len(d) > 6 else ACK_ERROR
        return ack == ACK_OK

    def set_midi_learn(self, command_index: int, channel: int, cc: int) -> bool:
        """Programmatically set a GlobalMIDICommand mapping.

        Args:
            command_index: GlobalMIDICommand enum value (0=PLAYBACK_RESTART,
                1=PLAY, 2=RECORD, 3=TAP, 4=LOOP, 5=LOOP_CONTINUOUS_LAYERING,
                6=UNDO, 7=REDO, 8=FILL, 9=TRANSPOSE, 10=NEXT_SONG,
                11+=STATUS_SRAM..STATUS_MODE when ENABLE_SYSEX_STATUS)
            channel: MIDI channel 0-15.
            cc: CC number 0-127.

        Returns:
            True if the Deluge replied ACK_OK.
        """
        payload = bytes([command_index, channel, cc])
        msg_data = _build_status_sysex(_SET_MIDI_LEARN, payload)
        reply = self._send_and_recv(
            msg_data,
            lambda m: _is_status_reply(m, _SET_MIDI_LEARN),
        )
        if reply is None:
            return False
        d = reply.data
        ack = d[6] if len(d) > 6 else ACK_ERROR
        return ack == ACK_OK

    def setup_test_mappings(self) -> dict[str, bool]:
        """Configure all GlobalMIDICommand CC mappings for test automation.

        Sets up a standard set of CC assignments on channel 1 so the Python
        test harness can control the device without manual MIDI Learn.

        Returns:
            Dict of command name -> success bool.
        """
        mappings = {
            "PLAYBACK_RESTART": (0, 0, 101),
            "PLAY": (1, 0, 102),
            "RECORD": (2, 0, 103),
            "TAP": (3, 0, 105),
            "LOOP": (4, 0, 104),
            "LOOP_LAYERING": (5, 0, 106),
            "UNDO": (6, 0, 107),
            "REDO": (7, 0, 108),
            "FILL": (8, 0, 109),
            "TRANSPOSE": (9, 0, 110),
            "NEXT_SONG": (10, 0, 112),
        }
        results = {}
        for name, (idx, ch, cc) in mappings.items():
            results[name] = self.set_midi_learn(idx, ch, cc)
        return results

    # ------------------------------------------------------------------
    # Transport CC
    # ------------------------------------------------------------------

    def _send_cc(self, name: str, value: int) -> None:
        """Send a transport CC if the port is open and the mapping exists."""
        entry = self._transport.get(name)
        if entry is None:
            warnings.warn(f"No CC mapping for transport command {name!r}")
            return
        ch, cc = entry
        self._ensure_open()
        self._out.send(  # type: ignore[union-attr]
            mido.Message("control_change", channel=ch, control=cc, value=value)
        )

    def play(self) -> None:
        """Tap the Play button (toggle play/stop) via CC."""
        self._send_cc("play", 127)

    def record(self) -> None:
        """Tap the Record button via CC."""
        self._send_cc("record", 127)

    def undo(self) -> None:
        """Send an Undo command via CC."""
        self._send_cc("undo", 127)

    def fill(self, on: bool = True) -> None:
        """Press or release the Fill button via CC."""
        self._send_cc("fill", 127 if on else 0)

    def loop(self) -> None:
        """Tap the Loop button via CC."""
        self._send_cc("loop", 127)

    # ------------------------------------------------------------------
    # Status monitor (background CC heartbeat reader)
    # ------------------------------------------------------------------

    def start_status_monitor(
        self,
        on_heartbeat: Callable[[CCHeartbeat], None] | None = None,
    ) -> None:
        """Start the background CC heartbeat monitor thread.

        Args:
            on_heartbeat: Optional callback called with each CCHeartbeat received.
                The callback runs on the monitor thread — keep it fast.

        The monitor reads from the status_in port.  To retrieve the most recent
        heartbeat, call :meth:`get_latest_heartbeat`.
        """
        if self._monitor and self._monitor._thread and self._monitor._thread.is_alive():
            return  # already running
        self._monitor = _StatusMonitor(
            port_name=self._status_in_name,
            cc_config=self._cc_config,
            on_heartbeat=on_heartbeat,
        )
        self._monitor.start()

    def stop_status_monitor(self) -> None:
        """Stop the background CC heartbeat monitor thread."""
        if self._monitor is not None:
            self._monitor.stop()
            self._monitor = None

    def get_latest_heartbeat(self) -> CCHeartbeat | None:
        """Return the most recently received CCHeartbeat, or None if none yet."""
        if self._monitor is None:
            return None
        return self._monitor.latest

    # ------------------------------------------------------------------
    # Windows MIDI services diagnostic
    # ------------------------------------------------------------------

    @staticmethod
    def check_windows_midi_services() -> None:
        """Warn if the Windows MidiSrv service is running.

        MidiSrv (Windows MIDI 2.0 service) can intercept USB MIDI devices
        and prevent python-rtmidi from seeing them.  Run this at startup
        if you get "port not found" errors on Windows 11.
        """
        try:
            result = subprocess.run(
                ["sc", "query", "MidiSrv"],
                capture_output=True,
                text=True,
                timeout=5,
            )
            if "RUNNING" in result.stdout:
                warnings.warn(
                    "Windows MidiSrv service is RUNNING.  This may block USB MIDI access.\n"
                    "To stop it temporarily:  sc stop MidiSrv\n"
                    "To disable permanently: sc config MidiSrv start= disabled",
                    stacklevel=2,
                )
        except FileNotFoundError:
            pass  # Not Windows or sc not available
        except subprocess.TimeoutExpired:
            pass

    # ------------------------------------------------------------------
    # Repr
    # ------------------------------------------------------------------

    def __repr__(self) -> str:
        state = "open" if self._out is not None else "closed"
        return (
            f"DelugeClient({state}, sysex_out={self._sysex_out_name!r}, "
            f"sysex_in={self._sysex_in_name!r})"
        )
