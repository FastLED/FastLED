"""Same as debug_object_fled_capture.py but with configurable lane size.

Used to isolate buffer-position-dependent decode failures in #3219:
- If laneSizes=[5] passes byte-level, the bug accumulates with frame
  length (DMA buffer overflow, sustained TX/RX skew, etc.).
- If laneSizes=[5] still fails, the bug fires every frame regardless
  of position.

Device serial goes through fbuild's native (Rust) serial monitor via
`RpcBench`, never raw pyserial — see agents/docs/hardware-autoresearch.md
-> "Device serial".
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


# Repo root on sys.path so `ci.*` imports resolve when run directly.
_REPO_ROOT = Path(__file__).resolve().parents[2]
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))

from ci.autoresearch.rpc_bench import RpcBench  # noqa: E402
from ci.util.global_interrupt_handler import handle_keyboard_interrupt  # noqa: E402


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="COM20")
    p.add_argument("--baud", default=115200, type=int)
    p.add_argument("--lane-size", default=5, type=int)
    p.add_argument("--wait-s", default=15.0, type=float)
    p.add_argument("--tx-pin", default=3, type=int)
    p.add_argument("--rx-pin", default=4, type=int)
    args = p.parse_args()

    print(
        f"[direct-small] {args.port} @ {args.baud}, laneSize={args.lane_size} "
        f"(fbuild Rust serial)",
        flush=True,
    )
    try:
        bench = RpcBench(args.port, timeout=max(15.0, args.wait_s))
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:  # noqa: BLE001
        print(f"[direct-small] could not connect to {args.port}: {e}", flush=True)
        return 1

    with bench:
        bench.call("setPins", args=[args.tx_pin, args.rx_pin], timeout=5.0)
        result = bench.call(
            "runSingleTest",
            args={
                "driver": "OBJECT_FLED",
                "laneSizes": [args.lane_size],
                "pattern": "MSB_LSB_A",
                "iterations": 1,
                "timing": "WS2812B-V5",
            },
            timeout=args.wait_s,
        )
        print(f"[direct-small] result: {result}", flush=True)
        return 0


if __name__ == "__main__":
    sys.exit(main())
