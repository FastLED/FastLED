"""Phase 2 of FastLED #3021 — bench-validate LPC SCT-RX against the
WS2812 bit-bang clockless TX driver (`clockless_arm_lpc.h`).

Drives `FastLED.show()` on the LPC845-BRK's bit-bang `WS2812` driver,
captures the wire via SCT input-capture (`LpcSctRxChannel`), decodes
the 4-phase WS2812 timing, and byte-matches the recovered LED data
against what was sent.

Five test patterns, mirroring the FlexIO `flexioObjectFledTest`:

    | Case | LEDs | Content                              |
    |------|------|--------------------------------------|
    |  0   |  1   | Red (0xFF, 0x00, 0x00)               |
    |  1   |  3   | R, G, B chain                        |
    |  2   |  1   | All zeros (T0H/T0L fidelity)         |
    |  3   |  1   | All ones (T1H/T1L fidelity)          |
    |  4   |  100 | Alternating R/G/B (watchdog safety)  |

**Hardware status:** the LPC845 SCT capture path used in Phase 1 is
polling-based — adequate for the ≤100 kHz pin-toggle bench but NOT
fast enough to capture every edge of an 800 kHz WS2812 stream on a
30 MHz Cortex-M0+. The SCT→DMA upgrade tracked as a #3021 follow-up
is required for `mismatched == 0` on real silicon. Until that lands,
this orchestrator reports raw decode statistics so the polling-vs-DMA
gap is observable.

Usage:
    uv run python ci/autoresearch/test_lpc_ws2812_loopback.py [--port COM10]

The orchestrator also requires the firmware to be built with
`-DFASTLED_LPC_RX_SCT_WS2812=1` (default off — see flash budget note
in `examples/AutoResearchLpc/AutoResearchLpc.ino`).

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from typing import Any


try:
    import serial  # type: ignore
except ImportError:
    serial = None  # type: ignore


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


def send_rpc(
    s: "serial.Serial",
    method: str,
    args: list[Any] | None = None,
    request_id: int = 1,
    timeout: float = 10.0,
) -> dict[str, Any] | None:
    """JSON-RPC send/receive helper (mirrors test_lpc_pin_toggle_rx.py)."""
    params = args if args is not None else []
    req = {"jsonrpc": "2.0", "method": method, "params": params, "id": request_id}
    s.write((json.dumps(req, separators=(",", ":")) + "\n").encode("ascii"))
    s.flush()

    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        chunk = s.read(s.in_waiting or 1)
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, _, buf = buf.partition(b"\n")
            text = line.decode("ascii", errors="replace").strip()
            for prefix in ("REMOTE: ", "RESULT: "):
                if text.startswith(prefix):
                    text = text[len(prefix) :]
                    break
            if not (text.startswith("{") and text.endswith("}")):
                continue
            try:
                msg = json.loads(text)
            except json.JSONDecodeError:
                continue
            if isinstance(msg, dict) and msg.get("id") == request_id:
                return msg
    return None


def parse_csv_result(csv: str) -> dict[str, int]:
    """Parse the AutoResearchLpc ws2812SctTest CSV reply.

    Format (must match the sketch handler):
        success,test_case,num_leds,exp_bytes,dec_bytes,matched,mismatched,edges
    """
    fields = (
        "success",
        "test_case",
        "num_leds",
        "exp_bytes",
        "dec_bytes",
        "matched",
        "mismatched",
        "edges",
    )
    parts = csv.split(",")
    if len(parts) != len(fields):
        return {}
    try:
        return {k: int(v) for k, v in zip(fields, parts)}
    except ValueError:
        return {}


def evaluate(case: TestCase, parsed: dict[str, int]) -> tuple[bool, str]:
    success = parsed.get("success", 0)
    num_leds = parsed.get("num_leds", 0)
    exp_bytes = parsed.get("exp_bytes", 0)
    dec_bytes = parsed.get("dec_bytes", 0)
    matched = parsed.get("matched", 0)
    mismatched = parsed.get("mismatched", 0)
    edges = parsed.get("edges", 0)

    if num_leds != case.num_leds:
        return False, (
            f"firmware reported num_leds={num_leds}, expected {case.num_leds} — "
            "test case mismatch"
        )
    if exp_bytes != case.num_leds * 3:
        return False, (
            f"firmware expected_bytes={exp_bytes}, expected {case.num_leds * 3}"
        )

    msg = (
        f"num_leds={num_leds}, exp_bytes={exp_bytes}, dec_bytes={dec_bytes}, "
        f"matched={matched}, mismatched={mismatched}, edges={edges}"
    )
    if not success or mismatched > 0:
        return False, msg
    return True, msg


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default=DEFAULT_PORT)
    p.add_argument("--baud", default=DEFAULT_BAUD, type=int)
    p.add_argument(
        "--tx-pin",
        default=10,
        type=int,
        help="LPC845 GPIO PIO0_n driven by bit-bang WS2812 TX (default: P0_10, "
        "must match the compile-time constant in the sketch handler)",
    )
    p.add_argument(
        "--rx-pin",
        default=11,
        type=int,
        help="LPC845 GPIO PIO0_n routed to SCT input 0 for capture (default: P0_11)",
    )
    p.add_argument(
        "--capture-ms",
        default=50,
        type=int,
        help="SCT capture window per test (default: 50 ms)",
    )
    args = p.parse_args()

    if serial is None:
        print(
            "ERROR: pyserial not installed. Run: uv pip install pyserial",
            file=sys.stderr,
        )
        return 2

    print(f"[lpc-ws2812-loopback] opening {args.port} @ {args.baud}")
    try:
        s = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"ERROR: could not open {args.port}: {e}", file=sys.stderr)
        return 2

    with s:
        s.dtr = False
        s.rts = False
        time.sleep(0.5)
        s.reset_input_buffer()
        time.sleep(2.0)
        s.reset_input_buffer()

        echo = send_rpc(s, "echo", args=[42], request_id=1, timeout=5.0)
        if not echo or echo.get("result") != 42:
            print(
                "ERROR: echo failed; is AutoResearchLpc flashed and running? "
                f"Got: {echo}",
                file=sys.stderr,
            )
            return 2
        print("[lpc-ws2812-loopback] echo ok")

        passes = 0
        fails = 0
        for i, case in enumerate(CASES, start=20):
            print(f"\n--- case {case.index} — {case.label} ---")
            resp = send_rpc(
                s,
                "ws2812SctTest",
                args=[case.index, args.tx_pin, args.rx_pin, args.capture_ms],
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
        if fails > 0:
            print(
                "\nNote: until the SCT→DMA RX upgrade lands (FastLED #3021 "
                "follow-up), polling-mode capture cannot keep up with the "
                "WS2812 800 kHz edge rate on M0+ — `mismatched > 0` is "
                "expected on real silicon for this orchestrator."
            )
        return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
