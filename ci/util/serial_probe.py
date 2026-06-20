#!/usr/bin/env python3
"""Ad-hoc serial-port probe with correct DTR/RTS handshake.

This is the agent-facing replacement for the forbidden PowerShell
`[System.IO.Ports.SerialPort]::Open()` pattern (FastLED #3336). It also
backs `port_utils.py`'s richer LPC845-BRK detection.

Two modes:

  - `list`   — enumerate every detected serial port with rich metadata
               (VID:PID, description, manufacturer, board hint).
  - `read`   — open a port with DTR=True/RTS=True (the universal host-ready
               idle state), read for N seconds, dump everything to stdout.

Why DTR=True/RTS=True is the right default:

  ESP32 native USB CDC: idle state — no reset, normal operation.
  CP2102 / CH340 / FTDI: harmless — chip gates data through.
  LPC11U35 USB-VCOM bridge (LPC845-BRK): REQUIRED — bridge uses DTR as a
    "host ready" signal. DTR=False makes the bridge silently drop every
    byte the device transmits. This is what burned #3300 / #3325 for two
    full sessions before being noticed.

Run from project root via `bash test` won't work (this is not a unit test);
invoke directly:

    uv run --no-project --with pyserial python ci/util/serial_probe.py list
    uv run --no-project --with pyserial python ci/util/serial_probe.py read COM20
    uv run --no-project --with pyserial python ci/util/serial_probe.py read COM20 --seconds 8
    uv run --no-project --with pyserial python ci/util/serial_probe.py read COM20 --send '{"jsonrpc":"2.0","id":1,"method":"echo","params":[4242]}'

See also:
- `ci/util/port_utils.py`        — autoresearch's port-detection logic.
- `ci/util/pyserial_monitor.py`  — the long-lived monitor used by autoresearch.
- FastLED #3336                  — why the PowerShell pattern is forbidden.
"""

from __future__ import annotations

import argparse
import sys
import time
from dataclasses import dataclass

import serial  # type: ignore[import-not-found]
import serial.tools.list_ports  # type: ignore[import-not-found]


# Known board USB VID:PID fingerprints. Add new entries here when porting to
# a new board so port_utils.py can pick the right COM port without guessing.
BOARD_FINGERPRINTS: dict[tuple[int, int], str] = {
    # NXP LPC845-BRK ships with an LPC11U35 carrying both the CMSIS-DAP
    # debug interface AND a USB-VCOM bridge for the LPC845's USART0.
    (0x1FC9, 0x0132): "NXP CMSIS-DAP debug (LPC845-BRK / LPC11U35)",
    # 16C0:0483 is shared between the LPC11U35 VCOM bridge (when used on
    # LPC845-BRK) and PJRC Teensy USB-Serial. Disambiguation requires
    # additional signal — e.g. presence of a sibling CMSIS-DAP port for
    # LPC, or `manufacturer.startswith("Teensyduino")` for PJRC. The
    # fingerprint table only carries the joint hint; callers that need
    # to act on the difference must inspect `manufacturer` / `serial_number`
    # or the companion-port heuristic in `find_lpc845brk_vcom`.
    (
        0x16C0,
        0x0483,
    ): "LPC11U35 VCOM bridge (LPC845-BRK USART0) OR PJRC Teensy USB-Serial",
    # Espressif native USB CDC across ESP32-S3/C3/C6/H2/P4 variants.
    (0x303A, 0x1001): "Espressif native USB CDC (ESP32-S3/C3/C6/H2/P4)",
    # CP2102 from Silicon Labs — common on ESP32-WROOM dev kits.
    (0x10C4, 0xEA60): "Silicon Labs CP2102 USB-UART (ESP32 dev kit)",
    # CH340/CH341 from WCH — common on cheaper ESP32/Arduino clones.
    (0x1A86, 0x7523): "WCH CH340 USB-Serial",
    (0x1A86, 0x55D4): "WCH CH343 USB-Serial",
    # FTDI FT232R — older Arduinos, some shields.
    (0x0403, 0x6001): "FTDI FT232R USB-UART",
    (0x0403, 0x6010): "FTDI FT2232 dual UART",
    (0x0403, 0x6014): "FTDI FT232H USB-UART",
    # Teensy 3.x/4.x.
    (0x16C0, 0x0486): "PJRC / Teensy USB-Serial (alt)",
}


@dataclass(frozen=True)
class PortInfo:
    device: str
    vid: int | None
    pid: int | None
    description: str
    manufacturer: str | None
    serial_number: str | None
    hwid: str

    @property
    def vid_pid(self) -> tuple[int, int] | None:
        return (self.vid, self.pid) if (self.vid and self.pid) else None

    @property
    def board_hint(self) -> str | None:
        vp = self.vid_pid
        if vp is None:
            return None
        return BOARD_FINGERPRINTS.get(vp)

    def format(self) -> str:
        vp = f"{self.vid:04X}:{self.pid:04X}" if self.vid and self.pid else "----:----"
        hint = f"  [{self.board_hint}]" if self.board_hint else ""
        ser = f"  ser={self.serial_number}" if self.serial_number else ""
        return f"{self.device:8s}  {vp}{ser}  {self.description}{hint}"


def list_ports() -> list[PortInfo]:
    out: list[PortInfo] = []
    for p in serial.tools.list_ports.comports():
        out.append(
            PortInfo(
                device=p.device,
                vid=p.vid,
                pid=p.pid,
                description=p.description or "",
                manufacturer=p.manufacturer,
                serial_number=p.serial_number,
                hwid=p.hwid or "",
            )
        )
    return out


def find_by_vid_pid(vid: int, pid: int) -> list[PortInfo]:
    """Return all detected ports matching the given VID:PID."""
    return [p for p in list_ports() if p.vid_pid == (vid, pid)]


def find_lpc845brk_vcom() -> PortInfo | None:
    """Return the LPC845-BRK USART0 VCOM port (LPC11U35 bridge), or None.

    Returns None on ambiguity — `16C0:0483` is shared between the LPC11U35
    VCOM bridge and PJRC Teensy USB-Serial, so when multiple matches are
    detected we cannot pick one safely without additional signal. The
    caller should require an explicit `--port` instead. (Per the
    no-silent-fallback rule; see PR #3339 review.)
    """
    matches = find_by_vid_pid(0x16C0, 0x0483)
    if not matches:
        return None
    if len(matches) == 1:
        return matches[0]
    # Ambiguous: filter to ports whose `manufacturer` looks like the LPC's
    # mbed/DAPLink stack (rather than PJRC's "Teensyduino"). If exactly one
    # remains, that's our LPC. Otherwise fail fast.
    lpc_like = [
        p
        for p in matches
        if p.manufacturer and not p.manufacturer.lower().startswith("teensy")
    ]
    if len(lpc_like) == 1:
        return lpc_like[0]
    return None


def read_port(
    device: str,
    seconds: float,
    baud: int = 115200,
    send: str | None = None,
    quiet: bool = False,
) -> int:
    """Open a port with DTR/RTS=True, optionally send, then dump bytes for `seconds`.

    Returns the total byte count read. Exits non-zero on open failure.
    """
    try:
        s = serial.Serial(device, baud, timeout=0.2, write_timeout=1.0)
    except serial.SerialException as e:
        print(f"ERROR: cannot open {device}: {e}", file=sys.stderr)
        return -1

    try:
        # The whole point of this helper — universal host-ready idle state.
        s.dtr = True
        s.rts = True
        # Small settle to let the bridge flush whatever it had buffered while
        # DTR was de-asserted.
        time.sleep(0.2)

        if send is not None:
            payload = send if send.endswith("\n") else send + "\n"
            s.write(payload.encode("utf-8"))
            s.flush()
            if not quiet:
                print(f"[serial_probe] SENT: {send!r}", file=sys.stderr)

        deadline = time.monotonic() + seconds
        total = 0
        buf = bytearray()
        while time.monotonic() < deadline:
            n = s.in_waiting
            if n:
                chunk = s.read(n)
                total += len(chunk)
                buf.extend(chunk)
                # Flush as text per-newline for streaming feel.
                while b"\n" in buf:
                    line, _, rest = buf.partition(b"\n")
                    sys.stdout.write(line.decode("utf-8", errors="replace") + "\n")
                    buf = bytearray(rest)
                sys.stdout.flush()
            else:
                time.sleep(0.02)
        if buf:
            sys.stdout.write(buf.decode("utf-8", errors="replace"))
            sys.stdout.flush()
        if not quiet:
            print(
                f"\n[serial_probe] {total} bytes in {seconds:.1f}s from {device}",
                file=sys.stderr,
            )
        return total
    finally:
        s.close()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Ad-hoc serial-port probe (replaces forbidden PowerShell SerialPort patterns; see FastLED #3336)."
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_list = sub.add_parser(
        "list", help="Enumerate all serial ports with rich metadata."
    )
    p_list.add_argument(
        "--vid-pid",
        metavar="VID:PID",
        help="Filter to ports matching the given VID:PID (hex, e.g. 16C0:0483 for LPC845-BRK VCOM).",
    )

    p_read = sub.add_parser(
        "read", help="Open a port with DTR=True/RTS=True and dump bytes."
    )
    p_read.add_argument("port", help="Serial port (e.g. COM20, /dev/ttyUSB0).")
    p_read.add_argument(
        "--seconds", type=float, default=6.0, help="How long to read (default: 6.0 s)."
    )
    p_read.add_argument(
        "--baud", type=int, default=115200, help="Baud rate (default: 115200)."
    )
    p_read.add_argument(
        "--send",
        default=None,
        help="Optional payload to write immediately after open (auto-appends newline).",
    )
    p_read.add_argument(
        "--quiet", action="store_true", help="Suppress informational stderr output."
    )

    p_find = sub.add_parser(
        "find-lpc",
        help="Print the LPC845-BRK USART0 VCOM port device path (16C0:0483) or exit non-zero.",
    )
    p_find.add_argument(
        "--quiet", action="store_true", help="Print device only, no banner."
    )

    args = parser.parse_args(argv)

    if args.cmd == "list":
        ports = list_ports()
        filt = args.vid_pid
        if filt:
            try:
                vid_s, pid_s = filt.split(":")
                vid, pid = int(vid_s, 16), int(pid_s, 16)
                ports = [p for p in ports if p.vid_pid == (vid, pid)]
            except (ValueError, AttributeError):
                print(
                    f"ERROR: bad --vid-pid: {filt!r} (expected HEX:HEX)",
                    file=sys.stderr,
                )
                return 2
        if not ports:
            print("(no matching ports)", file=sys.stderr)
            return 1
        for p in ports:
            print(p.format())
        return 0

    if args.cmd == "read":
        rc = read_port(
            args.port,
            seconds=args.seconds,
            baud=args.baud,
            send=args.send,
            quiet=args.quiet,
        )
        return 0 if rc >= 0 else 1

    if args.cmd == "find-lpc":
        p = find_lpc845brk_vcom()
        if p is None:
            if not args.quiet:
                print(
                    "ERROR: no LPC845-BRK VCOM port detected (VID:PID 16C0:0483)",
                    file=sys.stderr,
                )
            return 1
        if args.quiet:
            print(p.device)
        else:
            print(p.format())
        return 0

    return 2


if __name__ == "__main__":
    sys.exit(main())
