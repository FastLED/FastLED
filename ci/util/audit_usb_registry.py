#!/usr/bin/env python3
"""Audit this repo's hardcoded USB VID:PID literals against FastLED/boards.

The USB identity registry lives in https://github.com/FastLED/boards and is
published as a zstd-compressed protobuf, `usb-vids.proto.zstd`. fbuild ingests
that artifact; FastLED consumes it through fbuild. See
`agents/docs/usb-vid-pid-registry.md` for the full rule and the cascade
procedure.

This script exists so the migration status in that doc can be re-derived rather
than trusted. It fetches the published artifact, decodes it, and reports which
of the literals still present in `ci/` resolve from the registry.

    uv run python ci/util/audit_usb_registry.py
    uv run python ci/util/audit_usb_registry.py --lookup 1234:5678

Exit codes:
    0 — every audited literal resolves, or the requested pair resolves
    1 — an audited literal/requested pair is missing, or a known gap is stale
    2 — the artifact could not be fetched or decoded

Exit 0 while a filed gap is outstanding is deliberate: it makes this safe to
wire into CI or a pre-commit hook without going permanently red. Those show as
`GAP*` with their tracking issue. A gap that is NOT in KNOWN_GAPS still fails.

A missing literal is a registry gap, never a reason to keep or add a local
entry. `0403:6014` (FTDI FT232H) is the one known outstanding gap, tracked as
FastLED/boards#60.
"""

from __future__ import annotations

import argparse
import sys
import urllib.request
from typing import Iterator, NamedTuple

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


ARTIFACT_URL = "https://fastled.github.io/boards/usb-vids.proto.zstd"

# Decompression guard: the published artifact is ~8 KB compressed and well
# under a megabyte inflated. A far larger bound still refuses a zip bomb.
MAX_INFLATED_BYTES = 64 << 20

FETCH_TIMEOUT_S = 30


class Literal(NamedTuple):
    """A VID:PID pair hardcoded somewhere under `ci/`."""

    vid: int
    pid: int
    label: str
    source: str


# Every VID:PID literal in the ci/ tree. Keep in sync when a literal is
# retired — the point of this list is that it shrinks to empty.
AUDITED_LITERALS: tuple[Literal, ...] = (
    Literal(0x2E8A, 0x000A, "rp2040 / rpipico application CDC", "port_utils.py"),
    Literal(0x2E8A, 0xF00A, "rpipicow application CDC", "port_utils.py"),
    Literal(0x2E8A, 0x000F, "rp2350 / rpipico2 application CDC", "port_utils.py"),
    Literal(0x2E8A, 0xF00F, "rp2350w / rpipico2w application CDC", "port_utils.py"),
    Literal(0x2E8A, 0x0003, "RP2 ROM BOOTSEL", "port_utils.py (comment)"),
    Literal(0x16C0, 0x0483, "LPCXpresso VCOM (LPC11U35)", "port_utils.py"),
    Literal(0x1FC9, 0x0132, "NXP LPC-Link2 CMSIS-DAP", "port_utils.py"),
    Literal(0x16C0, 0x0486, "PJRC / Teensy USB-Serial (alt)", "serial_probe.py"),
    Literal(0x303A, 0x1001, "Espressif native USB CDC", "serial_probe.py"),
    Literal(0x10C4, 0xEA60, "Silicon Labs CP2102", "serial_probe.py"),
    Literal(0x1A86, 0x7523, "WCH CH340", "serial_probe.py"),
    Literal(0x1A86, 0x55D4, "WCH CH343", "serial_probe.py"),
    Literal(0x0403, 0x6001, "FTDI FT232R", "serial_probe.py"),
    Literal(0x0403, 0x6010, "FTDI FT2232", "serial_probe.py"),
    Literal(0x0403, 0x6014, "FTDI FT232H", "serial_probe.py"),
)


# Gaps already filed upstream and accepted as outstanding. A known gap keeps
# the exit code green so this script can be wired into CI or a pre-commit hook
# without going permanently red; an *unknown* gap still fails. Delete an entry
# once the registry publishes it — the audit then fails loudly if it regresses.
KNOWN_GAPS: dict[tuple[int, int], str] = {
    (0x0403, 0x6014): "FastLED/boards#60",
}


def _read_varint(buf: bytes, pos: int) -> tuple[int, int]:
    """Decode a protobuf base-128 varint at `pos`; return (value, next_pos)."""
    result = 0
    shift = 0
    while True:
        if pos >= len(buf):
            raise ValueError("truncated varint")
        byte = buf[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return result, pos
        shift += 7
        if shift > 63:
            raise ValueError("varint too long")


def _iter_fields(buf: bytes) -> Iterator[tuple[int, int | bytes]]:
    """Yield (field_number, value) for a protobuf message body.

    The schema uses varint (0) and length-delimited (2). Fixed-width fields
    (1, 5) are skipped rather than rejected so a forward-compatible upstream
    addition does not abort the parse; only genuinely malformed wire types
    raise. Callers ignore field numbers they do not recognise.
    """
    pos = 0
    while pos < len(buf):
        key, pos = _read_varint(buf, pos)
        field_number, wire_type = key >> 3, key & 0x07
        if wire_type == 0:
            value, pos = _read_varint(buf, pos)
            yield field_number, value
        elif wire_type == 2:
            length, pos = _read_varint(buf, pos)
            end = pos + length
            if end > len(buf):
                raise ValueError("length-delimited field overruns buffer")
            yield field_number, buf[pos:end]
            pos = end
        elif wire_type in (1, 5):
            # Fixed-width fields (8-byte / 4-byte). The schema does not use
            # them today, but FastLED/boards owns it independently, so an
            # added checksum or timestamp must not abort the parse — that
            # would report "artifact undecodable" when every audited VID:PID
            # is still present. Skip them the way any protobuf reader does.
            width = 8 if wire_type == 1 else 4
            end = pos + width
            if end > len(buf):
                raise ValueError("fixed-width field overruns buffer")
            pos = end
        else:
            # Wire types 3 and 4 are the deprecated start/end-group pair; 6
            # and 7 are unassigned. Any of them means genuinely malformed
            # data rather than a forward-compatible schema addition.
            raise ValueError(f"unsupported wire type {wire_type}")


def _as_bytes(value: int | bytes) -> bytes:
    if not isinstance(value, bytes):
        raise ValueError("expected a length-delimited field")
    return value


def _as_int(value: int | bytes) -> int:
    if isinstance(value, bytes):
        raise ValueError("expected a varint field")
    return value


def decode_registry(raw: bytes) -> dict[int, tuple[str, dict[int, str]]]:
    """Decode the inflated artifact into {vid: (vendor_name, {pid: product})}.

    Schema (published by FastLED/boards):

        message UsbVidDatabase { repeated Vendor vendors = 1; }
        message Vendor  { uint32 vid = 1; string name = 2;
                          repeated Product products = 3; }
        message Product { uint32 pid = 1; string name = 2; }
    """
    registry: dict[int, tuple[str, dict[int, str]]] = {}
    for field_number, value in _iter_fields(raw):
        if field_number != 1:
            continue
        vid: int | None = None
        vendor_name = ""
        products: dict[int, str] = {}
        for vendor_field, vendor_value in _iter_fields(_as_bytes(value)):
            if vendor_field == 1:
                vid = _as_int(vendor_value)
            elif vendor_field == 2:
                vendor_name = _as_bytes(vendor_value).decode("utf-8", "replace")
            elif vendor_field == 3:
                pid: int | None = None
                product_name = ""
                for product_field, product_value in _iter_fields(
                    _as_bytes(vendor_value)
                ):
                    if product_field == 1:
                        pid = _as_int(product_value)
                    elif product_field == 2:
                        product_name = _as_bytes(product_value).decode(
                            "utf-8", "replace"
                        )
                if pid is not None:
                    products[pid] = product_name
        if vid is not None:
            registry[vid] = (vendor_name, products)
    return registry


def fetch_artifact(url: str = ARTIFACT_URL) -> bytes:
    """Download and inflate the published registry artifact."""
    try:
        import zstandard
    except ImportError as exc:  # pragma: no cover - environment-dependent
        raise RuntimeError(
            "zstandard is required to decode usb-vids.proto.zstd. Run this "
            "script with `uv run --with zstandard python "
            "ci/util/audit_usb_registry.py`."
        ) from exc

    with urllib.request.urlopen(url, timeout=FETCH_TIMEOUT_S) as response:
        compressed = response.read()
    return zstandard.ZstdDecompressor().decompress(
        compressed, max_output_size=MAX_INFLATED_BYTES
    )


def _vid_pid(value: str) -> tuple[int, int]:
    """Parse a four-digit hexadecimal VID:PID command-line value."""
    parts = value.split(":")
    if len(parts) != 2 or any(len(part) != 4 for part in parts):
        raise argparse.ArgumentTypeError("expected VID:PID as four hex digits each")
    try:
        vid, pid = (int(part, 16) for part in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            "expected VID:PID as four hex digits each"
        ) from exc
    return vid, pid


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--lookup",
        type=_vid_pid,
        metavar="VID:PID",
        help="check whether one exact hexadecimal VID:PID pair is published",
    )
    args = parser.parse_args(argv)

    try:
        raw = fetch_artifact()
        registry = decode_registry(raw)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as exc:
        print(f"FAILED to obtain the FastLED/boards registry: {exc}")
        print(f"  artifact: {ARTIFACT_URL}")
        return 2

    vendor_count = len(registry)
    product_count = sum(len(products) for _, products in registry.values())
    print(f"FastLED/boards registry: {vendor_count} vendors, {product_count} products")

    if args.lookup is not None:
        vid, pid = args.lookup
        pair = f"{vid:04X}:{pid:04X}"
        vendor = registry.get(vid)
        if vendor is None or pid not in vendor[1]:
            print(f"MISSING {pair} from FastLED/boards")
            return 1
        vendor_name, products = vendor
        print(f"FOUND {pair}  {vendor_name} / {products[pid]}")
        return 0

    print(f"Auditing {len(AUDITED_LITERALS)} literals from ci/\n")

    new_gaps: list[Literal] = []
    known_gaps: list[Literal] = []
    stale_known: list[Literal] = []
    for entry in AUDITED_LITERALS:
        pair = f"{entry.vid:04X}:{entry.pid:04X}"
        tracker = KNOWN_GAPS.get((entry.vid, entry.pid))
        vendor = registry.get(entry.vid)
        if vendor is None or entry.pid not in vendor[1]:
            if tracker is not None:
                known_gaps.append(entry)
                print(f"  GAP* {pair}  {entry.label}  [{entry.source}]  {tracker}")
            else:
                new_gaps.append(entry)
                print(f"  GAP  {pair}  {entry.label}  [{entry.source}]")
            continue
        vendor_name, products = vendor
        print(f"  OK   {pair}  {entry.label}  -> {vendor_name} / {products[entry.pid]}")
        if tracker is not None:
            stale_known.append(entry)

    resolved = len(AUDITED_LITERALS) - len(new_gaps) - len(known_gaps)
    print(f"\n{resolved}/{len(AUDITED_LITERALS)} resolve from FastLED/boards")

    if stale_known:
        # The registry caught up. Prompt the cleanup rather than letting the
        # allowlist quietly mask a gap that no longer exists.
        print("\nKNOWN_GAPS is stale — these now resolve upstream:")
        for entry in stale_known:
            pair = f"{entry.vid:04X}:{entry.pid:04X}"
            tracker = KNOWN_GAPS[(entry.vid, entry.pid)]
            print(f"  {pair}  {entry.label}  ({tracker})")
        print(
            "Drop them from KNOWN_GAPS and retire the literal from "
            f"{stale_known[0].source}."
        )
        return 1

    if new_gaps:
        print(
            "\nEach GAP is a registry gap. Add the record on the FastLED/boards\n"
            "data branch, let fbuild ingest it, then cascade the fbuild pin\n"
            "in pyproject.toml. Do NOT keep or add a local literal.\n"
            "See agents/docs/usb-vid-pid-registry.md."
        )
        return 1

    if known_gaps:
        print(
            f"\n{len(known_gaps)} known gap(s) marked GAP*, already filed "
            "upstream and accepted as outstanding.\nEvery other literal is "
            "published and can be retired."
        )
        return 0

    print("\nAll audited literals are published upstream and can be retired.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
