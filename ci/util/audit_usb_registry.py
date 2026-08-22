#!/usr/bin/env python3
"""Audit this repo's hardcoded USB VID:PID literals against FastLED/boards.

The USB identity registry lives in https://github.com/FastLED/boards. FastLED
consumes it through fbuild. See `agents/docs/usb-vid-pid-registry.md` for the
full rule and the publish procedure.

This script exists so the migration status in that doc can be re-derived rather
than trusted. It fetches the published catalogue, and reports which of the
literals still present in `ci/` resolve from the registry.

    uv run python ci/util/audit_usb_registry.py

Exit codes:
    0 - every audited literal resolves from the registry
    1 - at least one literal is missing
    2 - the catalogue could not be fetched or parsed

A missing literal is a registry gap. We own FastLED/boards, so the fix is to
publish the record there (see the doc for the one-commit procedure), never to
keep or add a local literal.

Note on the endpoint: FastLED/boards publishes the same catalogue twice --
`usb-ids.json` (plain JSON) and `usb-vids.proto.zstd` (zstd-compressed
protobuf). fbuild consumes the protobuf because it wants the compact binary
form. This script deliberately uses the JSON: it carries identical data and
needs nothing outside the standard library, where the protobuf would drag in a
zstd dependency and a hand-rolled wire-format decoder for zero benefit.
"""

from __future__ import annotations

import json
import sys
import urllib.request
from typing import Any, NamedTuple

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


CATALOGUE_URL = "https://fastled.github.io/boards/usb-ids.json"

FETCH_TIMEOUT_S = 30


class Literal(NamedTuple):
    """A VID:PID pair hardcoded somewhere under `ci/`."""

    vid: int
    pid: int
    label: str
    source: str


# Every VID:PID literal in the ci/ tree. Keep in sync when a literal is
# retired - the point of this list is that it shrinks to empty.
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


def fetch_catalogue(url: str = CATALOGUE_URL) -> dict[int, tuple[str, dict[int, str]]]:
    """Fetch `usb-ids.json` and index it as {vid: (vendor_name, {pid: product})}.

    Published shape:

        {"0403": {"Vendor name": "FTDI",
                  "PIDs": [{"6001": "Digilent chipKIT UNO32"}, ...]}, ...}

    Keys are lowercase 4-digit hex. Unparseable entries are skipped rather than
    aborting the run, so an upstream addition we do not understand cannot mask
    the literals we do care about.
    """
    with urllib.request.urlopen(url, timeout=FETCH_TIMEOUT_S) as response:
        payload: dict[str, Any] = json.loads(response.read().decode("utf-8"))

    catalogue: dict[int, tuple[str, dict[int, str]]] = {}
    for vid_hex, entry in payload.items():
        try:
            vid = int(vid_hex, 16)
        except (TypeError, ValueError):
            continue
        if not isinstance(entry, dict):
            continue
        vendor_entry: dict[str, Any] = entry
        vendor_name = str(vendor_entry.get("Vendor name", ""))
        products: dict[int, str] = {}
        pid_items: Any = vendor_entry.get("PIDs") or []
        if not isinstance(pid_items, list):
            continue
        for item in pid_items:
            if not isinstance(item, dict):
                continue
            pid_map: dict[str, Any] = item
            for pid_hex, product in pid_map.items():
                try:
                    products[int(pid_hex, 16)] = str(product)
                except (TypeError, ValueError):
                    continue
        catalogue[vid] = (vendor_name, products)
    return catalogue


def main() -> int:
    try:
        catalogue = fetch_catalogue()
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as exc:
        print(f"FAILED to obtain the FastLED/boards catalogue: {exc}")
        print(f"  endpoint: {CATALOGUE_URL}")
        return 2

    vendor_count = len(catalogue)
    product_count = sum(len(products) for _, products in catalogue.values())
    print(f"FastLED/boards registry: {vendor_count} vendors, {product_count} products")
    print(f"Auditing {len(AUDITED_LITERALS)} literals from ci/\n")

    gaps: list[Literal] = []
    for entry in AUDITED_LITERALS:
        pair = f"{entry.vid:04X}:{entry.pid:04X}"
        vendor = catalogue.get(entry.vid)
        if vendor is None or entry.pid not in vendor[1]:
            gaps.append(entry)
            print(f"  GAP  {pair}  {entry.label}  [{entry.source}]")
            continue
        vendor_name, products = vendor
        print(f"  OK   {pair}  {entry.label}  -> {vendor_name} / {products[entry.pid]}")

    resolved = len(AUDITED_LITERALS) - len(gaps)
    print(f"\n{resolved}/{len(AUDITED_LITERALS)} resolve from FastLED/boards")
    if gaps:
        print(
            "\nEach GAP is a registry gap. We own FastLED/boards -- publish the\n"
            "record there rather than keeping a local literal. One commit to the\n"
            "`other` data branch; see agents/docs/usb-vid-pid-registry.md.\n"
            "Allow a few minutes for the site rebuild before re-running."
        )
        return 1
    print("\nAll audited literals are published upstream and can be retired.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
