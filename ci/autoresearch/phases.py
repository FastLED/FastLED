"""Phase functions for the autoresearch pipeline.

Each phase is an async function that takes RunContext and returns
None (success) or int (exit code on failure).
"""

from __future__ import annotations

import json
import os
import subprocess
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any

from colorama import Fore, Style

from ci.autoresearch.args import Args
from ci.autoresearch.build_driver import select_build_driver
from ci.autoresearch.context import (
    DEFAULT_EXPECT_PATTERNS,
    DEFAULT_FAIL_ON_PATTERN,
    EXIT_ON_ERROR_PATTERNS,
    PIN_RX,
    PIN_TX,
    QuietContext,
    RunContext,
    display_pattern_details,
)
from ci.autoresearch.gpio import run_gpio_pretest, run_pin_discovery
from ci.debug_attached import run_cpp_lint
from ci.rpc_client import RpcClient, RpcCrashError, RpcTimeoutError
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


def _is_native_platform(environment: str | None) -> bool:
    """Check if the target is the native/stub (host) platform."""
    if not environment:
        return False
    return environment.lower() in ("native", "stub", "host")


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


def _try_teensy_bootloader_upload(build_dir: Path, environment: str | None) -> bool:
    """Try to detect Teensy in HalfKay bootloader mode and upload firmware."""
    if not environment:
        return False

    loader_paths = [
        Path.home()
        / ".platformio"
        / "packages"
        / "tool-teensy"
        / "teensy_loader_cli.exe",
        Path.home() / ".platformio" / "packages" / "tool-teensy" / "teensy_loader_cli",
    ]
    loader = None
    for p in loader_paths:
        if p.exists():
            loader = p
            break
    if not loader:
        return False

    hex_path = build_dir / ".pio" / "build" / environment / "firmware.hex"
    if not hex_path.exists():
        return False

    mcu_map = {
        "teensy41": "TEENSY41",
        "teensy40": "TEENSY40",
        "teensylc": "TEENSYLC",
        "teensy36": "TEENSY36",
        "teensy35": "TEENSY35",
        "teensy31": "TEENSY31",
    }
    mcu = mcu_map.get(environment.lower())
    if not mcu:
        return False

    try:
        result = subprocess.run(
            [str(loader), f"--mcu={mcu}", "-v", str(hex_path)],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0 and "Programming" in result.stdout:
            return True
    except (subprocess.TimeoutExpired, OSError):
        pass

    return False


# ============================================================
# Phase A: Parse args and build commands
# ============================================================


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

    if args.all:
        drivers = [
            "PARLIO",
            "RMT",
            "SPI",
            "UART",
            "LCD_CLOCKLESS",
            "LCD_SPI",
            "LCD_RGB",
            "OBJECTFLED",
        ]
    else:
        if args.parlio:
            drivers.append("PARLIO")
        if args.rmt:
            drivers.append("RMT")
        if args.spi:
            drivers.append("SPI")
        if args.uart:
            drivers.append("UART")
        if args.lcd:
            drivers.append("LCD_CLOCKLESS")
        if args.lcd_spi:
            drivers.append("LCD_SPI")
        if args.lcd_rgb:
            drivers.append("LCD_RGB")
        if args.object_fled:
            drivers.append("OBJECTFLED")

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
    ) and (drivers or simd_test_mode or coroutine_test_mode):
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
        if max_lanes is not None and max_lanes > 1:
            print(
                "\u274c Error: --legacy only supports single-lane (pin 0-8). Remove --lanes or use --lanes 1"
            )
            return 1
        min_lanes = 1
        max_lanes = 1
        print(
            "\u2139\ufe0f  Legacy API mode: using WS2812B<PIN> template path (single-lane, pin 0-8)"
        )

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
            if len(per_lane_counts) < 1 or len(per_lane_counts) > 8:
                print(
                    f"\u274c Error: Lane count must be 1-8, got {len(per_lane_counts)} lanes"
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

    # Parse timeout
    try:
        timeout_seconds = parse_timeout(args.timeout)
    except ValueError as e:
        print(f"\u274c Error: {e}")
        return 1

    # Validate project directory
    build_dir = args.project_dir.resolve()
    if not (build_dir / "platformio.ini").exists():
        print(f"\u274c Error: platformio.ini not found in {build_dir}")
        print("   Make sure you're running this from a PlatformIO project directory")
        return 1

    sketch_path = build_dir / "examples" / "AutoResearch"
    if not sketch_path.exists():
        print(f"\u274c Error: AutoResearch sketch not found at {sketch_path}")
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
        max_wait_s = 60
        poll_interval_s = 1.0
        result = auto_detect_upload_port()
        if not result.ok:
            if is_teensy:
                print(f"\n{Fore.YELLOW}{'=' * 60}")
                print(f"  Teensy not detected on USB.")
                print(f"  Press the PROGRAM button on the Teensy to enter bootloader.")
                print(f"{'=' * 60}{Style.RESET_ALL}\n")
            print(
                f"\u23f3 No USB serial port found yet. Waiting up to {max_wait_s}s for device..."
            )
            deadline = time.monotonic() + max_wait_s
            last_msg_at = 0.0
            while time.monotonic() < deadline:
                time.sleep(poll_interval_s)
                result = auto_detect_upload_port()
                if result.ok:
                    elapsed = max_wait_s - (deadline - time.monotonic())
                    print(
                        f"\u2705 USB serial port detected after {elapsed:.1f}s: {result.selected_port}"
                    )
                    break
                if is_teensy:
                    teensy_result = _try_teensy_bootloader_upload(
                        ctx.build_dir, ctx.final_environment
                    )
                    if teensy_result:
                        print(
                            "\u2705 Firmware uploaded via Teensy bootloader, waiting for serial port..."
                        )
                        serial_deadline = time.monotonic() + 15
                        while time.monotonic() < serial_deadline:
                            time.sleep(1.0)
                            result = auto_detect_upload_port()
                            if result.ok:
                                print(
                                    f"\u2705 Teensy serial port detected: {result.selected_port}"
                                )
                                break
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
                    f"{Fore.YELLOW}Teensy hint: Hold the PROGRAM button while plugging in USB to force bootloader mode.{Style.RESET_ALL}"
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
        print()

    # Platform mismatch warning
    from ci.util.pio_package_daemon import get_default_environment

    default_env = get_default_environment(str(ctx.build_dir))
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
    upload_port = ctx.upload_port
    build_driver = ctx.build_driver
    assert build_driver is not None

    # Phase 0: Package Installation
    if not build_driver.install_packages(build_dir, final_environment):
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

    if not build_driver.deploy(
        build_dir,
        environment=final_environment,
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
    constrained_platforms = ["esp32c6", "esp32c2", "teensy41", "teensy40"]
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

    ctx.serial_iface = create_serial_interface(
        port=upload_port, use_pyserial=not use_fbuild
    )

    # Create crash trace decoder
    if final_environment:
        ctx.crash_decoder = CrashTraceDecoder(
            ctx.build_dir, final_environment, use_fbuild=use_fbuild
        )

    serial_iface = ctx.serial_iface

    # Pin discovery
    if ctx.simd_test_mode:
        print("\n\U0001f4cc SIMD mode: skipping pin discovery and GPIO pre-test")
    elif ctx.coroutine_test_mode:
        print("\n\U0001f4cc Coroutine mode: skipping pin discovery and GPIO pre-test")
    elif ctx.net_server_mode or ctx.net_client_mode or ctx.net_loopback_mode:
        print("\n\U0001f4cc Network mode: skipping pin discovery and GPIO pre-test")
    elif ctx.ota_mode:
        print("\n\U0001f4cc OTA mode: skipping pin discovery and GPIO pre-test")
    elif ctx.ble_mode:
        print("\n\U0001f4cc BLE mode: skipping pin discovery and GPIO pre-test")
    elif args.tx_pin is not None or args.rx_pin is not None:
        ctx.effective_tx_pin = args.tx_pin if args.tx_pin is not None else PIN_TX
        ctx.effective_rx_pin = args.rx_pin if args.rx_pin is not None else PIN_RX
        print(
            f"\n\U0001f4cc Using CLI-specified pins: TX={ctx.effective_tx_pin}, RX={ctx.effective_rx_pin}"
        )
        set_pins_cmd = {
            "method": "setPins",
            "params": [{"txPin": ctx.effective_tx_pin, "rxPin": ctx.effective_rx_pin}],
        }
        ctx.json_rpc_commands.insert(0, set_pins_cmd)
    elif args.auto_discover_pins:
        print("\n\U0001f50d Auto-discovery enabled - searching for connected pins...")
        pin_discovery = await run_pin_discovery(
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
            ctx.effective_tx_pin = PIN_TX
            ctx.effective_rx_pin = PIN_RX
            print(
                f"\U0001f4cc Using default pins: TX={ctx.effective_tx_pin}, RX={ctx.effective_rx_pin}"
            )
            if ctx.discovery_client:
                await ctx.discovery_client.close()
                ctx.discovery_client = None
    else:
        ctx.effective_tx_pin = PIN_TX
        ctx.effective_rx_pin = PIN_RX
        print(
            f"\n\U0001f4cc Using default pins: TX={ctx.effective_tx_pin}, RX={ctx.effective_rx_pin}"
        )

    # GPIO connectivity pre-test
    if ctx.simd_test_mode:
        pass
    elif ctx.coroutine_test_mode:
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
        ctx.effective_tx_pin or PIN_TX,
        ctx.effective_rx_pin or PIN_RX,
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
        tx = ctx.effective_tx_pin if ctx.effective_tx_pin is not None else PIN_TX
        rx = ctx.effective_rx_pin if ctx.effective_rx_pin is not None else PIN_RX
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

    # Main RPC test execution
    return await _run_rpc_tests(ctx, qctx)


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

            print(f"\n[{i}/{len(json_rpc_commands)}] Calling {method}()...")

            try:
                response = await client.send(
                    method,
                    args=params if params else [],
                    timeout=120.0,
                )

                test_data = response.data

                if not isinstance(test_data, dict):
                    print(
                        f"{Fore.YELLOW}\u26a0\ufe0f  Unexpected response type: {type(test_data)}{Style.RESET_ALL}"
                    )
                    print(f"   Response: {test_data}")
                    continue

                _driver = test_data.get("driver", method)
                _lanes = test_data.get("laneCount", "?")
                _leds = sum(test_data.get("laneSizes", [0]))
                _dur = test_data.get("duration_ms", "?")

                if test_data.get("success") and test_data.get("passed"):
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

                elif test_data.get("success") and not test_data.get("passed"):
                    stop_word_found = "ERROR"
                    test_failed = True
                    print(f"{Fore.RED}\u274c Test failed{Style.RESET_ALL}")
                    _err_detail = ""
                    if "firstFailure" in test_data:
                        _err_detail = f" {test_data['firstFailure'].get('pattern', '')}"
                    qctx.emit(
                        f"TEST {_driver} lanes={_lanes} leds={_leds} FAIL {_dur}ms{_err_detail}"
                    )

                    if "firstFailure" in test_data:
                        failure = test_data["firstFailure"]
                        print(f"   Failed pattern: {failure.get('pattern', 'unknown')}")
                        print(f"   Lane: {failure.get('lane', 'unknown')}")
                        print(f"   Expected: {failure.get('expected', 'unknown')}")
                        print(f"   Actual: {failure.get('actual', 'unknown')}")

                    if "patterns" in test_data:
                        display_pattern_details(test_data)

                elif test_data.get("success") is False:
                    stop_word_found = "ERROR"
                    test_failed = True
                    print(f"{Fore.RED}\u274c RPC command failed{Style.RESET_ALL}")
                    if "error" in test_data:
                        print(f"   Error: {test_data['error']}")

                else:
                    stop_word_found = "OK"
                    print(f"{Fore.GREEN}\u2713 Command completed{Style.RESET_ALL}")
                    if test_data:
                        print(f"   Response: {test_data}")

            except RpcCrashError as crash_err:
                print(
                    f"{Fore.RED}\u274c Device crashed during {method}(){Style.RESET_ALL}"
                )
                if not crash_err.decoded_lines:
                    print("   (no decoded stack trace available)")
                test_failed = True
                stop_word_found = "ERROR"
                break

            except RpcTimeoutError:
                print(f"{Fore.RED}\u274c RPC timeout{Style.RESET_ALL}")
                print(f"   No response within {120}s")
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
