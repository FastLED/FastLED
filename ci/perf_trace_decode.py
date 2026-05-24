#!/usr/bin/env python3
"""Decode a flash dump from examples/PerfTraceBench/PerfTraceBench.ino.

The sketch writes a self-describing binary blob to a data partition on the
ESP32-P4. Pull it back with:

    esptool --chip esp32p4 --port COM25 read-flash <part_offset> <size> dump.bin

Then run:

    uv run ci/perf_trace_decode.py dump.bin

Layout matches the C++ structs in PerfTraceBench.ino.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path
from typing import Any


HEADER_FMT = "<IHHII"  # magic, version, num_frames, total_bytes, reserved
HEADER_SIZE = struct.calcsize(HEADER_FMT)
HEADER_MAGIC = 0xFA571DED

FRAME_FMT = "<IIIHBB"  # magic, frame_idx, show_us, event_count, dropped, pad
FRAME_SIZE = struct.calcsize(FRAME_FMT)
FRAME_MAGIC = 0xF8AAE000

EVENT_FMT = "<IhhIII"  # magic, depth (i16), line (i16), dur_us, start_us, name_offset
EVENT_SIZE = struct.calcsize(EVENT_FMT)
EVENT_MAGIC = 0xE0E07E07


def decode(blob: bytes) -> dict[str, Any]:
    if len(blob) < HEADER_SIZE:
        raise ValueError(f"too short ({len(blob)} bytes) for header")

    magic, version, num_frames, total_bytes, _ = struct.unpack(
        HEADER_FMT, blob[:HEADER_SIZE]
    )
    if magic != HEADER_MAGIC:
        raise ValueError(
            f"bad header magic 0x{magic:08x} (expected 0x{HEADER_MAGIC:08x})"
        )

    pos = HEADER_SIZE
    frames: list[dict[str, Any]] = []
    for _ in range(num_frames):
        if pos + FRAME_SIZE > len(blob):
            raise ValueError(f"truncated frame record at offset {pos}")
        fm, fidx, show_us, ev_count, dropped, _pad = struct.unpack(
            FRAME_FMT, blob[pos : pos + FRAME_SIZE]
        )
        if fm != FRAME_MAGIC:
            raise ValueError(
                f"bad frame magic 0x{fm:08x} at offset {pos} "
                f"(expected 0x{FRAME_MAGIC:08x})"
            )
        pos += FRAME_SIZE
        events: list[tuple[int, int, int, int, int]] = []
        for _ in range(ev_count):
            if pos + EVENT_SIZE > len(blob):
                raise ValueError(f"truncated event record at offset {pos}")
            em, depth, line, dur_us, start_us, name_off = struct.unpack(
                EVENT_FMT, blob[pos : pos + EVENT_SIZE]
            )
            if em != EVENT_MAGIC:
                raise ValueError(
                    f"bad event magic 0x{em:08x} at offset {pos} "
                    f"(expected 0x{EVENT_MAGIC:08x})"
                )
            pos += EVENT_SIZE
            events.append((depth, line, dur_us, start_us, name_off))
        frames.append(
            {
                "frame_idx": fidx,
                "show_us": show_us,
                "dropped": bool(dropped),
                "events": events,
            }
        )

    # Name table: u32 count, then [u16 len, bytes[len]] × count.
    if pos + 4 > len(blob):
        # Names absent — return what we have.
        return {"version": version, "frames": frames, "names": []}
    (name_count,) = struct.unpack_from("<I", blob, pos)
    pos += 4
    names: list[str] = []
    for _ in range(name_count):
        if pos + 2 > len(blob):
            break
        (nlen,) = struct.unpack_from("<H", blob, pos)
        pos += 2
        if pos + nlen > len(blob):
            break
        names.append(blob[pos : pos + nlen].decode("utf-8", errors="replace"))
        pos += nlen

    return {"version": version, "frames": frames, "names": names}


def render(decoded: dict[str, Any]) -> None:
    names = decoded["names"]
    print(
        f"version=v{decoded['version']}  frames={len(decoded['frames'])}  names={len(names)}"
    )
    print()
    for f in decoded["frames"]:
        flag = " (DROPPED)" if f["dropped"] else ""
        print(
            f"=== frame {f['frame_idx']:3d}  show_us={f['show_us']:>7}  events={len(f['events'])}{flag} ==="
        )
        if not f["events"]:
            print("  (no events captured)")
            continue
        print(f"  {'depth':>5}  {'dur_us':>9}  {'start_us':>12}  name")
        for depth, line, dur_us, start_us, name_off in f["events"]:
            name = (
                names[name_off] if 0 <= name_off < len(names) else f"<idx {name_off}>"
            )
            print(f"  {depth:>5}  {dur_us:>9}  {start_us:>12}  {name}:{line}")
        print()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "path", type=Path, help="raw flash dump (from esptool read-flash)"
    )
    parser.add_argument(
        "--skip",
        type=lambda x: int(x, 0),
        default=0,
        help="skip N bytes at the start (default 0). Use this if your dump "
        "starts before the partition header.",
    )
    args = parser.parse_args()

    blob = args.path.read_bytes()
    if args.skip:
        blob = blob[args.skip :]

    # Trim trailing erased flash (0xFF) so summary is cleaner.
    trimmed = blob.rstrip(b"\xff")

    decoded = decode(trimmed)
    render(decoded)
    return 0


if __name__ == "__main__":
    sys.exit(main())
