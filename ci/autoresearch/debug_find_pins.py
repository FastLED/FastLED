"""Direct test of findConnectedPins RPC -- isolate wrapper vs firmware.

If this script gets a response in <2s, the firmware path is healthy and
the wrapper hang is wrapper-side. If it hangs too, the firmware itself is
the wedge.

Device serial goes through fbuild's native (Rust) serial monitor via
`RpcBench`, never raw pyserial — see agents/docs/hardware-autoresearch.md
-> "Device serial".
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

from ci.autoresearch.rpc_bench import RpcBench  # noqa: E402
from ci.util.global_interrupt_handler import handle_keyboard_interrupt  # noqa: E402


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "COM20"
    print(f"[direct] connecting {port} (fbuild Rust serial)", flush=True)
    try:
        bench = RpcBench(port, timeout=12.0)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:  # noqa: BLE001
        print(f"[direct] could not connect to {port}: {e}", flush=True)
        return 1

    with bench:
        # ping — confirm the firmware answers at all.
        if bench.call("ping", args=[{}], timeout=5.0) is None:
            print("[direct] ping timed out — firmware-side wedge", flush=True)
            return 1

        # findConnectedPins — the call under test, with timing.
        t0 = time.time()
        print("[direct] TX: findConnectedPins(startPin=0, endPin=8)", flush=True)
        result = bench.call(
            "findConnectedPins",
            args=[{"startPin": 0, "endPin": 8, "autoApply": True}],
            timeout=10.0,
        )
        elapsed = time.time() - t0
        if result is None:
            print(
                f"\n[direct] NO RESPONSE in {elapsed:.3f}s -- firmware-side hang",
                flush=True,
            )
            return 1
        print(f"\n[direct] === RESPONSE in {elapsed:.3f}s ===\n{result}", flush=True)
        return 0


if __name__ == "__main__":
    sys.exit(main())
