"""Direct serial monitor for OBJECT_FLED runSingleTest on Teensy 4.

Bypasses the autoresearch wrapper's serial filtering so we see every
FL_WARN line the device emits during the test. Sends runSingleTest
synchronously and firehose-dumps everything to stdout.

Usage:
    uv run python ci/autoresearch/debug_object_fled_capture.py --port COM20

Goal: characterize where runSingleTest stalls on Teensy 4 after the
dual-circuit FlexPWM RX fix from PR #3222 (issue #3219).

Device serial goes through fbuild's native (Rust) serial monitor, never
raw pyserial — see agents/docs/hardware-autoresearch.md -> "Device
serial". This is a raw firehose logger, so it drives the SerialInterface
(write + read_lines) directly rather than the RPC-oriented RpcBench.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
import time
from pathlib import Path


# Repo root on sys.path so `ci.*` imports resolve when run directly.
_REPO_ROOT = Path(__file__).resolve().parents[2]
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))

from ci.util.serial_interface import create_serial_interface  # noqa: E402


async def _run(args: argparse.Namespace) -> int:
    print(
        f"[direct] connecting {args.port} @ {args.baud} (fbuild Rust serial)",
        flush=True,
    )
    iface = create_serial_interface(args.port, baud_rate=args.baud)
    await iface.connect()
    await iface.reset_device(None)  # DTR/RTS reset, transport-appropriate

    # Set pins explicitly so we don't rely on prior findConnectedPins.
    set_pins = {
        "jsonrpc": "2.0",
        "method": "setPins",
        "params": [[args.tx_pin, args.rx_pin]],
        "id": 1,
    }
    await iface.write(json.dumps(set_pins, separators=(",", ":")) + "\n")

    run_single = {
        "jsonrpc": "2.0",
        "method": "runSingleTest",
        "params": [
            {
                "driver": "OBJECT_FLED",
                "laneSizes": [100],
                "pattern": "MSB_LSB_A",
                "iterations": 1,
                "timing": "WS2812B-V5",
            }
        ],
        "id": 2,
    }
    req = json.dumps(run_single, separators=(",", ":")) + "\n"
    print(f"[direct] TX: {req.strip()}", flush=True)
    await iface.write(req)

    # Firehose-log every line the device emits for wait_s seconds.
    deadline = time.time() + args.wait_s
    t0 = time.time()
    line_count = 0
    while time.time() < deadline:
        async for line in iface.read_lines(timeout=1.0):
            print(f"[{time.time() - t0:6.2f}] {line.rstrip()}", flush=True)
            line_count += 1
        # read_lines yields until its per-batch timeout; loop until wait_s.

    print(
        f"\n[direct] window closed after {args.wait_s:.1f}s, captured {line_count} lines",
        flush=True,
    )
    return 0


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="COM20")
    p.add_argument("--baud", default=115200, type=int)
    p.add_argument(
        "--wait-s", default=30.0, type=float, help="seconds to log after sending RPC"
    )
    p.add_argument(
        "--tx-pin", default=3, type=int, help="TX pin (must match jumper wire)"
    )
    p.add_argument(
        "--rx-pin", default=4, type=int, help="RX pin (must match jumper wire)"
    )
    args = p.parse_args()
    return asyncio.run(_run(args))


if __name__ == "__main__":
    sys.exit(main())
