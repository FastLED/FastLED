"""Jumper discovery for the LPC low-memory AutoResearch build.

The firmware's `findConnectedPins` RPC (examples/AutoResearch/
AutoResearchLowMemory.h) tests every pin against every other pin through
FastLED's own pin backend and replies with one string:

    pairs=4>7 7>4 10>13od ;stuck=8L ;skip=2:debug-swd 3:debug-swd ;od=10 11

`pairs` lists each direction in which `tx` drove `rx` against `rx`'s own
pull (`od` = the driver is an open-drain pad, which can only sink). `stuck`
pins did not follow their own pull with everything else released. `od` pins
have no pulls, so a jumper between two of them needs an external pull-up.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


@dataclass
class LpcPinScan:
    pairs: list[tuple[int, int, bool]] = field(default_factory=list)
    stuck: list[str] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)
    open_drain: list[int] = field(default_factory=list)
    raw: str = ""


def parse_scan(reply: str) -> LpcPinScan:
    """Parse the `findConnectedPins` reply string."""
    scan = LpcPinScan(raw=reply)
    for section in reply.split(";"):
        key, _, value = section.partition("=")
        tokens = value.split()
        if key == "pairs":
            for token in tokens:
                is_od = token.endswith("od")
                tx, _, rx = token.removesuffix("od").partition(">")
                scan.pairs.append((int(tx), int(rx), is_od))
        elif key == "stuck":
            scan.stuck = tokens
        elif key == "skip":
            scan.skipped = tokens
        elif key == "od":
            scan.open_drain = [int(t) for t in tokens]
    return scan


def pick_pair(scan: LpcPinScan) -> tuple[int, int] | None:
    """Prefer a push-pull pair connected in both directions, then any
    push-pull direction. An open-drain detection `od>pp` proves a wire, and
    a wire is bidirectional, so it is driven from the push-pull end: the
    open-drain pad can only sink, and its rising edge would be the slow
    internal pull-up."""
    directions = {(tx, rx) for tx, rx, _ in scan.pairs}
    push_pull = sorted((tx, rx) for tx, rx, od in scan.pairs if not od)
    for tx, rx in push_pull:
        if (rx, tx) in directions:
            return tx, rx
    if push_pull:
        return push_pull[0]
    od_pairs = sorted((rx, tx) for tx, rx, od in scan.pairs if od)
    return od_pairs[0] if od_pairs else None


def format_scan(scan: LpcPinScan) -> str:
    lines = []
    if scan.pairs:
        rendered = ", ".join(
            f"P0_{tx}->P0_{rx}{' (open-drain driver)' if od else ''}"
            for tx, rx, od in scan.pairs
        )
        lines.append(f"  connected: {rendered}")
    else:
        lines.append("  connected: none")
    if scan.stuck:
        lines.append(
            "  stuck (did not follow own pull; H=high, L=low, X=inverted): "
            + " ".join(scan.stuck)
        )
    if scan.skipped:
        lines.append("  not probed: " + " ".join(scan.skipped))
    if scan.open_drain:
        lines.append(
            "  open-drain (no pulls; sink only, needs external pull-up for "
            "high drive; a jumper between two of these is undetectable): "
            + " ".join(f"P0_{p}" for p in scan.open_drain)
        )
    return "\n".join(lines)


def run_scan(bench: Any, timeout: float = 15.0) -> LpcPinScan | None:
    """Call `findConnectedPins` on an RpcBench; None if unbound or silent."""
    reply = bench.call_flat("findConnectedPins", args=[], timeout=timeout)
    if not isinstance(reply, str):
        return None
    return parse_scan(reply)


def resolve_pins(
    bench: Any, tx_pin: int | None, rx_pin: int | None, tag: str
) -> tuple[int, int] | None:
    """Scan, print the wiring, and return (tx, rx).

    Explicit pins win (with a warning if the scan did not see them); else the
    best discovered pair. None when nothing usable was found.
    """
    scan = run_scan(bench)
    if scan is None:
        print(f"[{tag}] findConnectedPins unavailable on this firmware")
        if tx_pin is not None and rx_pin is not None:
            return tx_pin, rx_pin
        return None
    print(f"[{tag}] jumper scan: {scan.raw!r}")
    print(format_scan(scan))
    if tx_pin is not None and rx_pin is not None:
        if not any(tx == tx_pin and rx == rx_pin for tx, rx, _ in scan.pairs):
            print(
                f"[{tag}] WARNING: scan did not find P0_{tx_pin}->P0_{rx_pin}; "
                "using the requested pins anyway"
            )
        return tx_pin, rx_pin
    picked = pick_pair(scan)
    if picked is None:
        print(f"[{tag}] scan found NO connected pin pairs")
        return None
    print(f"[{tag}] using discovered pair TX=P0_{picked[0]} RX=P0_{picked[1]}")
    return picked
