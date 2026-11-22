"""Host driver for the #2994 timing-drift reproducer (FastLED#3765).

`AutoResearchTimingDrift.h` has faithfully replayed the #2994 reporter's
sketch for some time -- 35-LED WS2812B ring on the legacy addLeds path,
`setMaxRefreshRate(800)`, millisDelay-gated 255-step fade, `delay(1000)`
between sequences -- and exposes it as the `timingDriftTest` JSON-RPC method.

It had no host driver. Nothing in `ci/` referenced `timingDriftTest` and there
was no CLI flag, so the reproducer had never actually been run. This is that
driver.

Two numbers matter, and the second is the one that settles #2994:

1. **Per-sequence wall time.** Theoretical period is 225 + 254*5 + 1000 =
   **2495 ms**. The reporter sees a rock-steady 2495 on 3.10.3 and 2563-2752
   on master. Index 0 is skipped: the first sequence is partial (no leading
   225 ms arm, no trailing 1000 ms delay) -- see the header of
   AutoResearchTimingDrift.h.

2. **`FastLED.show()` duration.** The firmware already reports min/max/total
   across every show() in the run. The reported drift is 68-257 ms over 254
   fade steps = **268-1012 us per step**, which is the same order as one
   WS2812 show() for 35 LEDs (~1050 us of wire time). If show() is
   meaningfully slower than that floor, that is the regression; a host-side
   profile (tests/profile/fastled_show.cpp) already established the CPU-side
   dispatch path costs only ~2 us, so the cost is in encoding, DMA, or a
   driver wait -- none of which are visible off-device.

Usage:
    uv run python -m ci.autoresearch.test_timing_drift --port COM9
    uv run python -m ci.autoresearch.test_timing_drift --port COM9 --iterations 20
    uv run python -m ci.autoresearch.test_timing_drift --port COM9 --api channel

`--api channel` registers the same strip with `FastLED.add()` instead of
`addLeds`, so the channel API's show() cost is measured too (FastLED#5399).

`--cold-warm` (FastLED#5422) times show() right after the firmware empties the
flash cache against an immediately following warm show(), once counting
retired instructions and once reading real cycles, and prints IPC for each:
    uv run python -m ci.autoresearch.test_timing_drift --port COM9 --cold-warm

Run it on master and on 3.10.3 and compare. Exits 0 when the run completes,
1 on a connection/RPC failure. It deliberately does NOT fail on drift: this is
a measurement tool, and the threshold for "too much drift" is the question
under investigation, not something to hardcode.
"""

from __future__ import annotations

import argparse
import sys
from typing import Any

from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# Theoretical sequence period from the reporter's sketch:
#   225 ms initial arm + 254 fade steps * 5 ms + 1000 ms inter-sequence delay
THEORETICAL_MS = 225 + 254 * 5 + 1000

# Wire time for one 35-LED WS2812 frame: 35 LEDs * 24 bits * 1.25 us.
# This is the floor show() cannot go below on this geometry; it is context for
# the show() numbers, not a pass/fail threshold.
WS2812_35_LED_WIRE_US = 35 * 24 * 1.25


def _call(bench: RpcBench, method: str, args: Any, timeout: float) -> Any:
    """Call an RPC method, returning the result dict or None."""
    result = bench.call(method, args=args, timeout=timeout)
    if result is None:
        return None
    if result is METHOD_NOT_FOUND:
        print(
            f"ERROR: firmware does not expose '{method}'. Is this an "
            f"AutoResearch build recent enough to include "
            f"AutoResearchTimingDrift.h?",
            file=sys.stderr,
        )
        return None
    return result


def _summarize_sequences(iter_ms: list[int]) -> None:
    """Print the per-sequence histogram against the 2495 ms theoretical."""
    # Index 0 is a partial sequence by construction -- skip it.
    usable = iter_ms[1:]
    if not usable:
        print("  (only one sequence recorded; run with --iterations >= 3)")
        return

    lo = min(usable)
    hi = max(usable)
    mean = sum(usable) / len(usable)

    print(f"  theoretical      : {THEORETICAL_MS} ms")
    print(f"  sequences        : {len(usable)} (index 0 skipped, partial)")
    print(f"  min / mean / max : {lo} / {mean:.1f} / {hi} ms")
    print(
        f"  drift vs theory  : {lo - THEORETICAL_MS:+d} .. {hi - THEORETICAL_MS:+d} ms"
    )
    print()
    print("  per-sequence:")
    for i, ms in enumerate(usable, start=1):
        delta = ms - THEORETICAL_MS
        bar = "#" * min(60, max(0, delta // 5)) if delta > 0 else ""
        print(f"    [{i:2d}] {ms:5d} ms  {delta:+5d}  {bar}")


def _summarize_show(result: dict[str, Any]) -> None:
    """Print show() duration stats -- the decisive number for #2994."""
    count = result.get("show_count")
    total = result.get("show_total_us")
    lo = result.get("show_min_us")
    hi = result.get("show_max_us")

    if not count or total is None:
        print("  (firmware reported no show() timing)")
        return

    mean = total / count
    print(f"  show() calls     : {count}")
    print(f"  min / mean / max : {lo} / {mean:.1f} / {hi} us")
    print(
        f"  WS2812 wire floor: {WS2812_35_LED_WIRE_US:.0f} us (35 LEDs x 24 bits x 1.25 us)"
    )
    if lo is not None and lo > 0:
        print(f"  min / wire floor : {lo / WS2812_35_LED_WIRE_US:.2f}x")
    instr_count = result.get("show_instr_count")
    instr_total = result.get("show_instr_total")
    if instr_count and instr_total is not None:
        print(f"  instr per show() : {instr_total / instr_count:.0f}")
    print()
    print("  #2994 reports 268-1012 us of excess per fade step. If show()'s")
    print("  mean sits well above the wire floor, that excess is show() itself.")


def _mean(total: Any, count: Any) -> float | None:
    if not count or total is None:
        return None
    return total / count


def _fmt(value: float | None, spec: str) -> str:
    return "-" if value is None else format(value, spec)


def summarize_cold_warm(
    instr_result: dict[str, Any], cycle_result: dict[str, Any]
) -> list[str]:
    """Rows for the #5422 cold-vs-warm table: instructions, cycles, IPC, us.

    Instructions come from the counting run and cycles from the counter-off
    run (the two share one hardware counter). Mean us is from the cycle run,
    which carries no counter bookkeeping.
    """
    lines = [
        "| show() | instructions | cycles | IPC | mean us |",
        "|---|---:|---:|---:|---:|",
    ]
    for side in ("cold", "warm"):
        instr = _mean(
            instr_result.get(f"{side}_instr_total"), instr_result.get(f"{side}_count")
        )
        cycles = _mean(
            cycle_result.get(f"{side}_cycle_total"), cycle_result.get(f"{side}_count")
        )
        us = _mean(
            cycle_result.get(f"{side}_total_us"), cycle_result.get(f"{side}_count")
        )
        ipc = instr / cycles if instr is not None and cycles else None
        lines.append(
            f"| {side} | {_fmt(instr, '.0f')} | {_fmt(cycles, '.0f')} | "
            f"{_fmt(ipc, '.2f')} | {_fmt(us, '.1f')} |"
        )
    return lines


def _run_cold_warm(bench: RpcBench, args: argparse.Namespace) -> int:
    """Run both counter modes of the #5422 cold-vs-warm probe and print a table."""
    results: dict[bool, dict[str, Any]] = {}
    for count_instructions in (True, False):
        result = _call(
            bench,
            "timingDriftTest",
            {
                "pin": args.pin,
                "numLeds": args.num_leds,
                "api": args.api,
                "coldWarm": True,
                "repeats": args.repeats,
                "order": args.order,
                "countInstructions": count_instructions,
            },
            timeout=60.0,
        )
        if not result or not result.get("success", False):
            print(f"ERROR: cold-warm run failed: {result}", file=sys.stderr)
            return 1
        if result.get("ready_timeouts"):
            print(
                f"WARNING: {result['ready_timeouts']} READY waits timed out",
                file=sys.stderr,
            )
        results[count_instructions] = result
    print(
        f"CPU {results[False].get('cpu_mhz')} MHz, leds={args.num_leds}, "
        f"api={args.api}, order={args.order}, repeats={args.repeats}, cache flush="
        f"{results[False].get('cache_flush_supported')}"
    )
    for line in summarize_cold_warm(results[True], results[False]):
        print(line)
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        description="Run the #2994 timing-drift reproducer on an attached board."
    )
    p.add_argument("--port", default="COM9", help="Serial port (default: COM9)")
    p.add_argument("--pin", default=4, type=int, help="LED data pin (default: 4)")
    p.add_argument(
        "--num-leds",
        default=35,
        type=int,
        help="Strip length (default: 35, #2994 geometry)",
    )
    p.add_argument(
        "--iterations",
        default=10,
        type=int,
        help="Sequences to run; each takes ~2.5 s (default: 10)",
    )
    p.add_argument(
        "--api",
        default="legacy",
        choices=("legacy", "channel"),
        help="Register the strip with addLeds (legacy) or FastLED.add() (channel)",
    )
    p.add_argument(
        "--cold-warm",
        action="store_true",
        help="Time cache-evicted vs warm show() pairs instead of the fade (#5422)",
    )
    p.add_argument(
        "--repeats",
        default=32,
        type=int,
        help="Cold/warm pairs per counter mode with --cold-warm (default: 32)",
    )
    p.add_argument(
        "--order",
        default="grb",
        choices=("grb", "rgb"),
        help="With --cold-warm --api channel: colour order; rgb skips the reorder pass (#5378)",
    )
    args = p.parse_args(argv)

    print(f"[timing-drift] opening {args.port}")
    try:
        bench = RpcBench(args.port)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:  # noqa: BLE001
        print(f"Could not connect to {args.port}: {e}", file=sys.stderr)
        return 1

    with bench:
        ping = _call(bench, "ping", None, timeout=5.0)
        if not ping:
            print(
                "ERROR: ping failed; is AutoResearch flashed and running?",
                file=sys.stderr,
            )
            return 1
        print(f"[timing-drift] ping ok: uptime={ping.get('uptimeMs')} ms")

        if args.cold_warm:
            return _run_cold_warm(bench, args)

        # Each sequence is ~2.5 s of device-side blocking, plus headroom.
        timeout = args.iterations * 3.0 + 30.0
        print(
            f"[timing-drift] running {args.iterations} sequences "
            f"(~{args.iterations * 2.5:.0f}s device-side), pin={args.pin}, "
            f"leds={args.num_leds}"
        )

        result = _call(
            bench,
            "timingDriftTest",
            {
                "pin": args.pin,
                "numLeds": args.num_leds,
                "iterations": args.iterations,
                "api": args.api,
            },
            timeout=timeout,
        )
        if not result:
            print("ERROR: timingDriftTest returned no result", file=sys.stderr)
            return 1

        if not result.get("success", False):
            print(
                f"ERROR: firmware reported failure: "
                f"{result.get('error')} - {result.get('message')}",
                file=sys.stderr,
            )
            return 1

        print()
        print("=" * 62)
        print(
            f"CPU: {result.get('cpu_mhz')} MHz   pin={result.get('pin')}   "
            f"leds={result.get('num_leds')}   api={result.get('api', 'legacy')}"
        )
        print("=" * 62)
        print()
        print("--- per-sequence wall time (#2994's serial output) ---")
        _summarize_sequences(list(result.get("iter_ms") or []))
        print()
        print("--- FastLED.show() duration (the decisive number) ---")
        _summarize_show(result)
        print()

    return 0


if __name__ == "__main__":
    sys.exit(main())
