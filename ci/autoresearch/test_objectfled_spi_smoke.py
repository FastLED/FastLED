"""#3428 bring-up smoke test for ObjectFLED DMA-bit-banged SPI on Teensy 4.x.

Invokes the `objectfledSpiSelfTest` RPC on the AutoResearch firmware to
exercise the full ObjectFLED-SPI hardware path -- pin lookup, init,
FlexPWM2-clocked DMA show, wait, deinit -- without needing a logic
analyzer or APA102 strip wired up. Passes if every stage returns success
and the peripheral driver completes within the bounded timeout.

This is NOT byte-level verification (still need a scope to confirm the
wire shows the expected bit pattern + the SCLK polarity), but it catches
the worst-case bugs: firmware crash on init, DMA hanging past its 50 ms
timeout, FlexPWM2 mis-programming, etc.

Usage:
    uv run python -m ci.autoresearch.test_objectfled_spi_smoke [--port COM20]

Pre-flight: a Teensy 4.x flashed with the AutoResearch sketch on the
named serial port. Typical:

    bash autoresearch teensy41 --skip-lint
    uv run python -m ci.autoresearch.test_objectfled_spi_smoke

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench  # noqa: E402
from ci.util.global_interrupt_handler import handle_keyboard_interrupt  # noqa: E402


# NOTE: pyserial import is deferred to main() so `test_*.py` discovery by
# pytest (or similar collectors) doesn't fail with sys.exit(1) at module
# load when pyserial isn't installed in the discovery environment.
# (CodeRabbit review on PR #3432.)


@dataclass(frozen=True)
class SmokeCase:
    label: str
    mosi_pin: int
    sclk_pin: int
    clock_hz: int
    num_bytes: int


# Default GPIO6 pin pairs on Teensy 4.x:
#   pin 14 -> GPIO6 bit 18
#   pin 15 -> GPIO6 bit 19
#   pin 16 -> GPIO6 bit 23
#   pin 17 -> GPIO6 bit 22
# All four are physically accessible on T4.0 and T4.1.
CASES = [
    SmokeCase("1 MHz @ 14/15 (4 bytes)", 14, 15, 1_000_000, 4),
    SmokeCase("6 MHz @ 14/15 (16 bytes)", 14, 15, 6_000_000, 16),
    SmokeCase("12 MHz @ 16/17 (64 bytes)", 16, 17, 12_000_000, 64),
]


def send_rpc(
    s: Any,
    method: str,
    args: dict[str, Any] | None = None,
    request_id: int | None = None,
    timeout: float = 10.0,
) -> dict[str, Any] | None:
    """Shim over RpcBench (`s`) — device serial via fbuild's Rust monitor,
    never raw pyserial (agents/docs/hardware-autoresearch.md). request_id
    is accepted for compatibility and ignored (RpcClient owns ids)."""
    _ = request_id
    result = s.call(method, args=args, timeout=timeout)
    if result is None:
        return None
    if result is METHOD_NOT_FOUND:
        return {"error": {"code": -32601, "message": "method not found"}}
    return {"result": result}


def evaluate(case: SmokeCase, result: dict[str, Any]) -> tuple[bool, str]:
    if not result.get("success"):
        return False, (
            f"FAILED — error={result.get('error', 'Unknown')} "
            f"message={result.get('message', '(none)')}"
        )
    if not result.get("lookup_ok"):
        return False, "FAILED — objectfled_spi_lookup_pins returned false"
    if not result.get("init_ok"):
        return False, "FAILED — objectfled_spi_init returned false"
    if not result.get("show_ok"):
        return False, "FAILED — objectfled_spi_show returned false"

    wait_ms = int(result.get("wait_ms", 0))
    if wait_ms > 100:
        return False, f"FAILED — wait_ms={wait_ms} exceeded 100 ms ceiling"

    expected_ms = max(1, (case.num_bytes * 8 * 1000) // case.clock_hz + 1)
    msg = (
        f"PASS  — pins MOSI={result.get('mosi_pin')}->bit{result.get('mosi_bit')} "
        f"SCLK={result.get('sclk_pin')}->bit{result.get('sclk_bit')} "
        f"@ {result.get('clock_hz')} Hz x {result.get('num_bytes')}B "
        f"wait_ms={wait_ms} (expected ~{expected_ms} ms)"
    )
    return True, msg


def main() -> int:
    doc = __doc__ or ""
    description = doc.splitlines()[0] if doc else ""
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--port", default="COM20", help="Serial port (default COM20)")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    print(f"[objectfled-spi-smoke] connecting {args.port} @ {args.baud} (Rust serial)")
    try:
        s = RpcBench(args.port)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:  # noqa: BLE001
        print(f"[objectfled-spi-smoke] could not connect to {args.port}: {e}")
        return 1

    ping = send_rpc(s, "ping", request_id=1)
    if not ping or "result" not in ping:
        print("[objectfled-spi-smoke] ping FAILED — firmware not responsive")
        return 1
    uptime = ping["result"].get("uptimeMs")
    print(f"[objectfled-spi-smoke] ping ok: uptime={uptime} ms")

    passes = 0
    fails = 0
    for i, case in enumerate(CASES, start=2):
        print(f"\n[objectfled-spi-smoke] case: {case.label}")
        rpc = send_rpc(
            s,
            "objectfledSpiSelfTest",
            args={
                "mosi_pin": case.mosi_pin,
                "sclk_pin": case.sclk_pin,
                "clock_hz": case.clock_hz,
                "num_bytes": case.num_bytes,
            },
            request_id=i,
            timeout=5.0,
        )
        if rpc is None or "result" not in rpc:
            print("  FAILED — no RPC response (timeout or firmware error)")
            fails += 1
            continue
        result = rpc["result"]
        ok, msg = evaluate(case, result)
        print(f"  {msg}")
        if not ok:
            print(f"  raw: {json.dumps(result, separators=(',', ':'))}")
        if ok:
            passes += 1
        else:
            fails += 1

    print(f"\n[objectfled-spi-smoke] summary: {passes} pass / {fails} fail")
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
