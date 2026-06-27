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
    uv run python ci/autoresearch/test_objectfled_spi_smoke.py [--port COM20]

Pre-flight: a Teensy 4.x flashed with the AutoResearch sketch on the
named serial port. Typical:

    bash autoresearch teensy41 --skip-lint
    uv run python ci/autoresearch/test_objectfled_spi_smoke.py

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
    import serial  # type: ignore[import-not-found]
except ImportError:
    print("[objectfled-spi-smoke] pyserial not installed; run `uv sync`")
    sys.exit(1)


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
    s: "serial.Serial",
    method: str,
    args: dict[str, Any] | None = None,
    request_id: int = 1,
    timeout: float = 10.0,
) -> dict[str, Any] | None:
    params = [args] if args is not None else [{}]
    req = {"method": method, "params": params, "id": request_id}
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
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", default="COM20", help="Serial port (default COM20)")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    print(f"[objectfled-spi-smoke] opening {args.port} @ {args.baud}")
    s = serial.Serial(args.port, args.baud, timeout=0.05)
    time.sleep(0.5)

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
