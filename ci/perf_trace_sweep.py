#!/usr/bin/env python3
"""Drive an esp32p4 with the AutoResearch sketch and capture a PARLIO
performance-trace sweep via the fl::PerfTrace RPC.

Workflow (see docs/agents/docs/hardware-autoresearch.md):
    1. Connect to the device over JSON-RPC.
    2. setPerfTraceEnabled(true)         — flip runtime tracing on.
    3. runTest({drivers, laneRange, stripSizes}) — exercise the PARLIO
       hot path. Streaming "test_complete" indicates the show() calls
       finished.
    4. getPerfTraceLog()                  — pull the accumulated events
       (depth, dur_us, start_us, name, file, line).
    5. setPerfTraceEnabled(false)         — release.

The output is printed as a human-readable table and emitted as JSONL on
stdout so an enclosing CI step can post-process it for the
issue-2493 follow-up sweep.

Usage:
    uv run ci/perf_trace_sweep.py --port COM25 \
        --lanes 12 --strip-size 256

This script is intentionally minimal — it complements `bash autoresearch`
rather than replacing it. We use it because the perf trace flow is
orthogonal to the autoresearch pass/fail matrix and we want the raw event
log without the autoresearch streaming overhead.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from typing import Any

from ci.rpc_client import RpcClient
from ci.util.serial_interface import create_serial_interface


async def run_sweep(port: str, lanes: int, strip_size: int) -> dict[str, Any]:
    # Use pyserial backend so we don't need the fbuild monitor daemon.
    iface = create_serial_interface(port=port, baud_rate=115200, use_pyserial=True)
    client = RpcClient(port=port, baudrate=115200, timeout=20.0, serial_interface=iface)
    await client.connect()

    try:
        # 1. Turn capture on (compile-in flag already set in platformio.ini).
        on = await client.send("setPerfTraceEnabled", args=[True])
        if not on.get("success"):
            raise RuntimeError(f"setPerfTraceEnabled failed: {on}")

        # 2. Run a PARLIO test that exercises show().
        result = await client.send(
            "runTest",
            args=[
                {
                    "drivers": ["PARLIO"],
                    "laneRange": {"min": lanes, "max": lanes},
                    "stripSizes": [strip_size],
                }
            ],
            timeout=60.0,
        )

        # 3. Pull the trace log.
        log = await client.send("getPerfTraceLog", args=[])

        # 4. Disable capture (also clears the log on the device side).
        await client.send("setPerfTraceEnabled", args=[False])

        return {
            "config": {"lanes": lanes, "strip_size": strip_size},
            "run_test": result,
            "log": log,
        }
    finally:
        await client.close()


def print_event_table(events: list[list[Any]]) -> None:
    if not events:
        print("  (no events)")
        return
    print(
        f"  {'depth':>5}  {'dur_us':>9}  {'start_us':>12}  name                                @file:line"
    )
    for ev in events:
        depth, dur_us, start_us, name, file_, line = ev
        loc = f"{file_}:{line}"
        print(f"  {depth:>5}  {dur_us:>9}  {start_us:>12}  {name:<36}{loc}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM25", help="serial port (default COM25)")
    parser.add_argument("--lanes", type=int, default=12, help="number of PARLIO lanes")
    parser.add_argument("--strip-size", type=int, default=256, help="LEDs per lane")
    parser.add_argument(
        "--json", action="store_true", help="emit raw JSON instead of a table"
    )
    args = parser.parse_args()

    try:
        result = asyncio.run(run_sweep(args.port, args.lanes, args.strip_size))
    except Exception as exc:
        print(f"perf_trace_sweep failed: {exc}", file=sys.stderr)
        return 1

    if args.json:
        json.dump(result, sys.stdout, indent=2, default=str)
        sys.stdout.write("\n")
        return 0

    cfg = result["config"]
    print(f"PARLIO perf trace — lanes={cfg['lanes']} strip_size={cfg['strip_size']}")
    print()
    log = result.get("log", {})
    print(f"  count={log.get('count')} dropped={log.get('dropped')}")
    print_event_table(log.get("events", []))
    return 0


if __name__ == "__main__":
    sys.exit(main())
