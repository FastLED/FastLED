"""#3428 bring-up smoke test for FlexIO2 SPI master mode on Teensy 4.x.

Invokes the `flexioSpiSelfTest` RPC on the AutoResearch firmware to
exercise the full FlexIO2 SPI hardware path -- pin lookup, init,
DMA-driven show, wait, deinit -- without needing a logic analyzer or
APA102 strip wired up. Passes if every stage returns success and the
peripheral driver completes within the bounded timeout.

This is NOT byte-level verification (you still need a scope to confirm
the wire shows 0xA5 MSB-first), but it catches the worst-case bugs:
firmware crash, init returning false, DMA hanging past its 50 ms
timeout, etc.

Usage:
    uv run python ci/autoresearch/test_flexio_spi_smoke.py [--port COM20]

The host MUST have a Teensy 4.x flashed with the AutoResearch sketch
already on the named serial port. Typical pre-flight:

    bash autoresearch teensy41 --skip-lint   # flash + verify boot
    uv run python ci/autoresearch/test_flexio_spi_smoke.py

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
    print("[flexio-spi-smoke] pyserial not installed; run `uv sync`")
    sys.exit(1)


@dataclass(frozen=True)
class SmokeCase:
    label: str
    mosi_pin: int
    sclk_pin: int
    clock_hz: int
    num_bytes: int


# Pin pairs chosen from kFlexIOPins. Defaults match the firmware
# `flexioSpiSelfTest` defaults (MOSI=10, SCLK=12 — FlexIO2 pins 0, 1).
# Three rates: 1 MHz (safe), 6 MHz (APA102 spec default), 12 MHz (SK9822 default).
CASES = [
    SmokeCase("1 MHz @ 10/12 (4 bytes)", 10, 12, 1_000_000, 4),
    SmokeCase("6 MHz @ 10/12 (16 bytes)", 10, 12, 6_000_000, 16),
    SmokeCase("12 MHz @ 11/13 (64 bytes)", 11, 13, 12_000_000, 64),
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
        return False, "FAILED — flexio_spi_lookup_pins returned false"
    if not result.get("init_ok"):
        return False, "FAILED — flexio_spi_init returned false"
    if not result.get("show_ok"):
        return False, "FAILED — flexio_spi_show returned false"

    wait_ms = int(result.get("wait_ms", 0))
    if wait_ms > 100:
        return False, f"FAILED — wait_ms={wait_ms} exceeded 100 ms ceiling"

    expected_ms = max(1, (case.num_bytes * 8 * 1000) // case.clock_hz + 1)
    msg = (
        f"PASS  — pins MOSI={result.get('mosi_pin')}->flex{result.get('mosi_flexio_pin')} "
        f"SCLK={result.get('sclk_pin')}->flex{result.get('sclk_flexio_pin')} "
        f"@ {result.get('clock_hz')} Hz x {result.get('num_bytes')}B "
        f"wait_ms={wait_ms} (expected ~{expected_ms} ms)"
    )
    return True, msg


def main() -> int:
    # __doc__ may be None if Python runs with -OO (docstrings stripped).
    doc = __doc__ or ""
    description = doc.splitlines()[0] if doc else ""
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--port", default="COM20", help="Serial port (default COM20)")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    print(f"[flexio-spi-smoke] opening {args.port} @ {args.baud}")
    s = serial.Serial(args.port, args.baud, timeout=0.05)
    time.sleep(0.5)  # let the line settle

    # Ping first -- if this fails the firmware is dead.
    ping = send_rpc(s, "ping", request_id=1)
    if not ping or "result" not in ping:
        print("[flexio-spi-smoke] ping FAILED — firmware not responsive")
        return 1
    uptime = ping["result"].get("uptimeMs")
    print(f"[flexio-spi-smoke] ping ok: uptime={uptime} ms")

    passes = 0
    fails = 0
    for i, case in enumerate(CASES, start=2):
        print(f"\n[flexio-spi-smoke] case: {case.label}")
        rpc = send_rpc(
            s,
            "flexioSpiSelfTest",
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
        print(f"  raw: {json.dumps(result, separators=(',', ':'))}")
        ok, msg = evaluate(case, result)
        print(f"  {msg}")
        if not ok:
            print(f"  raw: {json.dumps(result, separators=(',', ':'))}")
        if ok:
            passes += 1
        else:
            fails += 1

    print(f"\n[flexio-spi-smoke] summary: {passes} pass / {fails} fail")
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
