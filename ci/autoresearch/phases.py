"""Phase functions for the autoresearch pipeline.

Each phase is an async function that takes RunContext and returns
None (success) or int (exit code on failure).
"""

from __future__ import annotations

import asyncio
import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any, TypeGuard

from colorama import Fore, Style

from ci.autoresearch.args import Args
from ci.autoresearch.build_driver import select_build_driver
from ci.autoresearch.context import (
    DEFAULT_EXPECT_PATTERNS,
    DEFAULT_FAIL_ON_PATTERN,
    EXIT_ON_ERROR_PATTERNS,
    QuietContext,
    RunContext,
    default_pins_for_environment,
    display_objectfled_diagnostics,
    display_pattern_details,
    display_tight_timing,
)
from ci.autoresearch.gpio import (
    run_gpio_pretest,
    run_pin_discovery,
    run_pin_discovery_segmented,
)
from ci.debug_attached import run_cpp_lint
from ci.rpc_client import RpcClient, RpcCrashError, RpcTimeoutError
from ci.util.blocker_alert import blocker_alert
from ci.util.crash_trace_decoder import CrashTraceDecoder
from ci.util.global_interrupt_handler import (
    handle_keyboard_interrupt,
    is_interrupted,
)
from ci.util.json_rpc_handler import parse_json_rpc_commands
from ci.util.port_utils import (
    auto_detect_upload_port,
    detect_attached_chip,
    environment_has_wifi,
    kill_port_users,
)
from ci.util.sketch_resolver import parse_timeout


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface


# ============================================================
# Helpers
# ============================================================

# Grace period added on top of the user's --timeout before the wrapper-level
# watchdog force-terminates. The deadline_epoch in RunContext is the user's
# hard wall; this is the additional headroom for any in-flight RPC to
# unwind cleanly before we kill the process. Per session 2026-06-22 spec.
WATCHDOG_GRACE_SECONDS = 20.0
FLEX_IO_TEENSY_DEFAULT_TX_PIN = 6


def _is_teensy4_environment(final_environment: str | None) -> bool:
    return final_environment is not None and final_environment.lower() in (
        "teensy40",
        "teensy41",
    )


def _driver_name_for_environment(driver: str, final_environment: str | None) -> str:
    if driver == "SPI" and _is_teensy4_environment(final_environment):
        return "SPI_UNIFIED"
    return driver


def _uses_teensy_flex_io_default_tx(
    args: Args, final_environment: str, drivers: list[str]
) -> bool:
    return (
        args.tx_pin is None
        and final_environment in {"teensy40", "teensy41"}
        and "FLEX_IO" in drivers
    )


def _autoresearch_watchdog_thread(
    ctx: "RunContext", cancel_event: "threading.Event"
) -> None:
    """Force-terminate the process if --timeout + grace expires.

    Runs on a daemon `threading.Thread` -- deliberately NOT on the
    asyncio event loop, because the failure modes we're guarding
    against (wedged DMA, hung serial monitor, blocking call that never
    yields control to the loop) can starve the loop's coroutines. A
    real OS thread keeps ticking even when the loop is dead.

    Steps:
      1. Sleeps until `deadline_epoch + WATCHDOG_GRACE_SECONDS`,
         in 1 s steps so a cancelled run can still bail quickly.
      2. Emits a red `blocker_alert` banner with the trigger reason.
      3. Best-effort closes the serial port from a side-thread with a
         bounded join so the close itself can't hang us.
      4. `os._exit(2)` -- bypasses any stuck asyncio cleanup. Clean
         exit would be preferable, but the whole point of this
         watchdog is to escape cases where clean exit doesn't happen.
    """
    assert ctx.deadline_epoch is not None, "watchdog started without deadline_epoch set"

    # Re-read `ctx.deadline_epoch` every iteration so that
    # `extend_autoresearch_watchdog_deadline()` can grant additional
    # runway to long-running cleanup paths (e.g. an `await client.close()`
    # that legitimately needs a few seconds) WITHOUT disarming the
    # safety net. If close() itself wedges, the watchdog still fires
    # after the new (extended) deadline.
    while True:
        now = time.monotonic()
        fire_at = ctx.deadline_epoch + WATCHDOG_GRACE_SECONDS
        if now >= fire_at:
            break
        if cancel_event.wait(min(fire_at - now, 1.0)):
            return  # normal shutdown: caller signaled cancel

    total = ctx.timeout_seconds + WATCHDOG_GRACE_SECONDS
    blocker_alert(
        f"AUTORESEARCH WATCHDOG: --timeout ({ctx.timeout_seconds:.0f}s) + "
        f"{WATCHDOG_GRACE_SECONDS:.0f}s grace exceeded -- force-terminating.",
        details=[
            f"total wall-clock budget: {total:.0f}s",
            "the per-RPC deadline path did not unwind in time",
            "best-effort closing serial port before exit",
            f"upload_port={ctx.upload_port!r}",
        ],
    )

    # Forensic stack dump across every live Python thread (top 4 frames
    # each) so we can post-mortem WHERE the wedge happened. Walks
    # `sys._current_frames()` directly rather than using
    # `faulthandler.dump_traceback()` so we control the depth and have
    # the per-thread name in the header. Output goes to stderr right
    # next to the red banner; piped logs keep the dump in order.
    sys.stderr.write("\n--- WATCHDOG STACK DUMP (top 4 frames per thread) ---\n")
    threads_by_id = {t.ident: t for t in threading.enumerate()}
    for tid, frame in sys._current_frames().items():
        thread_obj = threads_by_id.get(tid)
        name = thread_obj.name if thread_obj is not None else "?"
        daemon_marker = (
            " [daemon]" if thread_obj is not None and thread_obj.daemon else ""
        )
        sys.stderr.write(f"\nThread {name!r} (tid={tid}){daemon_marker}:\n")
        try:
            import traceback as _traceback

            stack_frames = _traceback.extract_stack(frame, limit=4)
            for entry in _traceback.format_list(stack_frames):
                sys.stderr.write(entry)
        except KeyboardInterrupt as ki:  # noqa: BLE001
            handle_keyboard_interrupt(ki)
            raise
        except Exception as dump_exc:  # never let the dump itself kill us
            sys.stderr.write(f"  <stack dump failed: {dump_exc}>\n")
    sys.stderr.write("--- END WATCHDOG STACK DUMP ---\n\n")

    # Best-effort serial close -- run in yet another thread so a wedged
    # close() can't keep us from exiting. 2 s join cap, then we exit.
    serial_iface = ctx.serial_iface

    def _close_target() -> None:
        if serial_iface is None:
            return
        # The async adapter wraps a sync fbuild SerialMonitor; reach for
        # the sync underlying object first since we're off the loop.
        # We swallow ALL exceptions here -- we're about to os._exit and
        # the caller cannot interrupt us via Ctrl-C anyway (watchdog
        # thread does not own the main-thread interrupt handler).
        try:
            mon = getattr(serial_iface, "_monitor", None)
            if mon is not None and hasattr(mon, "__exit__"):
                mon.__exit__(None, None, None)
                return
        except KeyboardInterrupt as ki:  # noqa: BLE001
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            pass
        try:
            close = getattr(serial_iface, "close", None)
            if callable(close):
                close()  # may return a coroutine; we ignore it
        except KeyboardInterrupt as ki:  # noqa: BLE001
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            pass

    closer = threading.Thread(
        target=_close_target,
        name="autoresearch-watchdog-closer",
        daemon=True,
    )
    closer.start()
    closer.join(timeout=2.0)

    sys.stderr.flush()
    os._exit(2)


def start_autoresearch_watchdog(ctx: "RunContext") -> None:
    """Spawn the watchdog daemon thread. Call once per run, after deploy.

    Uses a real OS thread (not an asyncio task) so a wedged event loop
    can't disable the safety net. The cancel signal is a
    `threading.Event` stored on `ctx._watchdog_task`. Clean-exit paths
    do NOT disarm the watchdog -- if cleanup itself wedges we still
    want the force-kill. They call
    `extend_autoresearch_watchdog_deadline(ctx, N)` to grant the
    cleanup phase additional runway without disarming the bomb.
    """
    if ctx._watchdog_task is not None:
        return  # already running
    cancel_event = threading.Event()
    ctx._watchdog_task = cancel_event
    thread = threading.Thread(
        target=_autoresearch_watchdog_thread,
        args=(ctx, cancel_event),
        name="autoresearch-watchdog",
        daemon=True,
    )
    thread.start()


def extend_autoresearch_watchdog_deadline(
    ctx: "RunContext", extra_seconds: float
) -> None:
    """Push the watchdog deadline forward by `extra_seconds`.

    Use this around long-running cleanup work that legitimately needs
    more time than the user's `--timeout` allows (e.g. an `await
    client.close()` that may take a couple seconds). The watchdog
    stays armed; if the cleanup itself wedges past the new deadline
    plus `WATCHDOG_GRACE_SECONDS`, it still force-terminates.

    This is preferred over a `stop_autoresearch_watchdog` because
    disabling the bomb during cleanup risks an indefinite hang if
    cleanup itself is the wedge.
    """
    if ctx.deadline_epoch is None:
        return
    ctx.deadline_epoch += extra_seconds


MAX_AUTORESEARCH_LANES = 16
LPC_BRING_UP_ENVS = {
    "lpc845brk",
    "lpc845",
    "lpcxpresso845max",
    "lpcxpresso804",
}
LPC_WS2812_ENVS = {"lpc845brk", "lpc845", "lpcxpresso845max"}
LPC_IEEE754_BUILD_ENVS = {
    "lpc845brk": "lpc845brk_ieee754",
}


def _is_native_platform(environment: str | None) -> bool:
    """Check if the target is the native/stub (host) platform."""
    if not environment:
        return False
    return environment.lower() in ("native", "stub", "host")


def _build_environment_for_mode(ctx: RunContext) -> str | None:
    """Return the PlatformIO/fbuild environment to compile for this run mode."""
    environment = ctx.final_environment
    if not environment:
        return None
    if ctx.ieee754_test_mode:
        return LPC_IEEE754_BUILD_ENVS.get(environment.lower(), environment)
    return environment


async def _run_native_autoresearch(args: Args, build_mode: str = "quick") -> int:
    """Run autoresearch on native/stub platform via Meson compile + execute."""
    from ci.util.meson_example_runner import run_meson_examples

    project_dir = args.project_dir.resolve()
    build_dir = project_dir / ".build" / "meson"

    print("\u2500" * 60)
    print("FastLED AutoResearch \u2014 Native Platform")
    print("\u2500" * 60)
    print(f"  Mode        Compile + Run (Meson, {build_mode})")
    print(f"  Build dir   {project_dir / '.build' / f'meson-{build_mode}'}")
    print("\u2500" * 60)
    print()

    if not args.skip_lint:
        if not run_cpp_lint():
            print("\n\u274c Linting failed. Fix issues or use --skip-lint to bypass.")
            return 1
    else:
        print("\u26a0\ufe0f  Skipping C++ linting (--skip-lint flag set)\n")

    print("=" * 60)
    print("COMPILE + EXECUTE (Meson \u2014 native host)")
    print("=" * 60)

    result = run_meson_examples(
        source_dir=project_dir,
        build_dir=build_dir,
        examples=["AutoResearch"],
        verbose=args.verbose,
        build_mode=build_mode,
        full=True,
    )

    print()
    print("=" * 60)
    if result.success:
        print(f"{Fore.GREEN}\u2713 NATIVE AUTORESEARCH SUCCEEDED{Style.RESET_ALL}")
    else:
        print(f"{Fore.RED}\u2717 NATIVE AUTORESEARCH FAILED{Style.RESET_ALL}")
    print("=" * 60)

    return 0 if result.success else 1


# ============================================================
# Phase A: Parse args and build commands
# ============================================================


def _is_teensy_environment(environment: str | None) -> bool:
    return environment is not None and environment.lower() in ("teensy40", "teensy41")


def _reject_teensy_root_platformio_ini(environment: str | None) -> bool:
    if not _is_teensy_environment(environment):
        return False
    print(
        f"{Fore.RED}❌ Error: --use-root-platformio-ini is not allowed for "
        f"Teensy AutoResearch acceptance. Use the synthesized fbuild project "
        f"from ci/boards.py instead.{Style.RESET_ALL}"
    )
    return True


def _parse_args_and_build_commands(args: Args) -> RunContext | int:
    """Parse CLI args, validate modes, build JSON-RPC command list.

    Pure computation — no hardware I/O.
    Returns RunContext on success, int exit code on validation failure.
    """
    final_environment = args.environment_positional or args.environment

    if args.lcd and not final_environment:
        final_environment = "esp32s3"
        print(
            "\u2139\ufe0f  --lcd flag requires ESP32-S3, auto-selecting 'esp32s3' environment"
        )

    if args.lcd_spi and not final_environment:
        final_environment = "esp32s3"
        print(
            "\u2139\ufe0f  --lcd-spi flag requires ESP32-S3, auto-selecting 'esp32s3' environment"
        )

    if args.lcd_rgb and not final_environment:
        final_environment = "esp32p4"
        print(
            "\u2139\ufe0f  --lcd-rgb flag requires ESP32-P4, auto-selecting 'esp32p4' environment"
        )

    # Driver selection
    drivers: list[str] = []
    simd_test_mode = args.simd
    coroutine_test_mode = args.coroutine
    ieee754_test_mode = args.ieee754

    # Parse --wave2d-perf "<W>x<H>" — None disables the mode.
    # Cf. #3124 for the planned --perf-XX convention rename.
    wave2d_perf_grid: tuple[int, int] | None = None
    if args.wave2d_perf is not None:
        spec = str(args.wave2d_perf).lower().strip()
        try:
            if "x" not in spec:
                raise ValueError("expected 'WxH' form")
            w_str, h_str = spec.split("x", 1)
            w_val = int(w_str)
            h_val = int(h_str)
            if not (4 <= w_val <= 1024 and 4 <= h_val <= 1024):
                raise ValueError("W and H must be in [4, 1024]")
            wave2d_perf_grid = (w_val, h_val)
        except ValueError as exc:
            print(
                f"{Fore.RED}❌ --wave2d-perf: invalid grid spec "
                f"{args.wave2d_perf!r}: {exc}{Style.RESET_ALL}",
                file=sys.stderr,
            )
            sys.exit(2)

    is_teensy4 = _is_teensy4_environment(final_environment)
    is_teensy_specific_driver = args.object_fled or args.flex_io or args.lpuart

    if args.use_root_platformio_ini and (is_teensy4 or is_teensy_specific_driver):
        print(
            f"{Fore.RED}❌ Error: --use-root-platformio-ini is not allowed for "
            f"Teensy AutoResearch acceptance. Use the synthesized fbuild project "
            f"from ci/boards.py instead.{Style.RESET_ALL}"
        )
        return 1

    if args.all and is_teensy4:
        drivers = [
            "OBJECT_FLED",
            "FLEX_IO",
        ]
    elif args.all:
        drivers = [
            "PARLIO",
            "RMT",
            "SPI",
            "UART",
            "LCD_CLOCKLESS",
            "LCD_SPI",
            "LCD_RGB",
            "OBJECT_FLED",
            "FLEX_IO",
        ]
    else:
        if args.parlio:
            drivers.append("PARLIO")
        if args.rmt:
            drivers.append("RMT")
        if args.spi:
            drivers.append(_driver_name_for_environment("SPI", final_environment))
        if args.uart:
            drivers.append("UART")
        if args.lcd:
            drivers.append("LCD_CLOCKLESS")
        if args.lcd_spi:
            drivers.append("LCD_SPI")
        if args.lcd_rgb:
            drivers.append("LCD_RGB")
        if args.object_fled:
            drivers.append("OBJECT_FLED")
        if args.flex_io:
            drivers.append("FLEX_IO")
        if args.lpuart:
            drivers.append("LPUART")

    parallel_mode = args.parallel
    if parallel_mode:
        if len(drivers) < 2:
            print(
                f"{Fore.RED}\u274c Error: --parallel requires at least 2 drivers (e.g., --parlio --lcd-rgb --parallel){Style.RESET_ALL}"
            )
            return 1
        print(
            f"\n{Fore.CYAN}\u2139\ufe0f  Parallel mode: testing {', '.join(drivers)} simultaneously{Style.RESET_ALL}"
        )

    net_server_mode = args.net_server
    net_client_mode = args.net_client
    net_loopback_mode = args.net
    ota_mode = args.ota
    ble_mode = args.ble
    decode_mode = args.decode is not None

    # Short-circuit validation for --decode
    if decode_mode:
        any_incompatible_mode = (
            bool(drivers)
            or simd_test_mode
            or coroutine_test_mode
            or ieee754_test_mode
            or net_server_mode
            or net_client_mode
            or net_loopback_mode
            or ota_mode
            or ble_mode
        )
        if any_incompatible_mode:
            print(
                f"{Fore.RED}\u274c Error: --decode cannot be combined with driver flags, --simd, --coroutine, --net, --ota, or --ble{Style.RESET_ALL}"
            )
            return 1

    # Validate mutual exclusivity
    if (
        net_server_mode or net_client_mode or net_loopback_mode or ota_mode or ble_mode
    ) and (drivers or simd_test_mode or coroutine_test_mode or ieee754_test_mode):
        print(
            f"{Fore.RED}\u274c Error: --net/--net-server/--net-client/--ota/--ble cannot be combined with driver flags, --simd, or --coroutine{Style.RESET_ALL}"
        )
        return 1
    net_mode_count = sum([net_server_mode, net_client_mode, net_loopback_mode])
    if net_mode_count > 1:
        print(
            f"{Fore.RED}\u274c Error: --net, --net-server, and --net-client are mutually exclusive{Style.RESET_ALL}"
        )
        return 1
    if ota_mode and net_mode_count > 0:
        print(
            f"{Fore.RED}\u274c Error: --ota cannot be combined with --net/--net-server/--net-client{Style.RESET_ALL}"
        )
        return 1
    if ble_mode and (net_mode_count > 0 or ota_mode):
        print(
            f"{Fore.RED}\u274c Error: --ble cannot be combined with --net/--net-server/--net-client/--ota{Style.RESET_ALL}"
        )
        return 1

    gpio_only_mode = (
        not drivers
        and not simd_test_mode
        and not coroutine_test_mode
        and not ieee754_test_mode
        and not net_server_mode
        and not net_client_mode
        and not net_loopback_mode
        and not ota_mode
        and not ble_mode
    )
    if gpio_only_mode:
        print(
            f"\n{Fore.CYAN}\u2139\ufe0f  No driver specified \u2014 running GPIO-only mode (pin discovery + toggle capture test){Style.RESET_ALL}"
        )

    # Parse --lanes argument
    min_lanes: int | None = None
    max_lanes: int | None = None

    if args.lanes:
        if "-" in args.lanes:
            parts = args.lanes.split("-", 1)
            try:
                min_lanes = int(parts[0])
                max_lanes = int(parts[1])
            except ValueError:
                print(
                    f"\u274c Error: Invalid lane range '{args.lanes}' (expected format: '1-4')"
                )
                return 1
        else:
            try:
                lane_count = int(args.lanes)
                min_lanes = lane_count
                max_lanes = lane_count
            except ValueError:
                print(
                    f"\u274c Error: Invalid lane count '{args.lanes}' (expected integer)"
                )
                return 1

    if args.legacy:
        requested_min_lanes = min_lanes if min_lanes is not None else 1
        requested_max_lanes = max_lanes if max_lanes is not None else 1
        if args.legacy_mixed_timings and requested_max_lanes < 2:
            print(
                "\u274c Error: --legacy-mixed-timings requires --legacy with at least 2 lanes"
            )
            return 1
        if args.legacy_rgbw_small_counts:
            if requested_min_lanes != 1 or requested_max_lanes != 1:
                print(
                    "\u274c Error: --legacy-rgbw-small-counts requires exactly 1 lane"
                )
                return 1
            if args.strip_sizes is not None:
                print(
                    "\u274c Error: --legacy-rgbw-small-counts supplies strip sizes 1,2,3,4; do not combine with --strip-sizes"
                )
                return 1
        if requested_max_lanes > 1:
            if args.tx_pin is None:
                print(
                    "\u274c Error: --legacy multi-lane requires explicit --tx-pin in the historical 0-8 template range"
                )
                return 1
            max_pin = args.tx_pin + requested_max_lanes - 1
            if args.tx_pin < 0 or max_pin > 8:
                print(
                    "\u274c Error: --legacy multi-lane supports consecutive TX pins 0-8 only; pin 22 is single-lane for the current ObjectFLED loopback"
                )
                return 1
        min_lanes = requested_min_lanes
        max_lanes = requested_max_lanes
        print(
            "\u2139\ufe0f  Legacy API mode: using WS2812B<PIN> template path (supported TX pins 0-8; pin 22 single-lane current loopback)"
        )
        if args.legacy_mixed_timings:
            print(
                "\u2139\ufe0f  Legacy mixed timing mode: alternating WS2812B/SK6812 template chipsets across lanes"
            )
        if args.legacy_rgbw_small_counts:
            print(
                "\u2139\ufe0f  Legacy RGBW small-count mode: running RGBW strip sizes 1, 2, 3, and 4"
            )
    elif args.legacy_mixed_timings or args.legacy_rgbw_small_counts:
        flag = (
            "--legacy-mixed-timings"
            if args.legacy_mixed_timings
            else "--legacy-rgbw-small-counts"
        )
        print(f"\u274c Error: {flag} requires --legacy")
        return 1

    if min_lanes is not None and max_lanes is not None:
        if min_lanes < 1 or max_lanes < 1 or min_lanes > max_lanes:
            print(
                f"\u274c Error: Invalid lane range {min_lanes}-{max_lanes} (expected 1-{MAX_AUTORESEARCH_LANES})"
            )
            return 1
        if max_lanes > MAX_AUTORESEARCH_LANES:
            print(
                f"\u274c Error: Lane count must be 1-{MAX_AUTORESEARCH_LANES}, got max {max_lanes}"
            )
            return 1

    # Parse --lane-counts argument
    per_lane_counts: list[int] | None = None
    if args.lane_counts:
        try:
            per_lane_counts = [int(c.strip()) for c in args.lane_counts.split(",")]
            if not per_lane_counts:
                print(f"\u274c Error: No lane counts provided in '{args.lane_counts}'")
                return 1
            if any(c <= 0 for c in per_lane_counts):
                print("\u274c Error: All lane counts must be positive integers")
                return 1
            if (
                len(per_lane_counts) < 1
                or len(per_lane_counts) > MAX_AUTORESEARCH_LANES
            ):
                print(
                    f"\u274c Error: Lane count must be 1-{MAX_AUTORESEARCH_LANES}, got {len(per_lane_counts)} lanes"
                )
                return 1
        except ValueError:
            print(
                f"\u274c Error: Invalid lane counts '{args.lane_counts}' (expected comma-separated integers like '100,200,300')"
            )
            return 1

    # Parse --color-pattern argument
    custom_color: tuple[int, int, int] | None = None
    if args.color_pattern:
        hex_str = args.color_pattern.strip()
        if hex_str.startswith("0x") or hex_str.startswith("0X"):
            hex_str = hex_str[2:]
        if len(hex_str) != 6:
            print(
                f"\u274c Error: Color pattern must be 6 hex digits (RRGGBB), got '{args.color_pattern}'"
            )
            return 1
        try:
            r = int(hex_str[0:2], 16)
            g = int(hex_str[2:4], 16)
            b = int(hex_str[4:6], 16)
            custom_color = (r, g, b)
            print(
                f"\u2139\ufe0f  Using custom color pattern: RGB({r}, {g}, {b}) = 0x{hex_str.upper()}"
            )
        except ValueError:
            print(
                f"\u274c Error: Invalid hex color '{args.color_pattern}' (expected format: 'RRGGBB' or '0xRRGGBB')"
            )
            return 1

    # Build JSON-RPC commands
    config: dict[str, Any] = {}
    config["drivers"] = drivers

    if min_lanes is not None and max_lanes is not None:
        config["laneRange"] = {"min": min_lanes, "max": max_lanes}

    if args.strip_sizes:
        preset_map = {
            "tiny": (10, 100),
            "small": (100, 500),
            "medium": (300, 1000),
            "large": (500, 3000),
            "xlarge": (1000, 5000),
        }

        if args.strip_sizes in preset_map:
            short, long = preset_map[args.strip_sizes]
            config["stripSizes"] = [short, long]
        elif "," in args.strip_sizes:
            try:
                sizes = [int(s.strip()) for s in args.strip_sizes.split(",")]
                if not sizes:
                    print(
                        f"\u274c Error: No strip sizes provided in '{args.strip_sizes}'"
                    )
                    return 1
                if any(s <= 0 for s in sizes):
                    print("\u274c Error: Strip sizes must be positive integers")
                    return 1
                config["stripSizes"] = sizes
            except ValueError:
                print(
                    f"\u274c Error: Invalid strip sizes '{args.strip_sizes}' (expected comma-separated integers or preset name)"
                )
                return 1
        else:
            try:
                size = int(args.strip_sizes)
                if size <= 0:
                    print("\u274c Error: Strip size must be positive")
                    return 1
                config["stripSizes"] = [size]
            except ValueError:
                print(
                    f"\u274c Error: Invalid strip size '{args.strip_sizes}' (expected integer, comma-separated integers, or preset name)"
                )
                return 1

    if args.legacy_rgbw_small_counts:
        config["stripSizes"] = [1, 2, 3, 4]

    rpc_commands_list: list[dict[str, Any]] = []

    if per_lane_counts is not None:
        set_lane_sizes_cmd = {"method": "setLaneSizes", "params": [per_lane_counts]}
        rpc_commands_list.append(set_lane_sizes_cmd)
        print(
            f"\u2139\ufe0f  Setting per-lane LED counts: {', '.join(str(c) for c in per_lane_counts)} ({len(per_lane_counts)} lanes)"
        )

    if custom_color is not None:
        r, g, b = custom_color
        set_color_cmd = {
            "method": "setSolidColor",
            "params": [{"r": r, "g": g, "b": b}],
        }
        rpc_commands_list.append(set_color_cmd)
        print(
            "\u26a0\ufe0f  Note: setSolidColor RPC command requires firmware support (may need implementation)"
        )

    chipset_timing_map = {
        "ws2812": "WS2812B-V5",
        "ucs7604": "UCS7604-800KHZ",
    }
    timing_name = chipset_timing_map.get(args.chipset, "WS2812B-V5")

    drivers_list = config["drivers"]
    lane_range = config.get("laneRange", {"min": 1, "max": 1})
    strip_sizes = config.get("stripSizes", [100])

    if parallel_mode:
        for lane_count in range(lane_range["min"], lane_range["max"] + 1):
            for strip_size in strip_sizes:
                lane_sizes = [strip_size] * lane_count
                driver_entries: list[dict[str, Any]] = []
                for driver in drivers_list:
                    driver_entry: dict[str, Any] = {
                        "driver": driver,
                        "laneSizes": lane_sizes,
                    }
                    driver_entries.append(driver_entry)
                parallel_config: dict[str, Any] = {
                    "drivers": driver_entries,
                    "pattern": "MSB_LSB_A",
                    "iterations": 1,
                    "timing": timing_name,
                }
                rpc_command = {"method": "runParallelTest", "params": parallel_config}
                rpc_commands_list.append(rpc_command)

        print(
            f"\u2139\ufe0f  Generated {len(rpc_commands_list)} parallel test(s) "
            f"({len(drivers_list)} drivers \u00d7 {lane_range['max'] - lane_range['min'] + 1} lane count(s) \u00d7 {len(strip_sizes)} strip size(s))"
        )
    else:
        for driver in drivers_list:
            for lane_count in range(lane_range["min"], lane_range["max"] + 1):
                for strip_size in strip_sizes:
                    lane_sizes = [strip_size] * lane_count
                    test_config: dict[str, Any] = {
                        "driver": driver,
                        "laneSizes": lane_sizes,
                        "pattern": "MSB_LSB_A",
                        "iterations": 1,
                        "timing": timing_name,
                    }
                    if args.legacy:
                        test_config["useLegacyApi"] = True
                        if args.legacy_rgbw_small_counts:
                            test_config["legacyRgbw"] = True
                        if args.legacy_mixed_timings:
                            test_config["legacyChipsets"] = [
                                "WS2812B" if i % 2 == 0 else "SK6812"
                                for i in range(lane_count)
                            ]
                    # Multi-frame capture: back-to-back show()/capture cycles per pattern.
                    # Defaults: SPI -> 2 (catches #2254/#2288 second-frame degradation),
                    # others -> 1. User can override with --frames N.
                    if args.frames is not None:
                        frame_count = args.frames
                    elif driver == "SPI":
                        frame_count = 2
                    else:
                        frame_count = 1
                    if frame_count > 1:
                        test_config["frameCount"] = frame_count
                    if args.contaminate_tx_mux:
                        test_config["contaminateTxMux"] = True
                    if args.tight_timing:
                        if (
                            args.tight_timing_iterations < 1
                            or args.tight_timing_iterations > 64
                        ):
                            print("Error: --tight-timing-iterations must be in [1, 64]")
                            return 1
                        if args.tight_timing_max_overhead_us < 1:
                            print("Error: --tight-timing-max-overhead-us must be >= 1")
                            return 1
                        test_config["tightTiming"] = True
                        test_config["tightTimingIterations"] = (
                            args.tight_timing_iterations
                        )
                        test_config["tightTimingMaxOverheadUs"] = (
                            args.tight_timing_max_overhead_us
                        )
                    rpc_command = {"method": "runSingleTest", "params": test_config}
                    rpc_commands_list.append(rpc_command)

    json_rpc_cmd_str = json.dumps(rpc_commands_list)

    try:
        json_rpc_commands = parse_json_rpc_commands(json_rpc_cmd_str)
    except ValueError as e:
        print(f"\u274c Error parsing JSON-RPC commands: {e}")
        return 1

    # Build pattern lists
    expect_keywords: list[str]
    if args.no_expect:
        expect_keywords = args.expect_keywords or []
    else:
        expect_keywords = DEFAULT_EXPECT_PATTERNS.copy()
        if args.expect_keywords:
            expect_keywords.extend(args.expect_keywords)

    fail_keywords: list[str]
    if args.no_fail_on:
        fail_keywords = args.fail_keywords or []
    else:
        fail_keywords = [DEFAULT_FAIL_ON_PATTERN]
        fail_keywords.extend(EXIT_ON_ERROR_PATTERNS)
        if args.fail_keywords:
            fail_keywords.extend(args.fail_keywords)

    # Parse timeout (FastLED #3309: 30s default with advisory banner;
    # --no-timeout to disable; supports 30s / 10m / 1h / 5000ms notation).
    AUTORESEARCH_DEFAULT_TIMEOUT_S = 30
    no_timeout = bool(getattr(args, "no_timeout", False))
    quiet = bool(getattr(args, "quiet", False))
    if no_timeout:
        # Effectively unbounded: a year-of-seconds is "no timeout" for any
        # practical bring-up session and stays well within int range.
        timeout_seconds = 365 * 24 * 3600
    elif args.timeout is None:
        timeout_seconds = AUTORESEARCH_DEFAULT_TIMEOUT_S
        if not quiet:
            # Yellow advisory banner so the user sees the default is in effect
            # and knows how to override. Suppressed under --quiet. Fore/Style
            # are imported at module level above.
            print(
                f"{Fore.YELLOW}"
                f"[default {AUTORESEARCH_DEFAULT_TIMEOUT_S}s timeout in effect "
                f"-- pass --timeout <duration> (e.g. 30s / 2m / 1h) or "
                f"--no-timeout to override]"
                f"{Style.RESET_ALL}"
            )
    else:
        try:
            timeout_seconds = parse_timeout(args.timeout)
        except ValueError as e:
            print(f"\u274c Error: {e}")
            return 1

    # Resolve project root (always the user's invocation cwd) and build_dir.
    #
    # Two code paths (#3281):
    #
    # 1. Default (``args.use_root_platformio_ini == False``): synthesise
    #    ``.build/pio/<board>/platformio.ini`` from ``ci/boards.py`` if the
    #    board is already known at parse time; otherwise defer synthesis to
    #    :func:`_resolve_port_and_environment` after chip auto-detect.
    # 2. Legacy (``args.use_root_platformio_ini == True``): read root
    #    ``./platformio.ini``. The flag is deprecated and emits a warning at
    #    parse time (see ci/autoresearch/args.py).
    #
    # In the deferred-synthesis case we keep ``build_dir`` pointing at the
    # invocation cwd so the sketch source can still be located under
    # ``examples/AutoResearch/``. Synthesis happens later, and ``ctx.build_dir``
    # is rewritten before fbuild is invoked.
    project_root = args.project_dir.resolve()

    if args.use_root_platformio_ini:
        build_dir = project_root
        if not (build_dir / "platformio.ini").exists():
            print(f"\u274c Error: platformio.ini not found in {build_dir}")
            print(
                "   Make sure you're running this from a PlatformIO project directory"
            )
            return 1
    elif final_environment:
        # Board known up-front \u2014 synthesise now so downstream callers (chip
        # auto-detect log lines, default_envs probes) see the staged file.
        from ci.autoresearch.staging import synthesise_autoresearch_project

        try:
            build_dir = synthesise_autoresearch_project(
                final_environment,
                project_root=project_root,
                verbose=args.verbose,
            )
        except KeyboardInterrupt as ki:
            # Let user-initiated interrupts propagate via the project's
            # canonical handler; never swallow them under the broad
            # Exception handler below (CodeRabbit feedback, PR #3290).
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            print(
                f"\u274c Error: failed to synthesise platformio.ini for "
                f"board '{final_environment}': {e}"
            )
            return 1
    else:
        # Board not yet known \u2014 defer synthesis to _resolve_port_and_environment
        # once chip auto-detect resolves the environment. Use project_root as
        # the build_dir so the sketch-resolver fallback below picks up
        # examples/AutoResearch/. The build_dir is rewritten before fbuild is
        # invoked.
        build_dir = project_root

    # Sketch selection.
    #
    # Both Low-memory ARM boards (e.g. NXP LPC845-BRK, 64 KB Flash) and the
    # rich ESP32 / Teensy targets now share examples/AutoResearch/AutoResearch.ino
    # -- the sketch flips to its low-memory mode (compile-gated bring-up RPC
    # surface only) when FL_PLATFORM_HAS_LARGE_MEMORY == 0. The previous
    # standalone examples/AutoResearchLpc/ harness was retired by FastLED #3030
    # once the soft-FP cascade from #3022 phase 2 freed up the LPC845 flash
    # budget.
    #
    # For the synthesised path the staged tree lives under ``build_dir/src/sketch/``
    # (populated by ``_init_platformio_build`` \u2192 ``copy_example_source``);
    # for the legacy and deferred-synthesis paths the in-tree
    # ``examples/AutoResearch/`` source applies.
    sketch_path = build_dir / "examples" / "AutoResearch"
    if not sketch_path.exists():
        staged_sketch_path = build_dir / "src" / "sketch"
        if staged_sketch_path.exists():
            sketch_path = staged_sketch_path
        else:
            # Fall back to <project_root>/examples/AutoResearch/ when we're in
            # the synthesised path and the staged sketch isn't laid down yet
            # (shouldn't happen, but keeps the error message useful).
            repo_sketch_path = project_root / "examples" / "AutoResearch"
            if repo_sketch_path.exists():
                sketch_path = repo_sketch_path
            else:
                print(
                    "\u274c Error: AutoResearch sketch not found at "
                    f"{sketch_path} or {staged_sketch_path} or {repo_sketch_path}"
                )
                return 1

    os.environ["PLATFORMIO_SRC_DIR"] = str(sketch_path)

    return RunContext(
        args=args,
        drivers=drivers,
        json_rpc_commands=json_rpc_commands,
        expect_keywords=expect_keywords,
        fail_keywords=fail_keywords,
        timeout_seconds=timeout_seconds,
        build_dir=build_dir,
        simd_test_mode=simd_test_mode,
        coroutine_test_mode=coroutine_test_mode,
        ieee754_test_mode=ieee754_test_mode,
        wave2d_perf_grid=wave2d_perf_grid,
        net_server_mode=net_server_mode,
        net_client_mode=net_client_mode,
        net_loopback_mode=net_loopback_mode,
        ota_mode=ota_mode,
        ble_mode=ble_mode,
        decode_mode=decode_mode,
        gpio_only_mode=gpio_only_mode,
        parallel_mode=parallel_mode,
        final_environment=final_environment,
    )


# ============================================================
# Phase B: Resolve port and environment
# ============================================================


async def _resolve_port_and_environment(ctx: RunContext) -> int | None:
    """Detect upload port (with retry), auto-detect chip, determine build driver.

    Mutates ctx: sets upload_port, final_environment, use_fbuild, build_driver.
    Returns None on success, int exit code on failure.
    """
    args = ctx.args
    is_teensy = (
        ctx.final_environment is not None and "teensy" in ctx.final_environment.lower()
    )

    upload_port = args.upload_port
    if not upload_port:
        expected_environment = None if is_teensy else ctx.final_environment
        max_wait_s = 60
        poll_interval_s = 1.0
        result = auto_detect_upload_port(expected_environment=expected_environment)
        if not result.ok:
            if is_teensy:
                print(f"\n{Fore.YELLOW}{'=' * 60}")
                print(f"  Teensy not detected on USB.")
                print(
                    "  AutoResearch will not pre-upload stale .pio firmware "
                    "during port detection."
                )
                print(
                    "  Power-cycle or reconnect the Teensy so the USB serial "
                    "port appears, then let fbuild own the deploy."
                )
                print(f"{'=' * 60}{Style.RESET_ALL}\n")
            print(
                f"\u23f3 No USB serial port found yet. Waiting up to {max_wait_s}s for device..."
            )
            deadline = time.monotonic() + max_wait_s
            last_msg_at = 0.0
            while time.monotonic() < deadline:
                time.sleep(poll_interval_s)
                result = auto_detect_upload_port(
                    expected_environment=expected_environment
                )
                if result.ok:
                    elapsed = max_wait_s - (deadline - time.monotonic())
                    print(
                        f"\u2705 USB serial port detected after {elapsed:.1f}s: {result.selected_port}"
                    )
                    break
                now = time.monotonic()
                if now - last_msg_at >= 5.0:
                    remaining = deadline - now
                    print(
                        f"   Still waiting... {int(remaining)}s remaining",
                        flush=True,
                    )
                    last_msg_at = now

        if not result.ok:
            print(f"{Fore.RED}{'=' * 60}")
            print(f"{Fore.RED}\u26a0\ufe0f  FATAL ERROR: PORT DETECTION FAILED")
            print(f"{Fore.RED}{'=' * 60}")
            print(f"\n{Fore.RED}{result.error_message}{Style.RESET_ALL}\n")
            if result.all_ports:
                print("Available ports (all non-USB):")
                for port in result.all_ports:
                    print(f"  {port.device}: {port.description}")
                    print(f"    hwid: {port.hwid}")
            else:
                print("No serial ports detected on system.")
            print(
                f"\n{Fore.RED}Only USB devices are supported. Please connect a USB device and try again.{Style.RESET_ALL}"
            )
            if is_teensy:
                print(
                    f"{Fore.YELLOW}Teensy hint: keep the board in USB serial mode "
                    f"for AutoResearch acceptance; fbuild must perform the first "
                    f"firmware deploy for this run.{Style.RESET_ALL}"
                )
            print(
                f"{Fore.RED}Note: Bluetooth serial ports (BTHENUM) are not supported.{Style.RESET_ALL}\n"
            )
            return 1

        upload_port = result.selected_port

    assert upload_port is not None, "upload_port should be set by auto-detection"
    ctx.upload_port = upload_port

    # Auto-detect environment from attached chip
    detected_chip_type: str | None = None
    detected_environment: str | None = None

    if not ctx.final_environment:
        print("\U0001f50d Detecting attached chip type...")
        chip_result = detect_attached_chip(upload_port)
        if chip_result.ok and chip_result.environment:
            detected_chip_type = chip_result.chip_type
            detected_environment = chip_result.environment
            ctx.final_environment = detected_environment
            print(
                f"\u2705 Detected {chip_result.chip_type} \u2192 using environment '{ctx.final_environment}'"
            )
        if not ctx.final_environment:
            # Fall back to <build_dir>/platformio.ini's `default_envs` value.
            # In the legacy (--use-root-platformio-ini) path this reads root
            # ./platformio.ini; in the synthesised path (#3281) this reads
            # nothing if the board isn't already known (deferred synthesis
            # hasn't run yet). That fallback is fine \u2014 we just don't have a
            # platformio.ini to consult yet, so we bail with a clear error.
            if args.use_root_platformio_ini:
                from ci.util.pio_package_daemon import get_default_environment

                default_env = get_default_environment(str(ctx.build_dir))
                if default_env:
                    ctx.final_environment = default_env
                    error_msg = "Chip detection failed"
                    print(
                        f"\u26a0\ufe0f  {error_msg}, "
                        f"using platformio.ini default: '{ctx.final_environment}'"
                    )
                else:
                    print(
                        "\u26a0\ufe0f  Chip detection failed and no default_envs in platformio.ini"
                    )
            else:
                # Synthesised path with no env known and chip detect failed:
                # there is no platformio.ini to fall back to and nothing
                # downstream can recover. Fail fast with a clear error
                # (CodeRabbit feedback, PR #3290) so callers don't silently
                # proceed with an unset final_environment.
                print(
                    "\u274c Chip detection failed and no environment given. "
                    "Pass a positional environment (e.g. `bash autoresearch esp32c6 ...`) "
                    "or attach a recognisable device."
                )
                return 1
        print()

    if args.use_root_platformio_ini and _reject_teensy_root_platformio_ini(
        ctx.final_environment
    ):
        return 1

    # Deferred synthesis (#3281). When --use-root-platformio-ini is NOT set
    # and the board wasn't known at parse time, the build_dir is still pointing
    # at the project root. Now that chip auto-detect has resolved the
    # environment we can synthesise .build/pio/<board>/platformio.ini and
    # redirect build_dir so fbuild reads the synthesised file instead of root.
    if (
        not args.use_root_platformio_ini
        and ctx.final_environment
        and ctx.build_dir == args.project_dir.resolve()
    ):
        from ci.autoresearch.staging import synthesise_autoresearch_project

        try:
            ctx.build_dir = synthesise_autoresearch_project(
                ctx.final_environment,
                project_root=args.project_dir.resolve(),
                verbose=args.verbose,
            )
            # The staged sketch lives under <build_dir>/src/sketch \u2014 point
            # PLATFORMIO_SRC_DIR at it so fbuild and any downstream sketch
            # resolver pick up the freshly-copied source.
            staged_sketch = ctx.build_dir / "src" / "sketch"
            if staged_sketch.exists():
                os.environ["PLATFORMIO_SRC_DIR"] = str(staged_sketch)
        except KeyboardInterrupt as ki:
            # Let user-initiated interrupts propagate via the project's
            # canonical handler; never swallow them under the broad
            # Exception handler below (CodeRabbit feedback, PR #3290).
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            print(
                f"\u274c Error: failed to synthesise platformio.ini for "
                f"board '{ctx.final_environment}': {e}"
            )
            return 1

    # Platform mismatch warning (legacy path only \u2014 synthesised platformio.ini
    # is generated from ci/boards.py, so `default_envs` always matches the
    # detected board by construction and the warning would be noise).
    if args.use_root_platformio_ini:
        from ci.util.pio_package_daemon import get_default_environment

        default_env = get_default_environment(str(ctx.build_dir))
    else:
        default_env = None
    if detected_environment and default_env and detected_environment != default_env:
        print(f"{Fore.YELLOW}{'=' * 60}")
        print(f"{Fore.YELLOW}\u26a0\ufe0f  PLATFORM MISMATCH WARNING")
        print(f"{Fore.YELLOW}{'=' * 60}")
        print(
            f"{Fore.YELLOW}Detected chip: {detected_chip_type} ({detected_environment})"
        )
        print(f"{Fore.YELLOW}platformio.ini default_envs: {default_env}")
        print(
            f"{Fore.YELLOW}Using detected environment '{detected_environment}' for this session."
        )
        print(
            f"{Fore.YELLOW}To make this permanent, update platformio.ini: default_envs = {detected_environment}"
        )
        print(f"{Fore.YELLOW}{'=' * 60}{Style.RESET_ALL}")
        print()

    # Select build driver
    ctx.build_driver = select_build_driver(
        ctx.final_environment, args.use_fbuild, args.no_fbuild
    )
    ctx.use_fbuild = ctx.build_driver.name == "fbuild"

    return None


# ============================================================
# Phase C: Build and deploy
# ============================================================


async def _run_build_deploy(ctx: RunContext, qctx: QuietContext) -> int | None:
    """Package install, C++ lint, compile, upload, USB re-enumeration wait.

    Returns None on success, int exit code on failure.
    """
    args = ctx.args
    build_dir = ctx.build_dir
    final_environment = ctx.final_environment
    final_environment_norm = (final_environment or "").lower()
    build_environment = _build_environment_for_mode(ctx)
    upload_port = ctx.upload_port
    build_driver = ctx.build_driver
    assert build_driver is not None

    # Phase 0: Package Installation
    if not build_driver.install_packages(build_dir, build_environment):
        print("\n\u274c Package installation failed")
        return 1
    print()

    # Phase 1: Lint C++ Code
    if not args.skip_lint:
        if not run_cpp_lint():
            print("\n\u274c Linting failed. Fix issues or use --skip-lint to bypass.")
            return 1
    else:
        print("\u26a0\ufe0f  Skipping C++ linting (--skip-lint flag set)")
        print()

    # Phase 2+3: Build + Deploy
    print(f"\U0001f4e6 Using {build_driver.name}")

    if final_environment_norm in LPC_BRING_UP_ENVS:
        # fbuild's nxplpc orchestrator does not yet ship a deployer
        # (`daemon/.../deploy.rs` only dispatches avr/teensy). Bring-up boards
        # run `fbuild build` followed by a pyocd-based flash + sw-reset here.
        if not _build_and_flash_nxplpc(
            build_dir,
            environment=build_environment
            or final_environment_norm
            or final_environment,
            upload_port=upload_port,
            verbose=args.verbose,
        ):
            qctx.emit("BUILD+FLASH FAIL (nxplpc)")
            qctx.emit_log_path()
            return 1
    elif not build_driver.deploy(
        build_dir,
        environment=build_environment,
        upload_port=upload_port,
        verbose=args.verbose,
        clean=args.clean,
        quiet=args.quiet,
        log_file=qctx.log_file,
    ):
        qctx.emit("BUILD+FLASH FAIL")
        qctx.emit_log_path()
        return 1

    # Wait for serial port to become available after upload
    if upload_port and build_driver.name == "platformio":
        print(
            "\n\u23f3 Waiting for device to reboot and serial port to become available..."
        )
        port_ready = False
        max_wait_time = 15.0
        start_time = time.time()

        while time.time() - start_time < max_wait_time:
            if is_interrupted():
                raise KeyboardInterrupt()
            kill_port_users(upload_port)
            try:
                import serial

                with serial.Serial(upload_port, 115200, timeout=0.1) as _ser:
                    port_ready = True
                    elapsed = time.time() - start_time
                    print(f"\u2705 Serial port available after {elapsed:.1f}s")
                    break
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception:
                for _ in range(5):
                    if is_interrupted():
                        raise KeyboardInterrupt()
                    time.sleep(0.1)

        if not port_ready:
            print(
                f"{Fore.YELLOW}\u26a0\ufe0f  Port not available after {max_wait_time}s, proceeding anyway...{Style.RESET_ALL}"
            )

        kill_port_users(upload_port)
        time.sleep(0.5)
    elif upload_port and build_driver.name == "fbuild":
        import serial

        port_ready = False
        for _ in range(10):
            if is_interrupted():
                raise KeyboardInterrupt()
            try:
                with serial.Serial(upload_port, 115200, timeout=0.1) as _ser:
                    port_ready = True
                    break
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except (serial.SerialException, OSError):
                time.sleep(0.3)
            except Exception as e:
                print(
                    f"\u26a0\ufe0f  Unexpected error checking port: {type(e).__name__}: {e}"
                )
                time.sleep(0.3)
        if not port_ready:
            print(
                f"\u26a0\ufe0f  Port {upload_port} not available after 3s, proceeding anyway..."
            )

    # POST-FLASH, PRE-CONNECT EPOCH. From here on, every downstream RPC
    # call computes its per-call timeout via `ctx.remaining_seconds()`
    # so that the user-supplied `--timeout N` is a HARD WALL on the
    # post-flash budget rather than a per-call value that could be
    # silently inflated by hardcoded callsites (the previous bug --
    # `--timeout 60` then 120 s hardcoded at the runSingleTest call
    # site stretched real wall-clock to 15+ min). The watchdog kicks
    # in `WATCHDOG_GRACE_SECONDS` after that hard wall to force-kill
    # if normal teardown wedges. See goal session 2026-06-22.
    ctx.start_timeout_epoch()
    start_autoresearch_watchdog(ctx)
    if ctx.deadline_epoch is not None:
        print(
            f"\u23f1  Post-flash deadline armed: "
            f"--timeout={ctx.timeout_seconds:.0f}s "
            f"(+ {WATCHDOG_GRACE_SECONDS:.0f}s watchdog grace)"
        )

    return None


# ============================================================
# Phase D: Schema validation and pin setup
# ============================================================


async def _run_schema_and_pin_setup(ctx: RunContext) -> int | None:
    """Validate RPC schema, create serial interface, discover pins, GPIO pre-test.

    Mutates ctx: sets serial_iface, crash_decoder, effective_tx/rx_pin,
    discovery_client, pins_discovered. May modify json_rpc_commands.
    Returns None on success, int exit code on failure.
    """
    args = ctx.args
    upload_port = ctx.upload_port
    assert upload_port is not None
    use_fbuild = ctx.use_fbuild
    final_environment = ctx.final_environment

    # Validate RPC commands against device schema
    constrained_platforms = ["esp32c6", "esp32c2", "esp32p4", "teensy41", "teensy40"]
    skip_schema = (
        args.skip_schema or args.quiet or final_environment in constrained_platforms
    )

    if skip_schema:
        print(
            f"\n\u23ed\ufe0f  Skipping schema validation on {final_environment} (constrained platform)"
        )

    if not skip_schema:
        try:
            from ci.rpc_schema_validator import RpcSchemaValidator

            print("\n\U0001f50d Validating RPC commands against device schema...")
            validator = RpcSchemaValidator(port=upload_port, timeout=10.0)

            validation_errors = []
            for i, cmd in enumerate(ctx.json_rpc_commands):
                method = cmd.get("method")
                if not method:
                    validation_errors.append(
                        f"Command {i + 1}: Missing 'method' or 'function' field"
                    )
                    continue

                cmd_params = cmd.get("params", [])
                if isinstance(cmd_params, list) and len(cmd_params) == 1:
                    cmd_params = cmd_params[0]

                try:
                    validator.validate_request(method, cmd_params)
                except KeyboardInterrupt as ki:
                    handle_keyboard_interrupt(ki)
                    raise
                except Exception as e:
                    validation_errors.append(f"Command {i + 1} ({method}): {e}")

            if validation_errors:
                print(f"\n\u274c Schema validation failed:")
                for error in validation_errors:
                    print(f"  - {error}")
                print(
                    f"\n\U0001f4a1 Fix: Update command parameters to match device schema"
                )
                return 1

            print(
                f"\u2705 All {len(ctx.json_rpc_commands)} RPC commands validated against schema"
            )
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except ImportError:
            print("\u26a0\ufe0f  pydantic not available - skipping schema validation")
        except Exception as e:
            print(f"\u26a0\ufe0f  Schema validation skipped: {e}")

    # Create serial interface
    from ci.util.serial_interface import create_serial_interface

    final_environment = (ctx.final_environment or "").lower()
    default_tx_pin, default_rx_pin = default_pins_for_environment(final_environment)
    uses_flex_io_default_tx = _uses_teensy_flex_io_default_tx(
        args, final_environment, ctx.drivers
    )
    if uses_flex_io_default_tx:
        default_tx_pin = FLEX_IO_TEENSY_DEFAULT_TX_PIN
    use_pyserial = (not use_fbuild) or final_environment in LPC_BRING_UP_ENVS
    ctx.serial_iface = create_serial_interface(
        port=upload_port, use_pyserial=use_pyserial
    )

    # Create crash trace decoder
    if final_environment:
        ctx.crash_decoder = CrashTraceDecoder(
            ctx.build_dir, final_environment, use_fbuild=use_fbuild
        )

    serial_iface = ctx.serial_iface

    # Pin discovery
    # LPC bring-up sketches only bind `echo` (no `findConnectedPins` RPC),
    # so pin discovery would hang. Skip it for those boards — the bring-up
    # short-circuit later in `_run_full_autoresearch_pipeline` will run
    # `_run_bring_up_tests` directly against the configured upload port.
    if final_environment in LPC_BRING_UP_ENVS:
        print(
            f"\n\U0001f4cc LPC bring-up mode ({ctx.final_environment}): "
            "skipping pin discovery and GPIO pre-test"
        )
    elif ctx.simd_test_mode:
        print("\n\U0001f4cc SIMD mode: skipping pin discovery and GPIO pre-test")
    elif ctx.coroutine_test_mode:
        print("\n\U0001f4cc Coroutine mode: skipping pin discovery and GPIO pre-test")
    elif ctx.ieee754_test_mode:
        print(
            "\n\U0001f4cc IEEE754 codec mode: skipping pin discovery and GPIO pre-test"
        )
    elif ctx.net_server_mode or ctx.net_client_mode or ctx.net_loopback_mode:
        print("\n\U0001f4cc Network mode: skipping pin discovery and GPIO pre-test")
    elif ctx.ota_mode:
        print("\n\U0001f4cc OTA mode: skipping pin discovery and GPIO pre-test")
    elif ctx.ble_mode:
        print("\n\U0001f4cc BLE mode: skipping pin discovery and GPIO pre-test")
    elif args.tx_pin is not None or args.rx_pin is not None:
        ctx.effective_tx_pin = (
            args.tx_pin if args.tx_pin is not None else default_tx_pin
        )
        ctx.effective_rx_pin = (
            args.rx_pin if args.rx_pin is not None else default_rx_pin
        )
        print(
            f"\n\U0001f4cc Using CLI-specified pins: TX={ctx.effective_tx_pin}, RX={ctx.effective_rx_pin}"
        )
        if uses_flex_io_default_tx:
            print(
                "\U0001f4cc Teensy FLEX_IO defaulted omitted TX to "
                f"{ctx.effective_tx_pin}."
            )
    elif args.auto_discover_pins:
        print("\n\U0001f50d Auto-discovery enabled - searching for connected pins...")
        # FastLED #3446: walk the full classic-ESP32 GPIO range via
        # overlapping 8-pin windows so shorts on any ADC2 / IO_MUX-only
        # pin (e.g. the user-reported (33, 34) pair) are reachable. The
        # segmented helper isolates segment-level hangs so an unsafe
        # window only takes out itself, not the whole sweep.
        pin_discovery = await run_pin_discovery_segmented(
            upload_port, serial_interface=serial_iface
        )
        ctx.discovery_client = pin_discovery.client

        if (
            pin_discovery.success
            and pin_discovery.tx_pin is not None
            and pin_discovery.rx_pin is not None
        ):
            ctx.effective_tx_pin = pin_discovery.tx_pin
            ctx.effective_rx_pin = pin_discovery.rx_pin
            ctx.pins_discovered = True
            print(
                f"\U0001f4cc Using discovered pins: TX={ctx.effective_tx_pin}, RX={ctx.effective_rx_pin}"
            )
        else:
            ctx.effective_tx_pin = default_tx_pin
            ctx.effective_rx_pin = default_rx_pin
            default_reason = "default pins"
            if uses_flex_io_default_tx:
                default_reason = "Teensy FLEX_IO default pins"
            print(
                f"\U0001f4cc Using {default_reason}: "
                f"TX={ctx.effective_tx_pin}, RX={ctx.effective_rx_pin}"
            )
            if ctx.discovery_client:
                await ctx.discovery_client.close()
                ctx.discovery_client = None
    else:
        ctx.effective_tx_pin = default_tx_pin
        ctx.effective_rx_pin = default_rx_pin
        default_reason = "default pins"
        if uses_flex_io_default_tx:
            default_reason = "Teensy FLEX_IO default pins"
        print(
            f"\n\U0001f4cc Using {default_reason}: "
            f"TX={ctx.effective_tx_pin}, RX={ctx.effective_rx_pin}"
        )

    _ensure_leading_set_pins_command(ctx)

    # GPIO connectivity pre-test
    if final_environment in LPC_BRING_UP_ENVS:
        # LPC bring-up: no jumper wire / pin loop, the bring-up RPC test
        # itself is the only check we run.
        pass
    elif ctx.simd_test_mode:
        pass
    elif ctx.coroutine_test_mode:
        pass
    elif ctx.ieee754_test_mode:
        pass
    elif ctx.net_server_mode or ctx.net_client_mode or ctx.net_loopback_mode:
        pass
    elif ctx.ota_mode:
        pass
    elif ctx.ble_mode:
        pass
    elif ctx.pins_discovered:
        print("\n\u2705 Skipping GPIO pre-test (pins verified during discovery)")
    elif not await run_gpio_pretest(
        upload_port,
        ctx.effective_tx_pin if ctx.effective_tx_pin is not None else default_tx_pin,
        ctx.effective_rx_pin if ctx.effective_rx_pin is not None else default_rx_pin,
        serial_interface=serial_iface,
    ):
        print()
        print(f"{Fore.RED}=" * 60)
        print(f"{Fore.RED}AUTORESEARCH ABORTED - GPIO PRE-TEST FAILED")
        print(f"{Fore.RED}=" * 60)
        print()
        print("The GPIO connectivity test did not pass. Possible causes:")
        print()
        print("  1. RPC COMMUNICATION FAILURE: The device is not responding to")
        print("     JSON-RPC commands. Check serial output above for boot errors")
        print("     or crashes. Try power-cycling the device.")
        print()
        tx = (
            ctx.effective_tx_pin if ctx.effective_tx_pin is not None else default_tx_pin
        )
        rx = (
            ctx.effective_rx_pin if ctx.effective_rx_pin is not None else default_rx_pin
        )
        print("  2. WRONG PIN PAIR: The jumper wire may be connected to different")
        print(f"     pins than expected (tested TX={tx}, RX={rx}).")
        print("     Try: bash autoresearch --auto-discover-pins")
        print("     Or specify pins explicitly: --tx-pin N --rx-pin M")
        print()
        print("  3. NO JUMPER WIRE: If no jumper wire is connected, connect one")
        print(f"     between GPIO {tx} and GPIO {rx}.")
        print()
        print("  4. DRIVER FAILURE: The pin test passed (electrical connection OK)")
        print("     but the LED driver captured 0 edges from DMA. This indicates")
        print("     a driver/DMA configuration issue, NOT a wiring problem.")
        print("     Check firmware logs for '[RX TEST]: wait() succeeded but 0")
        print("     edges captured' messages.")
        print()
        return 1

    return None


# ============================================================
# Phase E: Run tests or special mode
# ============================================================


async def _run_tests_or_special_mode(ctx: RunContext, qctx: QuietContext) -> int:
    """Execute GPIO-only, special modes, or main RPC test loop.

    Returns exit code (0 = success, 1 = failure, 130 = interrupted).
    """
    upload_port = ctx.upload_port
    assert upload_port is not None
    serial_iface = ctx.serial_iface
    use_fbuild = ctx.use_fbuild

    # Low-memory ARM bring-up mode short-circuit. LPC845/LPC804 boards run a
    # JSON-RPC echo verification (examples/AutoResearchLpc/) instead of the
    # GPIO + LED-protocol matrix that ESP32/Teensy targets use. Must come
    # BEFORE the gpio_only_mode early-return so the harness actually runs the
    # echo check instead of bailing with a "no tests requested" success.
    final_environment = (ctx.final_environment or "").lower()
    if final_environment in LPC_BRING_UP_ENVS:
        if ctx.ieee754_test_mode:
            return await _run_ieee754_tests(ctx)
        # FastLED #3021 Phase 1: --pin-toggle-rx runs the SCT-RX
        # loopback bench instead of the default echo bring-up.
        if getattr(ctx.args, "pin_toggle_rx", False):
            return await _run_lpc_pin_toggle_rx_tests(ctx)
        # FastLED #3021 Phase 2: --ws2812-loopback runs the WS2812
        # byte-match loopback bench. LPC845 low-memory builds bind the
        # required RPC automatically from the platform predicate.
        if getattr(ctx.args, "ws2812_loopback", False):
            if final_environment not in LPC_WS2812_ENVS:
                print(
                    "--ws2812-loopback is only supported on LPC845 boards "
                    "(lpc845brk, lpc845, lpcxpresso845max)."
                )
                return 1
            return await _run_lpc_ws2812_loopback_tests(ctx)
        # FastLED #3468: --pwm-dma-cl runs the channels-API SCT+DMA
        # clockless engine self-loopback. Sibling of --ws2812-loopback
        # but exercises `ChannelEngineLpcSctDma` (channels-API path)
        # rather than the legacy `ClocklessController` template.
        if getattr(ctx.args, "pwm_dma_cl", False):
            if final_environment not in LPC_WS2812_ENVS:
                print(
                    "--pwm-dma-cl is only supported on LPC845 boards "
                    "(lpc845brk, lpc845, lpcxpresso845max)."
                )
                return 1
            if getattr(ctx.args, "ws2812_loopback", False):
                print(
                    "--pwm-dma-cl and --ws2812-loopback are mutually "
                    "exclusive: both target the SCT peripheral."
                )
                return 1
            if getattr(ctx.args, "dma_spi", False):
                print(
                    "--pwm-dma-cl and --dma-spi are mutually exclusive: "
                    "both claim DMA0 channels and the LowMemory flash "
                    "budget doesn't fit both."
                )
                return 1
            return await _run_lpc_pwm_dma_cl_tests(ctx)
        # FastLED #3456: --dma-spi runs the SPI+DMA async driver bench.
        # Phase 1 of the #3453 bring-up.
        if getattr(ctx.args, "dma_spi", False):
            if final_environment not in LPC_WS2812_ENVS:
                print(
                    "--dma-spi is only supported on LPC845 boards "
                    "(lpc845brk, lpc845, lpcxpresso845max)."
                )
                return 1
            return await _run_lpc_dma_spi_tests(ctx)
        return await _run_bring_up_tests(ctx)

    # GPIO-only mode
    if ctx.gpio_only_mode:
        print()
        print("=" * 60)
        print(f"{Fore.GREEN}\u2713 GPIO-ONLY AUTORESEARCH SUCCEEDED{Style.RESET_ALL}")
        print("=" * 60)
        print("Pin discovery and GPIO connectivity pre-test passed.")
        print("No driver tests requested \u2014 skipping RPC test matrix.")
        print()
        return 0

    # Fall back to loopback for no-WiFi platforms
    net_server_mode = ctx.net_server_mode
    net_client_mode = ctx.net_client_mode
    net_loopback_mode = ctx.net_loopback_mode

    if (net_server_mode or net_client_mode) and ctx.final_environment:
        if not environment_has_wifi(ctx.final_environment):
            mode_name = "--net-server" if net_server_mode else "--net-client"
            print(
                f"\n\u26a0\ufe0f  {ctx.final_environment} does not have WiFi hardware"
            )
            print(f"   Falling back from {mode_name} to --net (loopback) mode")
            net_server_mode = False
            net_client_mode = False
            net_loopback_mode = True

    # Network mode
    if net_server_mode or net_client_mode:
        from ci.autoresearch.net import run_net_autoresearch

        return await run_net_autoresearch(
            upload_port=upload_port,
            serial_iface=serial_iface,
            net_server_mode=net_server_mode,
            net_client_mode=net_client_mode,
            timeout=ctx.timeout_seconds,
        )

    if net_loopback_mode:
        from ci.autoresearch.net import run_net_loopback_autoresearch

        return await run_net_loopback_autoresearch(
            upload_port=upload_port,
            serial_iface=serial_iface,
            timeout=ctx.timeout_seconds,
        )

    # OTA mode
    if ctx.ota_mode:
        from ci.autoresearch.ota import run_ota_autoresearch

        firmware_path: Path | None = None
        if ctx.final_environment is not None and ctx.build_driver is not None:
            firmware_path = ctx.build_driver.firmware_path(
                ctx.build_dir, ctx.final_environment
            )
        return await run_ota_autoresearch(
            upload_port=upload_port,
            serial_iface=serial_iface,
            timeout=ctx.timeout_seconds,
            firmware_path=firmware_path,
        )

    # BLE mode
    if ctx.ble_mode:
        from ci.autoresearch.ble import run_ble_autoresearch

        return await run_ble_autoresearch(
            upload_port=upload_port,
            serial_iface=serial_iface,
            timeout=ctx.timeout_seconds,
        )

    # SIMD test mode
    if ctx.simd_test_mode:
        return await _run_simd_tests(ctx)

    # Coroutine test mode
    if ctx.coroutine_test_mode:
        return await _run_coroutine_tests(ctx)

    # IEEE 754 codec test mode
    if ctx.ieee754_test_mode:
        return await _run_ieee754_tests(ctx)

    # Wave2D perf benchmark mode (#3113 Task 1 / #3122 A1)
    if ctx.wave2d_perf_grid is not None:
        return await _run_wave2d_perf_tests(ctx)

    # (LPC bring-up mode short-circuits earlier — see the gpio_only_mode
    # block above; reached only by ESP32/Teensy-class targets.)

    # Main RPC test execution
    return await _run_rpc_tests(ctx, qctx)


async def _run_wave2d_perf_tests(ctx: RunContext) -> int:
    """Run the Wave2D perf benchmark via RPC chain.

    Implements meta #3113 Task 1 host side. Wires the device-side
    wave2dPerf + perfProbe* RPCs (#3116, #3118, refactored to live
    behind fl::wave_perf::* in #3120) into a single host-driven
    measurement pass.

    Sequence per platform:
      1. perfProbeMemcpy — verify SRAM throughput vs vendor floor.
      2. perfProbeNop    — verify cycle-time consistency.
      3. perfProbeRepeat — verify wave2dPerf is reproducible
                           (std_dev_pct < 5%).
      4. wave2dPerf x 4  — { FivePoint, NinePointIsotropic } x
                           { compute, loads_only }.
         The loads_only/compute gap separates memory from compute cost.

    If any probe in 1-3 fails, results are stamped UNTRUSTED in the
    output. (Cf. issue #3124 for the planned --perf-XX flag-rename.)
    """
    assert ctx.wave2d_perf_grid is not None
    upload_port = ctx.upload_port
    assert upload_port is not None
    serial_iface = ctx.serial_iface
    grid_w, grid_h = ctx.wave2d_perf_grid

    print()
    print("=" * 60)
    print(f"WAVE2D PERF — {grid_w}x{grid_h}")
    print("=" * 60)
    print()

    client: RpcClient | None = None
    untrusted_reasons: list[str] = []
    results: dict[str, Any] = {
        "grid": [grid_w, grid_h],
        "probes": {},
        "bench": {},
        "untrusted": False,
        "untrusted_reasons": untrusted_reasons,
    }
    try:
        print("   Connecting to device...", end="", flush=True)
        client = RpcClient(upload_port, timeout=30.0, serial_interface=serial_iface)
        await client.connect(boot_wait=1.0)
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")

        # --- Probe 1: memcpy throughput ----------------------------------
        print("   perfProbeMemcpy (4096 bytes x 1000 iters)...", end="", flush=True)
        memcpy_resp = await client.send_and_match(
            "perfProbeMemcpy",
            args=[{"bytes": 4096, "iterations": 1000}],
            match_key="success",
            retries=2,
        )
        memcpy_data = memcpy_resp.data
        memcpy_mb_per_s = float(memcpy_data.get("mb_per_s", 0.0))
        results["probes"]["memcpy"] = memcpy_data
        print(f" {Fore.GREEN}{memcpy_mb_per_s:.1f} MB/s{Style.RESET_ALL}")

        # --- Probe 2: nop cycle calibration ------------------------------
        print("   perfProbeNop (100k iters)...", end="", flush=True)
        nop_resp = await client.send_and_match(
            "perfProbeNop",
            args=[{"iterations": 100000}],
            match_key="success",
            retries=2,
        )
        nop_data = nop_resp.data
        us_per_iter = float(nop_data.get("us_per_iter", 0.0))
        results["probes"]["nop"] = nop_data
        print(f" {Fore.GREEN}{us_per_iter:.4f} us/iter{Style.RESET_ALL}")

        # --- Probe 3: wave2dPerf repeat-stability ------------------------
        print(
            f"   perfProbeRepeat ({grid_w}x{grid_h}, 50 iters x 8 repeats)...",
            end="",
            flush=True,
        )
        repeat_resp = await client.send_and_match(
            "perfProbeRepeat",
            args=[
                {
                    "W": grid_w,
                    "H": grid_h,
                    "iterations": 50,
                    "repeats": 8,
                    "stencil": "FivePoint",
                }
            ],
            match_key="success",
            retries=2,
        )
        repeat_data = repeat_resp.data
        std_dev_pct = float(repeat_data.get("std_dev_pct", 100.0))
        results["probes"]["repeat"] = repeat_data
        if std_dev_pct >= 5.0:
            untrusted_reasons.append(
                f"perfProbeRepeat std_dev_pct={std_dev_pct:.2f} >= 5.0"
            )
            print(
                f" {Fore.YELLOW}{std_dev_pct:.2f}% (UNTRUSTED >= 5%){Style.RESET_ALL}"
            )
        else:
            print(f" {Fore.GREEN}{std_dev_pct:.2f}% (stable){Style.RESET_ALL}")

        # --- Main benchmark: wave2dPerf across 2 stencils x 2 modes ------
        print()
        print("   wave2dPerf:")
        for stencil in ("FivePoint", "NinePointIsotropic"):
            for loads_only in (False, True):
                label = (
                    f"     {stencil} {'(loads_only)' if loads_only else '(compute)'}"
                )
                print(f"{label} ...", end="", flush=True)
                bench_resp = await client.send_and_match(
                    "wave2dPerf",
                    args=[
                        {
                            "W": grid_w,
                            "H": grid_h,
                            "iterations": 100,
                            "stencil": stencil,
                            "loads_only": loads_only,
                        }
                    ],
                    match_key="success",
                    retries=2,
                )
                bench_data = bench_resp.data
                us_per_cell = float(bench_data.get("us_per_cell_per_update", 0.0))
                fps = float(bench_data.get("fps_at_one_update_per_frame", 0.0))
                key = f"{stencil.lower()}_{'loads_only' if loads_only else 'compute'}"
                results["bench"][key] = bench_data
                print(
                    f" {Fore.GREEN}{us_per_cell:.4f} us/cell, "
                    f"{fps:.0f} fps{Style.RESET_ALL}"
                )

        if untrusted_reasons:
            results["untrusted"] = True
            print()
            print(f"{Fore.YELLOW}*** RESULTS STAMPED UNTRUSTED ***{Style.RESET_ALL}")
            for reason in untrusted_reasons:
                print(f"   - {reason}")

        print()
        print("--- JSON ---")
        print(json.dumps(results, indent=2))
        print()
        return 0

    except RpcTimeoutError:
        print()
        print(f"{Fore.RED}WAVE2D PERF TIMEOUT{Style.RESET_ALL}")
        return 1
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print()
        print(f"{Fore.RED}WAVE2D PERF ERROR: {e}{Style.RESET_ALL}")
        return 1
    finally:
        if client is not None:
            await client.close()


async def _run_ieee754_tests(ctx: RunContext) -> int:
    """Run integer IEEE 754 decimal codec verification via RPC."""
    upload_port = ctx.upload_port
    assert upload_port is not None
    serial_iface = ctx.serial_iface

    print()
    print("=" * 60)
    print("IEEE754 CODEC TEST MODE")
    print("=" * 60)
    print()

    client: RpcClient | None = None
    try:
        print("   Connecting to device...", end="", flush=True)
        client = RpcClient(upload_port, timeout=30.0, serial_interface=serial_iface)
        await client.connect(boot_wait=1.0, drain_boot=True)
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")

        print("   Sending ieee754CodecTest RPC...", end="", flush=True)
        response = await client.send_and_match(
            "ieee754CodecTest", match_key="success", retries=3
        )
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")
        print()

        data = response.data
        tests_run = int(data.get("tests_run", 0))
        tests_failed = int(data.get("tests_failed", 0))
        first_failure = str(data.get("first_failure", ""))
        expected_bits = int(data.get("expected_bits", 0))
        actual_bits = int(data.get("actual_bits", 0))

        print(json.dumps(data, indent=2))
        print()

        if data.get("success", False) and tests_failed == 0 and tests_run > 0:
            print(
                f"{Fore.GREEN}IEEE754 CODEC TEST PASSED ({tests_run} checks){Style.RESET_ALL}"
            )
            return 0

        print(
            f"{Fore.RED}IEEE754 CODEC TEST FAILED"
            f" ({tests_failed}/{tests_run} failures){Style.RESET_ALL}"
        )
        if first_failure:
            print(
                f"   first_failure={first_failure} "
                f"expected=0x{expected_bits:08x} actual=0x{actual_bits:08x}"
            )
        return 1

    except RpcTimeoutError:
        print()
        print(f"{Fore.RED}IEEE754 CODEC TEST TIMEOUT{Style.RESET_ALL}")
        print("   No response from device within 30 seconds")
        return 1
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print()
        print(f"{Fore.RED}IEEE754 CODEC TEST ERROR: {e}{Style.RESET_ALL}")
        return 1
    finally:
        if client is not None:
            await client.close()


async def _run_simd_tests(ctx: RunContext) -> int:
    """Run SIMD test suite via RPC."""
    upload_port = ctx.upload_port
    assert upload_port is not None
    serial_iface = ctx.serial_iface

    print()
    print("=" * 60)
    print("SIMD TEST MODE - Comprehensive Test Suite")
    print("=" * 60)
    print()

    client: RpcClient | None = None
    try:
        print("   Connecting to device...", end="", flush=True)
        client = RpcClient(upload_port, timeout=30.0, serial_interface=serial_iface)
        await client.connect(boot_wait=1.0)
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")

        print("   Sending testSimd RPC...", end="", flush=True)
        response = await client.send_and_match(
            "testSimd", match_key="passed", retries=3
        )
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")
        print()

        total = response.get("totalTests", 0)
        passed_count = response.get("passedTests", 0)
        failed_count = response.get("failedTests", 0)
        failures = response.get("failures", [])

        print(f"   Results: {passed_count}/{total} passed", end="")
        if failed_count > 0:
            print(f", {failed_count} FAILED")
        else:
            print()

        if failures:
            print()
            print(f"   {Fore.RED}Failed tests:{Style.RESET_ALL}")
            for name in failures:
                print(f"     - {name}")

        print()
        simd_passed = response.get("passed", False)

        print("   Sending testSimdBenchmark RPC...", end="", flush=True)
        bench = await client.send_and_match(
            "testSimdBenchmark", match_key="success", retries=2
        )
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")
        print()

        print(json.dumps(bench.data, indent=2))
        print()

        if simd_passed:
            print(f"{Fore.GREEN}SIMD TEST PASSED ({total} tests){Style.RESET_ALL}")
            return 0
        else:
            print(
                f"{Fore.RED}SIMD TEST FAILED"
                f" ({failed_count}/{total} failures){Style.RESET_ALL}"
            )
            return 1

    except RpcTimeoutError:
        print()
        print(f"{Fore.RED}SIMD TEST TIMEOUT{Style.RESET_ALL}")
        print("   No response from device within 30 seconds")
        return 1
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print()
        print(f"{Fore.RED}SIMD TEST ERROR: {e}{Style.RESET_ALL}")
        return 1
    finally:
        if client is not None:
            await client.close()


def _build_and_flash_nxplpc(
    build_dir: Path,
    *,
    environment: str | None,
    upload_port: str | None,
    verbose: bool,
) -> bool:
    """Build via `fbuild build`, then flash + sw-reset via pyocd.

    fbuild's nxplpc orchestrator does not yet implement a deployer
    (daemon/handlers/operations/deploy.rs only wires avr/teensy). Until that
    lands as an upstream fbuild PR, we run the build phase via fbuild and the
    flash/reset phase via pyocd here. The pyocd path is identical to the
    manual bring-up workflow used during initial LPC845 hardware validation.
    """
    import shutil

    env = dict(os.environ)
    # The pinned ArduinoCore-LPC8xx commit only stripped the GitHub-archive
    # wrapping dir in fbuild >= 2.2.27. The bundled venv fbuild was bumped to
    # match; assume the user's PATH-resolved binary is current.

    print("\n📦 Building firmware via fbuild...")
    build_cmd = [
        "fbuild",
        "build",
        "--environment",
        environment or "lpc845brk",
    ]
    if verbose:
        build_cmd.append("--verbose")
    result = subprocess.run(build_cmd, env=env, cwd=str(build_dir))
    if result.returncode != 0:
        print(
            f"{Fore.RED}❌ fbuild build failed (exit {result.returncode}){Style.RESET_ALL}"
        )
        return False

    firmware_bin = (
        build_dir
        / ".fbuild"
        / "build"
        / (environment or "lpc845brk")
        / "release"
        / "firmware.bin"
    )
    if not firmware_bin.is_file():
        print(f"{Fore.RED}❌ firmware.bin not found at {firmware_bin}{Style.RESET_ALL}")
        return False

    pyocd = shutil.which("pyocd")
    if pyocd is None:
        # Fall back to uv-run pyocd (installed on demand). The bring-up
        # workflow already used this pattern.
        pyocd_cmd_prefix = ["uv", "run", "--with", "pyocd", "pyocd"]
    else:
        pyocd_cmd_prefix = [pyocd]

    print("\n📡 Flashing firmware via pyocd...")
    target = "lpc845" if "lpc845" in (environment or "") else "lpc804"
    load_cmd = pyocd_cmd_prefix + [
        "load",
        "--target",
        target,
        "--no-reset",
        str(firmware_bin),
    ]
    result = subprocess.run(load_cmd, env=env)
    if result.returncode != 0:
        print(
            f"{Fore.RED}❌ pyocd load failed (exit {result.returncode}){Style.RESET_ALL}"
        )
        return False

    print("\n🔄 Resetting target via pyocd (sw reset, VCOM-safe)...")
    reset_cmd = pyocd_cmd_prefix + [
        "commander",
        "--target",
        target,
        "-O",
        "reset_type=sw",
        "-c",
        "reset",
        "-c",
        "go",
        "-c",
        "quit",
    ]
    result = subprocess.run(reset_cmd, env=env)
    if result.returncode != 0:
        print(
            f"{Fore.YELLOW}⚠️  pyocd reset returned {result.returncode}; continuing{Style.RESET_ALL}"
        )

    print(f"{Fore.GREEN}✓ Build + flash + reset complete{Style.RESET_ALL}\n")
    return True


async def _run_bring_up_tests(ctx: RunContext) -> int:
    """Run minimal LPC845/LPC804 bring-up validation via JSON-RPC echo.

    Verifies the three transport components the bring-up sketch
    (examples/AutoResearchLpc/) exposes:
      1. Serial.println — the response body itself ("REMOTE: {...}\\r\\n").
      2. FastLED log pipeline — the FL_DBG line from fl::Remote
         ("src/fl/remote/remote.cpp.hpp(151): Stored request ID for echo")
         that emits before the response.
      3. JSON-RPC echo round-trip — TX {method:echo, params:[N], id:I}
         must produce RX {id:I, result:N, jsonrpc:"2.0"}.

    Uses raw serial (not ``RpcClient``) because the bring-up sketch binds
    ``echo`` as ``[](int v) -> int`` which expects a flat ``params:[N]``
    JSON array; ``RpcClient.send`` wraps args one level deeper to support
    the AutoResearch.ino style where bindings take a struct.
    """
    upload_port = ctx.upload_port
    assert upload_port is not None

    print()
    print("=" * 60)
    print("BRING-UP MODE — Serial + FastLED log + JSON-RPC echo")
    print("=" * 60)
    print()

    import serial as _serial  # local — avoid top-level import in this hot path

    sentinel = 4242
    ser = None
    try:
        print("   Opening serial port...", end="", flush=True)
        ser = _serial.Serial(upload_port, 115200, timeout=2)
        # Assert DTR/RTS = True (universal "host ready" idle state).
        # ESP32 native USB CDC tolerates this (it's the post-reset idle state);
        # LPC845-BRK *requires* DTR=True because the LPC11U35 USB-VCOM bridge
        # uses DTR as a flow gate. Previously hardcoded to False here, which
        # silently dropped every byte the LPC845 transmitted and produced the
        # bring-up "zero bytes" false alarm under FastLED #3300 / #3325.
        # See ci/util/pyserial_monitor.py for the same fix on the monitor path,
        # and ci/util/serial_probe.py for the agent-facing helper that mirrors
        # this default.
        ser.dtr = True  # type: ignore[assignment]
        ser.rts = True  # type: ignore[assignment]
        await asyncio.sleep(0.5)
        ser.reset_input_buffer()
        await asyncio.sleep(2.0)  # let boot banner drain
        ser.reset_input_buffer()
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")

        req = (
            '{"jsonrpc":"2.0","method":"echo","params":['
            + str(sentinel)
            + '],"id":1}\n'
        )
        print(f"   TX: {req.strip()}", flush=True)
        ser.write(req.encode())
        ser.flush()

        # Collect for up to 5 seconds or until we see a complete REMOTE: line
        deadline = time.monotonic() + 5.0
        accumulated = b""
        while time.monotonic() < deadline:
            await asyncio.sleep(0.2)
            if ser.in_waiting:
                accumulated += ser.read(ser.in_waiting)
                if (
                    b'"result":' in accumulated
                    and b"\r\n" in accumulated
                    and b"REMOTE:" in accumulated
                ):
                    break

        print(f"   RX: {accumulated!r}", flush=True)
        print()

        # Look for the echo result
        result_token = f'"result":{sentinel}'.encode()
        echo_ok = result_token in accumulated
        # Look for the FL_DBG line from fl::Remote (only present when
        # FASTLED_FORCE_DBG or LARGE_MEMORY — bonus signal, not required)
        dbg_token = b"Stored request ID for echo"
        log_ok = dbg_token in accumulated
        # Look for the FL_WARN_LIT marker the bring-up sketch emits from
        # inside the `echo` handler. Setup-time FL_WARN_LITs are emitted
        # too, but the boot-banner drain above clears them — this token
        # fires per-request and survives the drain. Works on Low-memory
        # targets (FastLED #3002) because FL_WARN_LIT routes through
        # fl::println(const char*) without the sstream/log_emit machinery.
        warn_token = b"FL_WARN: echo invoked"
        warn_ok = warn_token in accumulated
        # Look for the REMOTE: prefix proving Serial.println via the sink
        remote_ok = b"REMOTE: " in accumulated

        passed = echo_ok and remote_ok and warn_ok
        if passed:
            print(f"{Fore.GREEN}BRING-UP TEST PASSED{Style.RESET_ALL}")
            print(
                f"   ✅ Serial.println — REMOTE: response received"
                if remote_ok
                else f"   ❌ Serial.println — REMOTE: prefix missing"
            )
            print(
                f"   ✅ JSON-RPC echo — sentinel {sentinel} round-trip verified"
                if echo_ok
                else f"   ❌ JSON-RPC echo — result mismatch"
            )
            print(
                f"   ✅ FL_WARN_LIT — FastLED warn pipeline reached host"
                if warn_ok
                else f"   ❌ FL_WARN_LIT — warning literal not observed"
            )
            print(
                f"   ✅ FL_DBG — full log pipeline (bonus, LARGE_MEMORY only)"
                if log_ok
                else f"   ⚠️  FL_DBG — full pipeline not active"
                "  (expected on Low-memory targets — FL_WARN_LIT is the gate)"
            )
            print()
            return 0
        else:
            print(f"{Fore.RED}BRING-UP TEST FAILED{Style.RESET_ALL}")
            print(
                f"   echo_ok={echo_ok} remote_ok={remote_ok} "
                f"warn_ok={warn_ok} log_ok={log_ok}"
            )
            return 1
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print()
        print(f"{Fore.RED}BRING-UP TEST ERROR: {e}{Style.RESET_ALL}")
        return 1
    finally:
        if ser is not None:
            ser.close()


async def _run_lpc_pin_toggle_rx_tests(ctx: RunContext) -> int:
    """Run the FastLED #3021 Phase-1 SCT-RX pin-toggle bench.

    Delegates to `ci/autoresearch/test_lpc_pin_toggle_rx.py` so the
    bench logic stays in one place (the script is also runnable
    stand-alone for ad-hoc bring-up). Uses `subprocess` to keep the
    pyserial dependency contained to the script — the autoresearch
    runner itself stays import-free of `serial`.
    """
    upload_port = ctx.upload_port
    assert upload_port is not None

    tx_pin = ctx.args.tx_pin if ctx.args.tx_pin is not None else 10
    rx_pin = ctx.args.rx_pin if ctx.args.rx_pin is not None else 11

    print()
    print("=" * 60)
    print("LPC SCT-RX MODE — pin-toggle TX → SCT-RX loopback (#3021 Phase 1)")
    print(f"   Wiring required: jumper P0_{tx_pin} ↔ P0_{rx_pin} on LPC845-BRK")
    print("=" * 60)
    print()

    cmd = [
        "uv",
        "run",
        "python",
        "ci/autoresearch/test_lpc_pin_toggle_rx.py",
        "--port",
        upload_port,
        "--tx-pin",
        str(tx_pin),
        "--rx-pin",
        str(rx_pin),
    ]
    result = subprocess.run(cmd)
    return 0 if result.returncode == 0 else 1


async def _run_lpc_ws2812_loopback_tests(ctx: RunContext) -> int:
    """Run the FastLED #3021 Phase-2 SCT-RX WS2812 byte-match bench.

    Delegates to `ci/autoresearch/test_lpc_ws2812_loopback.py`. LPC845
    low-memory builds bind `ws2812SctTest` automatically from the platform
    predicate; no manual WS2812 build flag is required.
    """
    final_environment = (ctx.final_environment or "").lower()
    if final_environment not in LPC_WS2812_ENVS:
        print(
            "--ws2812-loopback is only supported on LPC845 boards "
            "(lpc845brk, lpc845, lpcxpresso845max)."
        )
        return 1

    upload_port = ctx.upload_port
    assert upload_port is not None

    tx_pin = ctx.args.tx_pin if ctx.args.tx_pin is not None else 10
    rx_pin = ctx.args.rx_pin if ctx.args.rx_pin is not None else 11

    print()
    print("=" * 60)
    print("LPC SCT-RX MODE — WS2812 byte-match loopback (#3021 Phase 2)")
    print(f"   Wiring required: jumper P0_{tx_pin} ↔ P0_{rx_pin} on LPC845-BRK")
    print("   ws2812SctTest RPC is platform-enabled on LPC845 builds")
    print("=" * 60)
    print()

    cmd = [
        "uv",
        "run",
        "python",
        "ci/autoresearch/test_lpc_ws2812_loopback.py",
        "--port",
        upload_port,
        "--tx-pin",
        str(tx_pin),
        "--rx-pin",
        str(rx_pin),
    ]
    result = subprocess.run(cmd)
    return 0 if result.returncode == 0 else 1


async def _run_lpc_pwm_dma_cl_tests(ctx: RunContext) -> int:
    """Run the FastLED #3468 SCT+DMA channels-API clockless bench.

    Sibling of `_run_lpc_ws2812_loopback_tests`, but exercises the new
    `ChannelEngineLpcSctDma` engine (channels-API path) instead of the
    legacy `ClocklessController` template. LPC845 low-memory builds
    bind the `pwmDmaClFrameOnce` / `pwmDmaClFrameBurst` /
    `pwmDmaClCaptureSelf` handlers automatically when
    `FASTLED_LPC_PWM_DMA` is set at compile time — this phase driver
    injects that macro via `build_flags` before delegating to the
    test runner.
    """
    final_environment = (ctx.final_environment or "").lower()
    if final_environment not in LPC_WS2812_ENVS:
        print(
            "--pwm-dma-cl is only supported on LPC845 boards "
            "(lpc845brk, lpc845, lpcxpresso845max)."
        )
        return 1

    upload_port = ctx.upload_port
    assert upload_port is not None

    tx_pin = ctx.args.tx_pin if ctx.args.tx_pin is not None else 10
    rx_pin = ctx.args.rx_pin if ctx.args.rx_pin is not None else 11

    print()
    print("=" * 60)
    print("LPC SCT+DMA channels-API MODE — self-loopback (#3468)")
    print(f"   Wiring required: jumper P0_{tx_pin} ↔ P0_{rx_pin} on LPC845-BRK")
    print("   Engine: ChannelEngineLpcSctDma via BusTraits<Bus::BIT_BANG>")
    print("   Build flag: -DFASTLED_LPC_PWM_DMA=1 (see")
    print("   examples/AutoResearch/AutoResearchPwmDmaClockless.h)")
    print("=" * 60)
    print()

    cmd = [
        "uv",
        "run",
        "python",
        "ci/autoresearch/test_lpc_pwm_dma_cl.py",
        "--port",
        upload_port,
        "--tx-pin",
        str(tx_pin),
        "--rx-pin",
        str(rx_pin),
    ]
    result = subprocess.run(cmd)
    return 0 if result.returncode == 0 else 1


async def _run_lpc_dma_spi_tests(ctx: RunContext) -> int:
    """Run the FastLED #3456 SPI+DMA async driver bench.

    Sibling of `_run_lpc_pwm_dma_cl_tests`, but exercises the LPC845
    `ARMHardwareSPIOutputDMA<>` driver from `spi_arm_lpc_dma.h`.
    Phase 1 of the #3453 bench bring-up series. LPC845 low-memory
    builds bind the `dmaSpiTransferOnce` / `dmaSpiTransferOverlap` /
    `dmaSpiMeasureSck` handlers automatically when `FASTLED_LPC_SPI_DMA`
    is set at compile time — this phase driver expects that flag to
    already be present in `build_flags`.

    Compile-time build flag: `-DFASTLED_LPC_SPI_DMA=1` (optionally
    combined with `-DFASTLED_LPC_SPI_DMA_CHANNEL=4` for SPI1). See
    `examples/AutoResearch/AutoResearchSpiDma.h`.
    """
    final_environment = (ctx.final_environment or "").lower()
    if final_environment not in LPC_WS2812_ENVS:
        print(
            "--dma-spi is only supported on LPC845 boards "
            "(lpc845brk, lpc845, lpcxpresso845max)."
        )
        return 1

    upload_port = ctx.upload_port
    assert upload_port is not None

    print()
    print("=" * 60)
    print("LPC SPI+DMA async driver bench — #3456 (Phase 1 of #3453)")
    print(
        "   Driver: ARMHardwareSPIOutputDMA<> (src/platforms/arm/lpc/spi_arm_lpc_dma.h)"
    )
    print("   Build flag: -DFASTLED_LPC_SPI_DMA=1 (see")
    print("   examples/AutoResearch/AutoResearchSpiDma.h)")
    print("   Optional: -DFASTLED_LPC_SPI_DMA_CHANNEL=4 (SPI1 default)")
    print("   Wiring: no jumper required for transferOnce/Overlap timing;")
    print("   SCK measurement is wall-clock derived, not SCT-captured.")
    print("=" * 60)
    print()

    cmd = [
        "uv",
        "run",
        "python",
        "ci/autoresearch/test_lpc_dma_spi.py",
        "--port",
        upload_port,
    ]
    result = subprocess.run(cmd)
    return 0 if result.returncode == 0 else 1


async def _run_coroutine_tests(ctx: RunContext) -> int:
    """Run coroutine test suite via RPC."""
    upload_port = ctx.upload_port
    assert upload_port is not None
    serial_iface = ctx.serial_iface

    print()
    print("=" * 60)
    print("COROUTINE TEST MODE - Task Creation, Stop, Await")
    print("=" * 60)
    print()

    client: RpcClient | None = None
    try:
        print("   Connecting to device...", end="", flush=True)
        client = RpcClient(upload_port, timeout=60.0, serial_interface=serial_iface)
        await client.connect(boot_wait=1.0)
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")

        print("   Sending testCoroutineAll RPC...", end="", flush=True)
        response = await client.send_and_match(
            "testCoroutineAll", match_key="passed", retries=3
        )
        print(f" {Fore.GREEN}ok{Style.RESET_ALL}")
        print()

        total = response.get("total", 0)
        passed_count = response.get("passed", 0)
        failed_count = response.get("failed", 0)
        results = response.get("results", {})

        print(f"   Results: {passed_count}/{total} passed", end="")
        if failed_count > 0:
            print(f", {failed_count} FAILED")
        else:
            print()

        if isinstance(results, dict):
            for test_name, test_result in results.items():
                if isinstance(test_result, dict):
                    success = test_result.get("success", False)
                    duration = test_result.get("durationMs", "?")
                    status = (
                        f"{Fore.GREEN}PASS{Style.RESET_ALL}"
                        if success
                        else f"{Fore.RED}FAIL{Style.RESET_ALL}"
                    )
                    print(f"     {status} {test_name} ({duration}ms)")
                    if not success and "error" in test_result:
                        print(f"          {test_result['error']}")

        print()
        if response.get("success", False):
            print(f"{Fore.GREEN}COROUTINE TEST PASSED ({total} tests){Style.RESET_ALL}")
            return 0
        else:
            print(
                f"{Fore.RED}COROUTINE TEST FAILED"
                f" ({failed_count}/{total} failures){Style.RESET_ALL}"
            )
            return 1

    except RpcTimeoutError:
        print()
        print(f"{Fore.RED}COROUTINE TEST TIMEOUT{Style.RESET_ALL}")
        print("   No response from device within 60 seconds")
        return 1
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print()
        print(f"{Fore.RED}COROUTINE TEST ERROR: {e}{Style.RESET_ALL}")
        return 1
    finally:
        if client is not None:
            await client.close()


_TEST_RPC_METHODS = frozenset({"runSingleTest", "runParallelTest"})


def _is_plain_int(value: Any) -> TypeGuard[int]:
    return isinstance(value, int) and not isinstance(value, bool)


def _expected_test_drivers(method: str, cmd: dict[str, Any]) -> list[str]:
    params = cmd.get("params")
    if not isinstance(params, dict):
        return []

    if method == "runSingleTest":
        driver = params.get("driver")
        return [driver] if isinstance(driver, str) and driver else []

    if method == "runParallelTest":
        drivers = params.get("drivers")
        if not isinstance(drivers, list):
            return []
        expected: list[str] = []
        for entry in drivers:
            if isinstance(entry, dict):
                driver = entry.get("driver")
                if isinstance(driver, str) and driver:
                    expected.append(driver)
        return expected

    return []


def _actual_test_drivers(method: str, data: dict[str, Any]) -> list[str]:
    if method == "runSingleTest":
        driver = data.get("driver")
        return [driver] if isinstance(driver, str) and driver else []

    if method == "runParallelTest":
        drivers = data.get("drivers")
        if not isinstance(drivers, list):
            return []
        actual: list[str] = []
        for entry in drivers:
            if isinstance(entry, dict):
                driver = entry.get("driver")
                if isinstance(driver, str) and driver:
                    actual.append(driver)
        return actual

    return []


def _set_pins_rpc_command(tx_pin: int, rx_pin: int) -> dict[str, Any]:
    return {"method": "setPins", "params": [{"txPin": tx_pin, "rxPin": rx_pin}]}


def _expected_setup_pins(
    method: str, cmd: dict[str, Any]
) -> tuple[int | None, int | None]:
    params = cmd.get("params")
    if method == "setPins":
        if (
            isinstance(params, list)
            and len(params) == 1
            and isinstance(params[0], dict)
        ):
            tx_pin = params[0].get("txPin")
            rx_pin = params[0].get("rxPin")
            return (
                tx_pin if _is_plain_int(tx_pin) else None,
                rx_pin if _is_plain_int(rx_pin) else None,
            )
        if isinstance(params, list) and len(params) == 2:
            tx_pin, rx_pin = params
            return (
                tx_pin if _is_plain_int(tx_pin) else None,
                rx_pin if _is_plain_int(rx_pin) else None,
            )
        return None, None

    if not isinstance(params, list) or len(params) != 1:
        return None, None

    pin = params[0]
    if not _is_plain_int(pin):
        return None, None

    if method == "setTxPin":
        return pin, None
    if method == "setRxPin":
        return None, pin
    return None, None


def _validate_setup_rpc_response(
    method: str, cmd: dict[str, Any], data: dict[str, Any]
) -> list[str]:
    """Return validation errors for setup RPCs that must not drift silently."""
    errors: list[str] = []
    expected_tx_pin, expected_rx_pin = _expected_setup_pins(method, cmd)

    for field, expected in (
        ("txPin", expected_tx_pin),
        ("rxPin", expected_rx_pin),
    ):
        if expected is None:
            continue
        value = data.get(field)
        if not _is_plain_int(value):
            errors.append(f"missing integer {field}")
        elif value != expected:
            errors.append(f"{field}={value} does not match expected {expected}")

    return errors


def _ensure_leading_set_pins_command(ctx: RunContext) -> None:
    if ctx.effective_tx_pin is None or ctx.effective_rx_pin is None:
        return

    set_pins_cmd = _set_pins_rpc_command(ctx.effective_tx_pin, ctx.effective_rx_pin)
    if ctx.json_rpc_commands and ctx.json_rpc_commands[0].get("method") == "setPins":
        ctx.json_rpc_commands[0] = set_pins_cmd
        return
    ctx.json_rpc_commands.insert(0, set_pins_cmd)


def _expected_test_pins(
    method: str,
    cmd: dict[str, Any],
    default_tx_pin: int | None,
    default_rx_pin: int | None,
) -> tuple[int | None, int | None]:
    params = cmd.get("params")
    if not isinstance(params, dict):
        return default_tx_pin, default_rx_pin

    if method == "runParallelTest":
        drivers = params.get("drivers")
        if isinstance(drivers, list) and drivers and isinstance(drivers[0], dict):
            tx_pin = drivers[0].get("pinTx")
            if _is_plain_int(tx_pin):
                return tx_pin, default_rx_pin
        return default_tx_pin, default_rx_pin

    tx_pin = params.get("pinTx")
    rx_pin = params.get("pinRx")
    return (
        tx_pin if _is_plain_int(tx_pin) else default_tx_pin,
        rx_pin if _is_plain_int(rx_pin) else default_rx_pin,
    )


def _validate_test_rpc_response(
    method: str,
    cmd: dict[str, Any],
    data: dict[str, Any],
    expected_tx_pin: int | None,
    expected_rx_pin: int | None,
) -> list[str]:
    """Return validation errors that prevent a test RPC from proving PASS."""
    errors: list[str] = []

    if not isinstance(data.get("success"), bool):
        errors.append("missing boolean success")
    if not isinstance(data.get("passed"), bool):
        errors.append("missing boolean passed")

    total_tests = data.get("totalTests")
    passed_tests = data.get("passedTests")
    if not _is_plain_int(total_tests):
        errors.append("missing integer totalTests")
    elif total_tests <= 0:
        errors.append("totalTests must be > 0")

    if not _is_plain_int(passed_tests):
        errors.append("missing integer passedTests")
    elif _is_plain_int(total_tests):
        if passed_tests < 0:
            errors.append("passedTests must be >= 0")
        if passed_tests > total_tests:
            errors.append("passedTests must be <= totalTests")
        if data.get("passed") is True and passed_tests != total_tests:
            errors.append("passed=true requires passedTests == totalTests")

    expected_drivers = _expected_test_drivers(method, cmd)
    actual_drivers = _actual_test_drivers(method, data)
    if not actual_drivers:
        field = "driver" if method == "runSingleTest" else "drivers"
        errors.append(f"missing selected {field}")
    else:
        for driver in expected_drivers:
            if driver not in actual_drivers:
                errors.append(f"missing expected driver {driver}")

    expected_tx_pin, expected_rx_pin = _expected_test_pins(
        method, cmd, expected_tx_pin, expected_rx_pin
    )
    if expected_tx_pin is not None or expected_rx_pin is not None:
        capture_backend = data.get("captureBackend")
        if not isinstance(capture_backend, str) or not capture_backend:
            errors.append("missing string captureBackend")
        if data.get("passed") is True:
            capture_evidence_bytes = data.get("captureEvidenceBytes")
            capture_evidence_raw_edges = data.get("captureEvidenceRawEdges")
            has_capture_bytes = (
                _is_plain_int(capture_evidence_bytes) and capture_evidence_bytes > 0
            )
            has_capture_edges = (
                _is_plain_int(capture_evidence_raw_edges)
                and capture_evidence_raw_edges > 0
            )
            if not has_capture_bytes and not has_capture_edges:
                errors.append("passed test response requires nonzero capture evidence")

    for field, expected in (
        ("requestedTxPin", expected_tx_pin),
        ("requestedRxPin", expected_rx_pin),
        ("actualTxPin", expected_tx_pin),
        ("actualRxPin", expected_rx_pin),
    ):
        if expected is None:
            continue
        value = data.get(field)
        if not _is_plain_int(value):
            errors.append(f"missing integer {field}")
        elif expected is not None and value != expected:
            errors.append(f"{field}={value} does not match expected {expected}")

    return errors


def _classify_test_failure(data: dict[str, Any]) -> tuple[str, str]:
    """Classify a failed test response using structured RPC evidence."""
    patterns = data.get("patterns")
    if isinstance(patterns, list) and patterns:
        saw_pattern = False
        saw_capture_bytes = False
        saw_raw_edges = False
        saw_mismatched_bytes = False
        saw_capture_failure = False

        for pattern in patterns:
            if not isinstance(pattern, dict):
                continue
            saw_pattern = True
            captured = pattern.get("capturedBytes")
            raw_edges = pattern.get("rawEdgesAfterWait")
            mismatched = pattern.get("mismatchedBytes")
            if _is_plain_int(captured) and captured > 0:
                saw_capture_bytes = True
            if _is_plain_int(raw_edges) and raw_edges > 0:
                saw_raw_edges = True
            if _is_plain_int(mismatched) and mismatched > 0:
                saw_mismatched_bytes = True
            if pattern.get("captureFailed") is True:
                saw_capture_failure = True

        if saw_pattern:
            if not saw_capture_bytes and not saw_raw_edges:
                return (
                    "zero_capture",
                    "RX produced no raw edges or decodable bytes",
                )
            if saw_mismatched_bytes or saw_capture_failure:
                return (
                    "decode_mismatch",
                    "RX captured signal/data but decoded output did not match",
                )

    evidence_bytes = data.get("captureEvidenceBytes")
    evidence_edges = data.get("captureEvidenceRawEdges")
    has_capture_bytes = _is_plain_int(evidence_bytes) and evidence_bytes > 0
    has_capture_edges = _is_plain_int(evidence_edges) and evidence_edges > 0
    if data.get("passed") is False:
        if not has_capture_bytes and not has_capture_edges:
            return ("zero_capture", "test failed with no positive capture evidence")
        return ("decode_mismatch", "test failed despite positive capture evidence")

    return ("test_failed", "test response reported failure")


async def _run_rpc_tests(ctx: RunContext, qctx: QuietContext) -> int:
    """Execute main RPC test loop."""
    upload_port = ctx.upload_port
    assert upload_port is not None
    serial_iface = ctx.serial_iface
    use_fbuild = ctx.use_fbuild

    if upload_port and not use_fbuild:
        kill_port_users(upload_port)
        time.sleep(0.5)

    if ctx.effective_tx_pin is not None and ctx.effective_rx_pin is not None:
        qctx.emit(f"PINS tx={ctx.effective_tx_pin} rx={ctx.effective_rx_pin}")

    print()
    print("=" * 60)
    print("EXECUTING AUTORESEARCH TESTS VIA RPC")
    print("=" * 60)

    test_failed = False
    stop_word_found: str | None = None
    json_rpc_commands = ctx.json_rpc_commands
    crash_decoder = ctx.crash_decoder

    client: RpcClient | None = None
    try:
        if ctx.discovery_client:
            client = ctx.discovery_client
        else:
            print(f"\U0001f4e1 Connecting to {upload_port}...")
            client = RpcClient(
                upload_port,
                timeout=ctx.timeout_seconds,
                serial_interface=serial_iface,
                verbose=True,
                crash_decoder=crash_decoder,
            )
            await client.connect(boot_wait=1.0, drain_boot=True)
        print(f"{Fore.GREEN}\u2713 Connected{Style.RESET_ALL}")

        print(f"\n\U0001f527 Executing {len(json_rpc_commands)} RPC command(s)...")
        print("\u2500" * 60)

        for i, cmd in enumerate(json_rpc_commands, 1):
            method = cmd.get("method", "unknown")
            params = cmd.get("params", [])
            setup_method = method in {
                "setPins",
                "setTxPin",
                "setRxPin",
                "setLaneSizes",
                "setSolidColor",
            }
            test_method = method in _TEST_RPC_METHODS

            print(f"\n[{i}/{len(json_rpc_commands)}] Calling {method}()...")

            try:
                # Per-RPC budget = TIME REMAINING UNTIL ctx.deadline_epoch.
                # `--timeout N` is a TOTAL wall-clock budget from the post-
                # flash epoch (not a per-call value), so each call shrinks
                # the remaining window. Previously this site hardcoded
                # 120.0 (ignoring --timeout entirely) and PR #3219 first
                # changed it to `ctx.timeout_seconds` (still per-call, not
                # decrementing) -- both let runs blow well past the user's
                # stated budget. The watchdog thread enforces a hard
                # ceiling at `deadline_epoch + WATCHDOG_GRACE_SECONDS`.
                response = await client.send(
                    method,
                    args=params if params else [],
                    timeout=ctx.remaining_seconds(minimum=1.0),
                )

                test_data = response.data

                if not isinstance(test_data, dict):
                    print(
                        f"{Fore.YELLOW}\u26a0\ufe0f  Unexpected response type: {type(test_data)}{Style.RESET_ALL}"
                    )
                    print(f"   Response: {test_data}")
                    if test_method:
                        test_failed = True
                        stop_word_found = "ERROR"
                        break
                    continue

                _driver = test_data.get("driver", method)
                _lanes = test_data.get("laneCount", "?")
                _leds = sum(test_data.get("laneSizes", [0]))
                _dur = test_data.get("duration_ms", "?")

                if test_data.get("success") is False or "error" in test_data:
                    stop_word_found = "ERROR"
                    test_failed = True
                    print(f"{Fore.RED}\u274c RPC command failed{Style.RESET_ALL}")
                    if "error" in test_data:
                        print(f"   Error: {test_data['error']}")
                    if "message" in test_data:
                        print(f"   Message: {test_data['message']}")
                    if setup_method:
                        break

                elif setup_method and (
                    validation_errors := _validate_setup_rpc_response(
                        method,
                        cmd,
                        test_data,
                    )
                ):
                    stop_word_found = "ERROR"
                    test_failed = True
                    print(f"{Fore.RED}\u274c Setup response mismatch{Style.RESET_ALL}")
                    print("   Failure class: pin_state_mismatch")
                    for error in validation_errors:
                        print(f"   - {error}")
                    qctx.emit(f"FAILURE class=pin_state_mismatch method={method}")
                    break

                elif test_method and (
                    validation_errors := _validate_test_rpc_response(
                        method,
                        cmd,
                        test_data,
                        ctx.effective_tx_pin,
                        ctx.effective_rx_pin,
                    )
                ):
                    stop_word_found = "ERROR"
                    test_failed = True
                    print(f"{Fore.RED}\u274c Malformed test response{Style.RESET_ALL}")
                    print("   Failure class: malformed_response")
                    for error in validation_errors:
                        print(f"   - {error}")
                    qctx.emit(f"FAILURE class=malformed_response method={method}")
                    break

                elif test_data.get("success") and test_data.get("passed"):
                    stop_word_found = "OK"
                    print(f"{Fore.GREEN}\u2705 Test passed{Style.RESET_ALL}")
                    qctx.emit(
                        f"TEST {_driver} lanes={_lanes} leds={_leds} PASS {_dur}ms"
                    )

                    if "passedTests" in test_data and "totalTests" in test_data:
                        print(
                            f"   Tests: {test_data['passedTests']}/{test_data['totalTests']} passed"
                        )
                    if "duration_ms" in test_data:
                        duration_str = f"{test_data['duration_ms']}ms"
                        if "show_duration_ms" in test_data:
                            duration_str += (
                                f" (show: {test_data['show_duration_ms']}ms)"
                            )
                        elif "show_duration_us" in test_data:
                            duration_str += (
                                f" (show: {test_data['show_duration_us']}us)"
                            )
                        print(f"   Duration: {duration_str}")

                    display_tight_timing(test_data)
                    display_objectfled_diagnostics(test_data)

                    if "drivers" in test_data and isinstance(
                        test_data["drivers"], list
                    ):
                        drv_names = [d.get("driver", "?") for d in test_data["drivers"]]
                        print(f"   Parallel drivers: {', '.join(drv_names)}")
                        if test_data.get("rx_validation_attempted"):
                            rx_status = (
                                "passed"
                                if test_data.get("rx_validation_passed")
                                else "failed"
                            )
                            print(f"   RX validation: {rx_status}")

                    if "patterns" in test_data:
                        display_pattern_details(test_data)

                elif (
                    test_data.get("success")
                    and "passed" in test_data
                    and not test_data.get("passed")
                ):
                    stop_word_found = "ERROR"
                    test_failed = True
                    failure_class, failure_detail = _classify_test_failure(test_data)
                    print(f"{Fore.RED}\u274c Test failed{Style.RESET_ALL}")
                    print(f"   Failure class: {failure_class}")
                    print(f"   Detail: {failure_detail}")
                    _err_detail = ""
                    if "firstFailure" in test_data:
                        _err_detail = f" {test_data['firstFailure'].get('pattern', '')}"
                    qctx.emit(
                        f"TEST {_driver} lanes={_lanes} leds={_leds} FAIL {_dur}ms "
                        f"class={failure_class}{_err_detail}"
                    )

                    if "firstFailure" in test_data:
                        failure = test_data["firstFailure"]
                        print(f"   Failed pattern: {failure.get('pattern', 'unknown')}")
                        print(f"   Lane: {failure.get('lane', 'unknown')}")
                        print(f"   Expected: {failure.get('expected', 'unknown')}")
                        print(f"   Actual: {failure.get('actual', 'unknown')}")

                    display_tight_timing(test_data)
                    display_objectfled_diagnostics(test_data)

                    if "patterns" in test_data:
                        display_pattern_details(test_data)

                else:
                    print(f"{Fore.GREEN}\u2713 Command completed{Style.RESET_ALL}")
                    if test_data:
                        print(f"   Response: {test_data}")
                    if not setup_method:
                        stop_word_found = "OK"

            except RpcCrashError as crash_err:
                print(
                    f"{Fore.RED}\u274c Device crashed during {method}(){Style.RESET_ALL}"
                )
                print("   Failure class: crash")
                if not crash_err.decoded_lines:
                    print("   (no decoded stack trace available)")
                qctx.emit(f"FAILURE class=crash method={method}")
                test_failed = True
                stop_word_found = "ERROR"
                break

            except RpcTimeoutError:
                print(f"{Fore.RED}\u274c RPC timeout{Style.RESET_ALL}")
                print(f"   No response within {120}s")
                print("   Failure class: timeout")
                qctx.emit(f"FAILURE class=timeout method={method}")
                test_failed = True
                stop_word_found = "ERROR"
                break

            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise

            except Exception as e:
                print(f"{Fore.RED}\u274c RPC error: {e}{Style.RESET_ALL}")
                test_failed = True
                stop_word_found = "ERROR"
                break

    except KeyboardInterrupt as ki:
        print("\n\n\u26a0\ufe0f  Interrupted by user")
        handle_keyboard_interrupt(ki)
        return 130
    except Exception as e:
        print(f"\n{Fore.RED}\u274c RPC client error: {e}{Style.RESET_ALL}")
        return 1
    finally:
        # Grant the cleanup phase 10s of additional runway BUT keep the
        # watchdog armed. If `await client.close()` itself wedges (which
        # is the exact failure mode that motivated the watchdog), the
        # force-kill still fires after deadline_epoch+10+grace. Per
        # user spec 2026-06-22: never disarm the bomb, just give it
        # more fuse when we're entering a known-slow phase.
        extend_autoresearch_watchdog_deadline(ctx, 10.0)
        if client is not None:
            await client.close()
            print(f"\n\u2705 RPC connection closed")

    print("\u2500" * 60)

    if test_failed:
        print(f"\n{Fore.RED}\u274c AUTORESEARCH FAILED{Style.RESET_ALL}")
        return 1

    if not stop_word_found:
        print(
            f"\n{Fore.YELLOW}\u26a0\ufe0f  No test completion signal received{Style.RESET_ALL}"
        )
        return 1

    if stop_word_found == "OK":
        print()
        print("=" * 60)
        print(f"{Fore.GREEN}\u2713 AUTORESEARCH SUCCEEDED{Style.RESET_ALL}")
        print("=" * 60)
        qctx.emit(f"RESULT PASS {len(json_rpc_commands)} test(s)")
        return 0
    elif stop_word_found == "ERROR":
        print()
        print("=" * 60)
        print(f"{Fore.RED}\u2717 AUTORESEARCH FAILED{Style.RESET_ALL}")
        print("=" * 60)
        qctx.emit(f"RESULT FAIL {len(json_rpc_commands)} test(s)")
        qctx.emit_log_path()
        return 1

    # Fallback
    if stop_word_found == "ERROR":
        print(f"\n{Fore.RED}FAILED{Style.RESET_ALL} Some autoresearch tests failed")
        return 1
    else:
        print(f"\n{Fore.GREEN}PASSED{Style.RESET_ALL} All autoresearch tests completed")
        return 0
