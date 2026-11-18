"""Phase 2 of FastLED #3021 — bench-validate LPC SCT-RX against the
LPC845 default clockless WS2812 driver.

Sends a WS2812 frame through the platform default clockless driver (the
one `addLeds<WS2812, PIN>()` selects, fed the way its low-memory direct show
feeds it, but with the pin chosen at run time), captures the wire via SCT
input-capture + DMA (`LpcSctRxChannel`), decodes the 4-phase WS2812 timing,
and byte-matches the recovered LED data against what was sent.

TX/RX come from the firmware's `findConnectedPins` jumper scan unless both
`--tx-pin` and `--rx-pin` are given. TX is driven from a push-pull pad:
LPC845 P0_10/P0_11 are true open-drain I2C pads whose rising edge is the
slow pull-up (~1.7 us), longer than any WS2812 HIGH (FastLED #5266).

Five test patterns, mirroring the FlexIO `flexioObjectFledTest`:

    | Case | LEDs | Content                              |
    |------|------|--------------------------------------|
    |  0   |  1   | Red (0xFF, 0x00, 0x00)               |
    |  1   |  3   | R, G, B chain                        |
    |  2   |  1   | All zeros (T0H/T0L fidelity)         |
    |  3   |  1   | All ones (T1H/T1L fidelity)          |
    |  4   |  100 | Alternating R/G/B (watchdog safety)  |

Case 4 sends 100 LEDs (4800 edges), but the 16 KB-SRAM sketch captures at
most `cap` = 192 edges, so only the first `cap // 16` bytes can be checked.
The firmware reports `cap`; the case passes when that whole prefix decodes
and matches (see `checkable_bytes`).

Usage:
    uv run python -m ci.autoresearch.test_lpc_ws2812_loopback --port <port>

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from ci.autoresearch.lpc_pin_scan import resolve_pins
from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench  # noqa: E402,F401
from ci.util.global_interrupt_handler import handle_keyboard_interrupt  # noqa: E402


def send_rpc(
    s,
    method,
    args=None,
    request_id=None,
    timeout: float = 10.0,
):
    """Shim over RpcBench (`s`) so run_* keep their reply["result"] shape.

    Device serial goes through fbuild's Rust monitor (never raw pyserial);
    see agents/docs/hardware-autoresearch.md -> "Device serial". The
    request_id kwarg is accepted for call-site compatibility and ignored
    (RpcClient assigns its own correlation ids).
    """
    _ = request_id
    result = s.call_flat(method, args=args, timeout=timeout)
    if result is None:
        return None
    if result is METHOD_NOT_FOUND:
        return {"error": {"code": -32601, "message": "method not found"}}
    return {"result": result}


DEFAULT_PORT = "COM10"
DEFAULT_BAUD = 115200


@dataclass
class TestCase:
    index: int
    label: str
    num_leds: int


CASES: list[TestCase] = [
    TestCase(0, "Red single LED", 1),
    TestCase(1, "RGB three-LED chain", 3),
    TestCase(2, "All zeros (T0 fidelity)", 1),
    TestCase(3, "All ones (T1 fidelity)", 1),
    TestCase(4, "100-LED alternating R/G/B", 100),
]


CSV_FIELDS: tuple[str, ...] = (
    "success",
    "test_case",
    "num_leds",
    "exp_bytes",
    "dec_bytes",
    "matched",
    "mismatched",
    "edges",
    "cap",
)

# Each WS2812 bit is one HIGH and one LOW interval, so a byte is 16 edges.
EDGES_PER_BYTE = 16


def parse_csv_result(csv: str) -> dict[str, int]:
    """Parse the low-memory sketch's ws2812SctTest CSV reply.

    Format (must match the sketch handler):
        success,test_case,num_leds,exp_bytes,dec_bytes,matched,mismatched,edges,cap

    `mismatched` counts decoded bytes that differ from what was sent;
    `exp_bytes - dec_bytes` were not decoded at all. `cap` is the edge
    capacity of the capture.
    """
    parts = csv.split(",")
    if len(parts) != len(CSV_FIELDS):
        return {}
    try:
        return {k: int(v) for k, v in zip(CSV_FIELDS, parts)}
    except ValueError:
        return {}


def checkable_bytes(exp_bytes: int, cap: int) -> int:
    """Bytes a capture of `cap` edges can hold, at most the frame size."""
    return min(exp_bytes, cap // EDGES_PER_BYTE)


def evaluate(case: TestCase, parsed: dict[str, int]) -> tuple[bool, str]:
    num_leds = parsed.get("num_leds", 0)
    exp_bytes = parsed.get("exp_bytes", 0)
    dec_bytes = parsed.get("dec_bytes", 0)
    matched = parsed.get("matched", 0)
    mismatched = parsed.get("mismatched", 0)
    edges = parsed.get("edges", 0)
    cap = parsed.get("cap", 0)

    if num_leds == 0:
        return False, (
            "firmware rejected the arguments (TX on an open-drain pad, a "
            "reserved pad, TX == RX, or a pin out of range)"
        )
    if num_leds != case.num_leds:
        return False, (
            f"firmware reported num_leds={num_leds}, expected {case.num_leds} — "
            "test case mismatch"
        )
    if exp_bytes != case.num_leds * 3:
        return False, (
            f"firmware expected_bytes={exp_bytes}, expected {case.num_leds * 3}"
        )

    need = checkable_bytes(exp_bytes, cap)
    msg = (
        f"num_leds={num_leds}, exp_bytes={exp_bytes}, dec_bytes={dec_bytes}, "
        f"matched={matched}, mismatched={mismatched}, edges={edges}, cap={cap}"
    )
    if need < exp_bytes:
        msg += f" (prefix check: {cap} edges hold {need} of {exp_bytes} bytes)"
    # The firmware applies the same rule; both must agree.
    ok = mismatched == 0 and matched >= need and need > 0
    if ok != bool(parsed.get("success", 0)):
        return False, msg + " (firmware and host verdicts disagree)"
    return ok, msg


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default=DEFAULT_PORT)
    p.add_argument("--baud", default=DEFAULT_BAUD, type=int)
    p.add_argument(
        "--tx-pin",
        default=None,
        type=int,
        help="LPC845 GPIO PIO0_n driven by the WS2812 TX; must be a push-pull "
        "pad (default: discovered by the findConnectedPins jumper scan)",
    )
    p.add_argument(
        "--rx-pin",
        default=None,
        type=int,
        help="LPC845 GPIO PIO0_n routed to SCT input 0 for capture "
        "(default: discovered by the jumper scan)",
    )
    p.add_argument(
        "--capture-ms",
        default=50,
        type=int,
        help="SCT capture window per test (default: 50 ms)",
    )
    args = p.parse_args()

    print(f"[lpc-ws2812-loopback] opening {args.port} @ {args.baud}")
    try:
        s = RpcBench(args.port)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:  # noqa: BLE001
        print(f"Could not connect to {args.port}: {e}")
        return 1

    with s:
        echo = send_rpc(s, "echo", args=[42], request_id=1, timeout=5.0)
        if not echo or echo.get("result") != 42:
            print(
                "ERROR: echo failed; is AutoResearchLpc flashed and running? "
                f"Got: {echo}",
                file=sys.stderr,
            )
            return 2
        print("[lpc-ws2812-loopback] echo ok")

        pins = resolve_pins(s, args.tx_pin, args.rx_pin, "lpc-ws2812-loopback")
        if pins is None:
            return 3
        tx_pin, rx_pin = pins

        passes = 0
        fails = 0
        for i, case in enumerate(CASES, start=20):
            print(f"\n--- case {case.index} — {case.label} ---")
            resp = send_rpc(
                s,
                "ws2812SctTest",
                args=[case.index, tx_pin, rx_pin, args.capture_ms],
                request_id=i,
                timeout=args.capture_ms / 1000.0 * 4 + 5.0,
            )
            if not resp or "result" not in resp:
                print(f"  FAIL: no response or RPC error: {resp}")
                fails += 1
                continue
            csv = resp["result"]
            if not isinstance(csv, str):
                print(f"  FAIL: expected string CSV, got {type(csv).__name__}: {csv}")
                fails += 1
                continue
            parsed = parse_csv_result(csv)
            if not parsed:
                print(f"  FAIL: malformed CSV: {csv!r}")
                fails += 1
                continue
            ok, msg = evaluate(case, parsed)
            if ok:
                print(f"  PASS: {msg}")
                passes += 1
            else:
                print(f"  FAIL: {msg}")
                print(f"        raw csv: {csv!r}")
                fails += 1

        print(
            f"\n[lpc-ws2812-loopback] summary: {passes} pass / {fails} fail "
            f"out of {len(CASES)} cases"
        )
        return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
