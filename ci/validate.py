#!/usr/bin/env python3
"""FastLED Validation Test Runner - JSON-RPC Scripting Language.

This script orchestrates hardware-in-the-loop testing using a JSON-RPC scripting
model. All test coordination happens via RPC commands and responses:

JSON-RPC Workflow (Fail-Fast Model):
    1. findConnectedPins()       - Auto-discover TX-RX jumper wire
       → Returns: {found, txPin, rxPin} or {found: false}
       → Fail-fast: Exit immediately if no connection found

    2. testGpioConnection()      - Verify electrical connection
       → Returns: {connected: true/false, rxWhenTxLow, rxWhenTxHigh}
       → Fail-fast: Exit if connection test fails

    3. runSingleTest({driver, laneSizes, pattern?, iterations?}) - Run one validation test
       → Args: Single test configuration with driver (required), laneSizes (required), pattern (optional), iterations (optional)
       → Returns: {success, passed, totalTests, passedTests, duration_ms, driver, laneCount, laneSizes, pattern}
       → Python orchestrates test matrix by calling runSingleTest multiple times
       → Replaces legacy runTest() batch operation (one-test-per-RPC architecture)

Legacy Text Patterns:
    - Text output is for human diagnostics ONLY
    - Machine coordination uses ONLY JSON-RPC commands/responses
    - No grep/regex parsing for control flow decisions

Usage:
    🎯 AI agents should use 'bash validate' wrapper (see CLAUDE.md)

    # Driver selection (mandatory)
    uv run ci/validate.py --parlio              # Test only PARLIO driver
    uv run ci/validate.py --rmt --spi           # Test RMT and SPI drivers
    uv run ci/validate.py --all                 # Test all drivers

    # Options
    uv run ci/validate.py --parlio --skip-lint  # Skip linting
    uv run ci/validate.py --rmt --timeout 120   # Custom timeout
    uv run ci/validate.py --help                # Show help

Architecture:
    This script orchestrates the same 4-phase workflow as debug_attached.py:
    Phase 0: Package Installation (GLOBAL LOCK via daemon)
    Phase 1: Linting (C++ linting - catches ISR errors)
    Phase 2: Compile (NO LOCK - parallelizable)
    Phase 3: Upload
    Phase 4: Monitor (with validation-specific patterns and JSON-RPC)
"""

import argparse
import json
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, cast

from colorama import Fore, Style, init

# Import phase functions from debug_attached
from ci.debug_attached import (
    run_compile,
    run_cpp_lint,
    run_monitor,
    run_upload,
)
from ci.rpc_client import RpcClient, RpcTimeoutError
from ci.util.global_interrupt_handler import (
    handle_keyboard_interrupt_properly,
    install_signal_handler,
    is_interrupted,
)
from ci.util.json_rpc_handler import parse_json_rpc_commands
from ci.util.port_utils import (
    auto_detect_upload_port,
    detect_attached_chip,
    kill_port_users,
)


# Try to import fbuild ledger for cached chip detection
try:
    from fbuild.ledger import (  # pyright: ignore[reportMissingImports]
        detect_and_cache as fbuild_detect_and_cache,
    )

    FBUILD_LEDGER_AVAILABLE = True
except ImportError:
    FBUILD_LEDGER_AVAILABLE = False
    fbuild_detect_and_cache = None  # type: ignore[assignment,misc]
from ci.util.sketch_resolver import parse_timeout


# Initialize colorama
init(autoreset=True)


# ============================================================
# Validation-Specific Configuration
# ============================================================

# No legacy expect patterns - JSON-RPC test_complete is the authoritative
# completion signal. Text pattern matching is obsolete now that we have
# full JSON-RPC orchestration.
DEFAULT_EXPECT_PATTERNS: list[str] = []

# Stop patterns - early exit when test suite completes
# Firmware emits either:
#   TEST_COMPLETED_EXIT_OK (all tests passed)
#   TEST_COMPLETED_EXIT_ERROR (some tests failed)
# After detection, validate script queries getTestSummary RPC for final status
STOP_PATTERN = "TEST_COMPLETED_EXIT_(OK|ERROR)"

# Default fail-on pattern
DEFAULT_FAIL_ON_PATTERN = "ERROR"

# Exit-on-error patterns for immediate device failure detection
EXIT_ON_ERROR_PATTERNS = [
    "ClearCommError",  # Device stuck (ISR hogging CPU, crashed, etc.)
    "register dump",  # ESP32 panic/crash with register state dump
]

# Input-on-trigger configuration
# Wait for VALIDATION_READY pattern before proceeding
# NOTE: Tests are triggered via runSingleTest RPC commands (in json_rpc_commands)
INPUT_ON_TRIGGER = None  # No legacy START command needed

# GPIO pin definitions (must match Validation.ino)
# Note: ESP32-C6 uses RX=0, ESP32 uses RX=2 (different defaults per platform)
PIN_TX = 1  # TX pin used by FastLED drivers
PIN_RX = 0  # RX pin used by RMT receiver (ESP32-C6 default; ESP32 uses 2)


# ============================================================
# GPIO Connectivity Pre-Test
# ============================================================


def run_gpio_pretest(
    port: str,
    tx_pin: int = PIN_TX,
    rx_pin: int = PIN_RX,
    timeout: float = 15.0,
    use_pyserial: bool = False,
) -> bool:
    """Test GPIO connectivity between TX and RX pins before running validation.

    This pre-test uses a simple pullup/drive-low pattern to verify that
    the TX and RX pins are physically connected via a jumper wire.

    Args:
        port: Serial port path
        tx_pin: TX pin number (default: PIN_TX)
        rx_pin: RX pin number (default: PIN_RX)
        timeout: Timeout in seconds for response
        use_pyserial: If True, use pyserial directly instead of fbuild (for --no-fbuild)

    Returns:
        True if pins are connected, False otherwise
    """
    print()
    print("=" * 60)
    print("GPIO CONNECTIVITY PRE-TEST")
    print("=" * 60)
    print(f"Testing connection: TX (GPIO {tx_pin}) → RX (GPIO {rx_pin})")
    print()

    try:
        print("  Waiting for device to boot...")
        with RpcClient(port, timeout=timeout, use_pyserial=use_pyserial) as client:
            print()
            print("  Sending GPIO test command...")

            response = client.send_and_match(
                "testGpioConnection",
                args=[tx_pin, rx_pin],
                match_key="connected",
                retries=3,
            )

            if response.get("connected", False):
                print()
                print(f"{Fore.GREEN}✅ GPIO PRE-TEST PASSED{Style.RESET_ALL}")
                print(f"   TX (GPIO {tx_pin}) and RX (GPIO {rx_pin}) are connected")
                print()
                return True
            else:
                print()
                print(f"{Fore.RED}❌ GPIO PRE-TEST FAILED{Style.RESET_ALL}")
                print()
                print(
                    f"   {Fore.RED}Error: {response.get('message', 'Unknown error')}{Style.RESET_ALL}"
                )
                print()
                print("   The TX and RX pins are NOT electrically connected.")
                print()
                print(f"   {Fore.YELLOW}ACTION REQUIRED:{Style.RESET_ALL}")
                print(
                    f"   Connect a jumper wire between GPIO {tx_pin} (TX) and GPIO {rx_pin} (RX)"
                )
                print()
                print("   Debug info:")
                print(f"     RX when TX=LOW:  {response.get('rxWhenTxLow', '?')}")
                print(f"     RX when TX=HIGH: {response.get('rxWhenTxHigh', '?')}")
                print()
                return False

    except RpcTimeoutError:
        print()
        print(f"{Fore.RED}❌ GPIO PRE-TEST TIMEOUT{Style.RESET_ALL}")
        print(f"   No response from device within {timeout} seconds")
        print()
        print("   Possible causes:")
        print("   1. Firmware is outdated (recompile with latest code)")
        print("   2. Device is stuck in setup (check serial output above)")
        print("   3. Serial communication issue")
        print()
        return False
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except (RuntimeError, OSError) as e:
        print()
        print(f"{Fore.RED}❌ GPIO PRE-TEST ERROR{Style.RESET_ALL}")
        print(f"   Serial connection error: {e}")
        print()
        return False
    except Exception as e:
        print()
        print(f"{Fore.RED}❌ GPIO PRE-TEST ERROR{Style.RESET_ALL}")
        print(f"   Unexpected error: {e}")
        print()
        return False


# ============================================================
# Pin Discovery (Auto-Detect Connected Pins)
# ============================================================


def run_pin_discovery(
    port: str,
    start_pin: int = 0,
    end_pin: int = 21,
    timeout: float = 15.0,
    use_pyserial: bool = False,
) -> tuple[bool, int | None, int | None]:
    """Auto-discover connected pin pairs by probing adjacent GPIO pins.

    This function calls the findConnectedPins RPC to search for a jumper wire
    connection between adjacent pin pairs.

    Args:
        port: Serial port path
        start_pin: Start of pin range to search (default: 0)
        end_pin: End of pin range to search (default: 21)
        timeout: Timeout in seconds for response
        use_pyserial: If True, use pyserial directly instead of fbuild (for --no-fbuild)

    Returns:
        Tuple of (success, tx_pin, rx_pin) where:
        - success: True if pins were found and auto-applied
        - tx_pin: Discovered TX pin number (or None if not found)
        - rx_pin: Discovered RX pin number (or None if not found)
    """
    print()
    print("=" * 60)
    print("PIN DISCOVERY (Auto-Detect Connected Pins)")
    print("=" * 60)
    print(f"Searching for jumper wire connection in GPIO range {start_pin}-{end_pin}")
    print()

    try:
        print("  Waiting for device to boot...")
        with RpcClient(port, timeout=timeout, use_pyserial=use_pyserial) as client:
            print()
            print("  Probing adjacent pin pairs for jumper wire connection...")

            response = client.send_and_match(
                "findConnectedPins",
                args=[{"startPin": start_pin, "endPin": end_pin, "autoApply": True}],
                match_key="found",
                retries=3,
            )

            if response.get("found", False):
                tx_pin = response.get("txPin")
                rx_pin = response.get("rxPin")
                auto_applied = response.get("autoApplied", False)

                print()
                print(f"{Fore.GREEN}✅ PIN DISCOVERY SUCCESSFUL{Style.RESET_ALL}")
                print(
                    f"   Found connected pins: TX (GPIO {tx_pin}) → RX (GPIO {rx_pin})"
                )
                if auto_applied:
                    print(
                        f"   {Fore.CYAN}Pins auto-applied to firmware{Style.RESET_ALL}"
                    )
                print()
                return (True, tx_pin, rx_pin)
            else:
                print()
                print(
                    f"{Fore.YELLOW}⚠️  PIN DISCOVERY: No connection found{Style.RESET_ALL}"
                )
                print()
                print(
                    f"   {response.get('message', 'No connected pin pairs detected')}"
                )
                print()
                print(f"   {Fore.YELLOW}Falling back to default pins{Style.RESET_ALL}")
                print()
                return (False, None, None)

    except RpcTimeoutError:
        print()
        print(f"{Fore.YELLOW}⚠️  PIN DISCOVERY TIMEOUT{Style.RESET_ALL}")
        print(f"   No response within {timeout} seconds, falling back to default pins")
        print()
        return (False, None, None)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except (RuntimeError, OSError) as e:
        print()
        print(f"{Fore.YELLOW}⚠️  PIN DISCOVERY ERROR{Style.RESET_ALL}")
        print(f"   Serial error: {e}")
        print("   Falling back to default pins")
        print()
        return (False, None, None)
    except Exception as e:
        print()
        print(f"{Fore.YELLOW}⚠️  PIN DISCOVERY ERROR{Style.RESET_ALL}")
        print(f"   Unexpected error: {e}")
        print("   Falling back to default pins")
        print()
        return (False, None, None)


# ============================================================
# Argument Parsing
# ============================================================


@dataclass
class Args:
    """Parsed command-line arguments for validation test runner."""

    # Positional argument
    environment_positional: str | None

    # Driver selection flags
    parlio: bool
    rmt: bool
    spi: bool
    uart: bool
    i2s: bool
    all: bool
    simd: bool

    # Standard options
    environment: str | None
    verbose: bool
    skip_lint: bool
    upload_port: str | None
    timeout: str
    project_dir: Path

    # Pattern overrides
    no_expect: bool
    no_fail_on: bool
    expect_keywords: list[str] | None
    fail_keywords: list[str] | None

    # Pin configuration
    tx_pin: int | None
    rx_pin: int | None
    auto_discover_pins: bool

    # Build system selection
    use_fbuild: bool
    no_fbuild: bool

    # Strip size configuration (NEW - Phase 8)
    strip_sizes: str | None

    # Lane configuration (NEW)
    lanes: str | None

    # Per-lane LED counts (NEW)
    lane_counts: str | None

    # Color pattern (NEW)
    color_pattern: str | None

    @staticmethod
    def parse_args() -> "Args":
        """Parse command-line arguments and return Args dataclass instance."""
        parser = argparse.ArgumentParser(
            description="FastLED Validation Test Runner with JSON-RPC support",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  %(prog)s --parlio                    # Auto-detect env, test only PARLIO driver
  %(prog)s esp32s3 --parlio            # Test esp32s3, PARLIO driver
  %(prog)s --rmt --spi                 # Test RMT and SPI drivers
  %(prog)s --all                       # Test all drivers
  %(prog)s --parlio --skip-lint        # Skip linting for faster iteration
  %(prog)s --rmt --timeout 120         # Custom timeout (default: 60s)
  %(prog)s --i2s --lanes 2 --strip-sizes 100,300  # Test I2S with 2 lanes, strips of 100 and 300 LEDs
  %(prog)s --parlio --lanes 1-4        # Test PARLIO with 1-4 lanes
  %(prog)s --rmt --strip-sizes small   # Test RMT with 'small' preset (100/500 LEDs)
  %(prog)s --i2s --lane-counts 100,200,300  # Test I2S with 3 lanes (100, 200, 300 LEDs per lane)
  %(prog)s --parlio --color-pattern 0xff00aa  # Test PARLIO with custom color pattern (RGB hex)
  %(prog)s --help                      # Show this help message

Driver Selection (JSON-RPC):
  Driver selection happens at runtime via JSON-RPC (no recompilation needed).
  You can instantly switch between drivers without rebuilding firmware.

  MANDATORY: You MUST specify at least one driver flag:
    --parlio    Test only PARLIO driver
    --rmt       Test only RMT driver
    --spi       Test only SPI driver
    --uart      Test only UART driver
    --all       Test all drivers (equivalent to --parlio --rmt --spi --uart)

Strip Size Configuration:
  Configure LED strip sizes for validation testing via JSON-RPC:
    --strip-sizes <preset or custom>   Set strip sizes (preset or comma-separated counts)

  Presets (shortcuts for common configurations):
    tiny    - 10, 100 LEDs
    small   - 100, 500 LEDs (default)
    medium  - 300, 1000 LEDs
    large   - 500, 3000 LEDs
    xlarge  - 1000, 5000 LEDs (high-memory devices only)

  Custom arrays (comma-separated LED counts):
    --strip-sizes 100,300         Test with 100 and 300 LED strips
    --strip-sizes 100,300,1000    Test with 100, 300, and 1000 LED strips
    --strip-sizes 500             Test with single 500 LED strip

Lane Configuration:
  Configure number of lanes for validation testing via JSON-RPC:
    --lanes <N or MIN-MAX>   Set lane count or range
    --lane-counts <LED1,LED2,...>  Set per-lane LED counts (comma-separated)

  Examples:
    --lanes 2          Test with exactly 2 lanes
    --lanes 1-4        Test with 1 to 4 lanes (tests all combinations)
    --lane-counts 100,200,300  3 lanes with 100, 200, 300 LEDs per lane
    Default: 1-8 lanes (firmware default)

Color Pattern Configuration:
  Configure custom color pattern for validation testing:
    --color-pattern <HEX>  Set RGB color pattern (hex format: RRGGBB or 0xRRGGBB)

  Examples:
    --color-pattern ff00aa      Custom color (pink)
    --color-pattern 0x00ff00    Custom color (green)

Exit Codes:
  0   Success (all patterns found, no failures)
  1   Failure (missing patterns, ERROR detected, or compilation/upload failed)
  130 User interrupt (Ctrl+C)

See Also:
  - examples/Validation/Validation.ino - Validation sketch documentation
  - CLAUDE.md Section "Live Device Testing (AI Agents)" - Usage guidelines
        """,
        )

        # Positional argument for environment
        parser.add_argument(
            "environment_positional",
            nargs="?",
            help="PlatformIO environment to build (e.g., esp32s3, esp32dev). If omitted, auto-detects from attached device.",
        )

        # Driver selection flags
        driver_group = parser.add_argument_group("Driver Selection (mandatory)")
        driver_group.add_argument(
            "--parlio",
            action="store_true",
            help="Test only PARLIO driver",
        )
        driver_group.add_argument(
            "--rmt",
            action="store_true",
            help="Test only RMT driver",
        )
        driver_group.add_argument(
            "--spi",
            action="store_true",
            help="Test only SPI driver",
        )
        driver_group.add_argument(
            "--uart",
            action="store_true",
            help="Test only UART driver",
        )
        driver_group.add_argument(
            "--i2s",
            action="store_true",
            help="Test only I2S LCD_CAM driver (ESP32-S3 only)",
        )
        driver_group.add_argument(
            "--all",
            action="store_true",
            help="Test all drivers (equivalent to --parlio --rmt --spi --uart --i2s)",
        )
        driver_group.add_argument(
            "--simd",
            action="store_true",
            help="Test SIMD operations only (no LED drivers)",
        )

        # Standard options
        parser.add_argument(
            "--env",
            "-e",
            dest="environment",
            help="PlatformIO environment to build (optional, auto-detect if not provided)",
        )
        parser.add_argument(
            "--verbose",
            "-v",
            action="store_true",
            help="Enable verbose output",
        )
        parser.add_argument(
            "--skip-lint",
            action="store_true",
            help="Skip C++ linting phase (faster iteration, but may miss ISR errors)",
        )
        parser.add_argument(
            "--upload-port",
            "-p",
            help="Serial port to use for upload and monitoring (e.g., /dev/ttyUSB0, COM3)",
        )
        parser.add_argument(
            "--timeout",
            "-t",
            type=str,
            default="60",
            help="Timeout for monitor phase. Supports: plain number (seconds), '120s', '2m', '5000ms' (default: 60)",
        )
        parser.add_argument(
            "--project-dir",
            "-d",
            type=Path,
            default=Path.cwd(),
            help="PlatformIO project directory (default: current directory)",
        )

        # Pattern overrides (for advanced usage)
        pattern_group = parser.add_argument_group(
            "Pattern Overrides (advanced)",
            "Override default validation patterns. Use with caution.",
        )
        pattern_group.add_argument(
            "--no-expect",
            action="store_true",
            help="Clear default --expect patterns (use custom only)",
        )
        pattern_group.add_argument(
            "--no-fail-on",
            action="store_true",
            help="Clear default --fail-on pattern (use custom only)",
        )
        pattern_group.add_argument(
            "--expect",
            "-x",
            action="append",
            dest="expect_keywords",
            help="Add custom regex pattern that must be matched by timeout for exit 0 (can be specified multiple times)",
        )
        pattern_group.add_argument(
            "--fail-on",
            "-f",
            action="append",
            dest="fail_keywords",
            help="Add custom regex pattern that triggers immediate termination + exit 1 (can be specified multiple times)",
        )

        # Pin configuration
        pin_group = parser.add_argument_group(
            "Pin Configuration",
            "Override default TX/RX pins (platform-specific defaults otherwise).",
        )
        pin_group.add_argument(
            "--tx-pin",
            type=int,
            help="Override TX pin number (FastLED driver output)",
        )
        pin_group.add_argument(
            "--rx-pin",
            type=int,
            help="Override RX pin number (RMT receiver input)",
        )
        pin_group.add_argument(
            "--auto-discover-pins",
            action="store_true",
            default=True,
            help="Auto-discover connected pin pairs (default: True)",
        )
        pin_group.add_argument(
            "--no-auto-discover-pins",
            action="store_true",
            help="Disable auto-discovery of connected pins",
        )

        # Build system selection
        parser.add_argument(
            "--use-fbuild",
            action="store_true",
            help="Use fbuild for compile and upload instead of PlatformIO (default for esp32s3/esp32c6)",
        )
        parser.add_argument(
            "--no-fbuild",
            action="store_true",
            help="Force PlatformIO even for esp32s3/esp32c6 (disables fbuild default)",
        )

        # Strip size configuration (NEW - Phase 8)
        strip_size_group = parser.add_argument_group(
            "Strip Size Configuration",
            "Configure LED strip sizes for validation testing.",
        )
        strip_size_group.add_argument(
            "--strip-sizes",
            type=str,
            metavar="PRESET or SIZE1,SIZE2,...",
            help="Strip sizes: preset name (tiny/small/medium/large/xlarge) OR comma-separated LED counts (e.g., '100,300,1000')",
        )

        # Lane configuration (NEW)
        lane_group = parser.add_argument_group(
            "Lane Configuration",
            "Configure number of lanes for validation testing.",
        )
        lane_group.add_argument(
            "--lanes",
            type=str,
            metavar="N or MIN-MAX",
            help="Lane count: single number (e.g., '2') or range (e.g., '1-4'). Default: 1-8",
        )
        lane_group.add_argument(
            "--lane-counts",
            type=str,
            metavar="LED1,LED2,...",
            help="Per-lane LED counts (comma-separated, e.g., '100,200,300' for 3 lanes with different counts)",
        )

        # Color pattern configuration (NEW)
        color_group = parser.add_argument_group(
            "Color Pattern Configuration",
            "Configure custom color pattern for validation testing.",
        )
        color_group.add_argument(
            "--color-pattern",
            type=str,
            metavar="HEX",
            help="RGB color pattern in hex format (e.g., 'ff00aa' or '0x00ff00')",
        )

        parsed = parser.parse_args()

        # Convert argparse.Namespace to Args dataclass
        return Args(
            environment_positional=parsed.environment_positional,
            parlio=parsed.parlio,
            rmt=parsed.rmt,
            spi=parsed.spi,
            uart=parsed.uart,
            i2s=parsed.i2s,
            all=parsed.all,
            simd=parsed.simd,
            environment=parsed.environment,
            verbose=parsed.verbose,
            skip_lint=parsed.skip_lint,
            upload_port=parsed.upload_port,
            timeout=parsed.timeout,
            project_dir=parsed.project_dir,
            no_expect=parsed.no_expect,
            no_fail_on=parsed.no_fail_on,
            expect_keywords=parsed.expect_keywords,
            fail_keywords=parsed.fail_keywords,
            tx_pin=parsed.tx_pin,
            rx_pin=parsed.rx_pin,
            auto_discover_pins=parsed.auto_discover_pins
            and not parsed.no_auto_discover_pins,
            use_fbuild=parsed.use_fbuild,
            no_fbuild=parsed.no_fbuild,
            strip_sizes=parsed.strip_sizes,  # NEW - Phase 8
            lanes=parsed.lanes,  # NEW
            lane_counts=parsed.lane_counts,  # NEW
            color_pattern=parsed.color_pattern,  # NEW
        )


# ============================================================
# Build System Selection
# ============================================================


def _should_use_fbuild(
    environment: str | None, use_fbuild_flag: bool, no_fbuild_flag: bool
) -> bool:
    """Determine if fbuild should be used for compilation and upload.

    fbuild is the default for esp32s3 and esp32c6 (RISC-V) environments.

    Args:
        environment: PlatformIO environment name (e.g., "esp32s3", "esp32c6")
        use_fbuild_flag: True if --use-fbuild was explicitly specified
        no_fbuild_flag: True if --no-fbuild was explicitly specified

    Returns:
        True if fbuild should be used, False otherwise
    """
    # Explicit --no-fbuild takes priority
    if no_fbuild_flag:
        return False

    # Explicit --use-fbuild
    if use_fbuild_flag:
        return True

    # Default: use fbuild for esp32s3 and esp32c6
    if environment:
        env_lower = environment.lower()
        if "esp32s3" in env_lower or "esp32c6" in env_lower:
            return True

    return False


# ============================================================
# Main Entry Point
# ============================================================


def run(args: Args | None = None) -> int:
    """Main entry point."""
    # Install signal handler for proper Ctrl+C handling on Windows
    install_signal_handler()

    args = args or Args.parse_args()
    assert args is not None

    # ============================================================
    # Environment Resolution
    # ============================================================
    # Resolve environment: positional argument takes precedence over --env flag
    final_environment = args.environment_positional or args.environment

    # If --i2s is specified but no environment, force esp32s3
    # I2S LCD_CAM driver is only available on ESP32-S3
    if args.i2s and not final_environment:
        final_environment = "esp32s3"
        print("ℹ️  --i2s flag requires ESP32-S3, auto-selecting 'esp32s3' environment")

    # ============================================================
    # Driver Selection Validation
    # ============================================================
    drivers: list[str] = []

    # SIMD test mode - special case, no drivers needed
    simd_test_mode = args.simd

    # Check if any driver flags were specified
    if args.all:
        drivers = ["PARLIO", "RMT", "SPI", "UART", "I2S"]
    else:
        if args.parlio:
            drivers.append("PARLIO")
        if args.rmt:
            drivers.append("RMT")
        if args.spi:
            drivers.append("SPI")
        if args.uart:
            drivers.append("UART")
        if args.i2s:
            drivers.append("I2S")

    # MANDATORY: At least one driver OR --simd must be specified
    if not drivers and not simd_test_mode:
        print(f"{Fore.RED}{'=' * 60}")
        print(f"{Fore.RED}ERROR: No LED driver specified.")
        print(f"{Fore.RED}{'=' * 60}")
        print(
            f"\n{Fore.RED}You must specify at least one driver to test.{Style.RESET_ALL}\n"
        )
        print("Available driver options:")
        print("  --parlio    Test parallel I/O driver")
        print("  --rmt       Test RMT (Remote Control) driver")
        print("  --spi       Test SPI driver")
        print("  --uart      Test UART driver")
        print("  --i2s       Test I2S LCD_CAM driver (ESP32-S3 only)")
        print("  --all       Test all drivers")
        print("\nExample commands:")
        print("  uv run ci/validate.py --parlio")
        print("  uv run ci/validate.py esp32s3 --parlio")
        print("  uv run ci/validate.py --rmt --spi")
        print("  uv run ci/validate.py --all")
        print()
        return 1

    # Parse --lanes argument
    min_lanes: int | None = None
    max_lanes: int | None = None

    if args.lanes:
        if "-" in args.lanes:
            # Range format: "1-4"
            parts = args.lanes.split("-", 1)
            try:
                min_lanes = int(parts[0])
                max_lanes = int(parts[1])
            except ValueError:
                print(
                    f"❌ Error: Invalid lane range '{args.lanes}' (expected format: '1-4')"
                )
                return 1
        else:
            # Single number: "2"
            try:
                lane_count = int(args.lanes)
                min_lanes = lane_count
                max_lanes = lane_count
            except ValueError:
                print(f"❌ Error: Invalid lane count '{args.lanes}' (expected integer)")
                return 1

    # Parse --lane-counts argument (NEW)
    per_lane_counts: list[int] | None = None

    if args.lane_counts:
        try:
            per_lane_counts = [int(c.strip()) for c in args.lane_counts.split(",")]
            if not per_lane_counts:
                print(f"❌ Error: No lane counts provided in '{args.lane_counts}'")
                return 1
            if any(c <= 0 for c in per_lane_counts):
                print("❌ Error: All lane counts must be positive integers")
                return 1
            # Validate lane count (1-8)
            if len(per_lane_counts) < 1 or len(per_lane_counts) > 8:
                print(
                    f"❌ Error: Lane count must be 1-8, got {len(per_lane_counts)} lanes"
                )
                return 1
        except ValueError:
            print(
                f"❌ Error: Invalid lane counts '{args.lane_counts}' (expected comma-separated integers like '100,200,300')"
            )
            return 1

    # Parse --color-pattern argument (NEW)
    custom_color: tuple[int, int, int] | None = None

    if args.color_pattern:
        # Remove optional 0x prefix
        hex_str = args.color_pattern.strip()
        if hex_str.startswith("0x") or hex_str.startswith("0X"):
            hex_str = hex_str[2:]

        # Validate hex format (should be 6 characters: RRGGBB)
        if len(hex_str) != 6:
            print(
                f"❌ Error: Color pattern must be 6 hex digits (RRGGBB), got '{args.color_pattern}'"
            )
            return 1

        try:
            # Parse RGB components
            r = int(hex_str[0:2], 16)
            g = int(hex_str[2:4], 16)
            b = int(hex_str[4:6], 16)
            custom_color = (r, g, b)
            print(
                f"ℹ️  Using custom color pattern: RGB({r}, {g}, {b}) = 0x{hex_str.upper()}"
            )
        except ValueError:
            print(
                f"❌ Error: Invalid hex color '{args.color_pattern}' (expected format: 'RRGGBB' or '0xRRGGBB')"
            )
            return 1

    # Build JSON-RPC command with named arguments (NEW consolidated format)
    # Generate multiple runSingleTest calls (one per test configuration)
    config: dict[str, Any] = {}

    # Drivers (required)
    config["drivers"] = drivers

    # Lane range (optional)
    if min_lanes is not None and max_lanes is not None:
        config["laneRange"] = {"min": min_lanes, "max": max_lanes}

    # Strip sizes (optional)
    if args.strip_sizes:
        # Check if it's a preset name
        preset_map = {
            "tiny": (10, 100),
            "small": (100, 500),
            "medium": (300, 1000),
            "large": (500, 3000),
            "xlarge": (1000, 5000),
        }

        if args.strip_sizes in preset_map:
            # Use preset (convert to array)
            short, long = preset_map[args.strip_sizes]
            config["stripSizes"] = [short, long]
        elif "," in args.strip_sizes:
            # Parse comma-separated array
            try:
                sizes = [int(s.strip()) for s in args.strip_sizes.split(",")]
                if not sizes:
                    print(f"❌ Error: No strip sizes provided in '{args.strip_sizes}'")
                    return 1
                if any(s <= 0 for s in sizes):
                    print("❌ Error: Strip sizes must be positive integers")
                    return 1
                config["stripSizes"] = sizes
            except ValueError:
                print(
                    f"❌ Error: Invalid strip sizes '{args.strip_sizes}' (expected comma-separated integers or preset name)"
                )
                return 1
        else:
            # Single integer
            try:
                size = int(args.strip_sizes)
                if size <= 0:
                    print("❌ Error: Strip size must be positive")
                    return 1
                config["stripSizes"] = [size]
            except ValueError:
                print(
                    f"❌ Error: Invalid strip size '{args.strip_sizes}' (expected integer, comma-separated integers, or preset name)"
                )
                return 1

    # Build list of RPC commands
    rpc_commands_list: list[dict[str, Any]] = []

    # Add setLaneSizes command if per-lane counts are specified (NEW)
    if per_lane_counts is not None:
        # setLaneSizes command takes [[size1, size2, ...]] (array wrapped in array)
        set_lane_sizes_cmd = {"function": "setLaneSizes", "args": [per_lane_counts]}
        rpc_commands_list.append(set_lane_sizes_cmd)
        print(
            f"ℹ️  Setting per-lane LED counts: {', '.join(str(c) for c in per_lane_counts)} ({len(per_lane_counts)} lanes)"
        )

    # Add custom color command if specified (NEW)
    # NOTE: This requires firmware support for a setSolidColor or setCustomPattern RPC command
    # For now, we'll add a placeholder that the firmware can implement
    if custom_color is not None:
        r, g, b = custom_color
        # Proposed RPC format: {"function": "setSolidColor", "args": [{"r": R, "g": G, "b": B}]}
        set_color_cmd = {
            "function": "setSolidColor",
            "args": [{"r": r, "g": g, "b": b}],
        }
        rpc_commands_list.append(set_color_cmd)
        print(
            "⚠️  Note: setSolidColor RPC command requires firmware support (may need implementation)"
        )

    # Generate runSingleTest commands for each test configuration
    # New API: one test per RPC call (no matrix, no batch operations)
    drivers_list = config["drivers"]
    lane_range = config.get("laneRange", {"min": 1, "max": 1})
    strip_sizes = config.get("stripSizes", [100])

    # Generate test configurations: drivers × lane counts × strip sizes
    for driver in drivers_list:
        for lane_count in range(lane_range["min"], lane_range["max"] + 1):
            for strip_size in strip_sizes:
                # Create lane sizes array (all lanes have same LED count)
                lane_sizes = [strip_size] * lane_count

                # Build runSingleTest command
                # Format: {"function":"runSingleTest","args":[{"driver":"PARLIO","laneSizes":[100,100],"pattern":"MSB_LSB_A","iterations":1}]}
                test_config = {
                    "driver": driver,
                    "laneSizes": lane_sizes,
                    "pattern": "MSB_LSB_A",  # Default pattern
                    "iterations": 1,  # Default iterations
                }
                rpc_command = {"function": "runSingleTest", "args": [test_config]}
                rpc_commands_list.append(rpc_command)

    # Convert to JSON string
    json_rpc_cmd_str = json.dumps(rpc_commands_list)

    # Parse JSON-RPC commands
    try:
        json_rpc_commands = parse_json_rpc_commands(json_rpc_cmd_str)
    except ValueError as e:
        print(f"❌ Error parsing JSON-RPC commands: {e}")
        return 1

    # ============================================================
    # Build Pattern Lists
    # ============================================================

    # Expect patterns
    expect_keywords: list[str]
    if args.no_expect:
        expect_keywords = args.expect_keywords or []
    else:
        expect_keywords = DEFAULT_EXPECT_PATTERNS.copy()
        if args.expect_keywords:
            expect_keywords.extend(args.expect_keywords)

    # Fail patterns
    fail_keywords: list[str]
    if args.no_fail_on:
        fail_keywords = args.fail_keywords or []
    else:
        # Start with default fail pattern
        fail_keywords = [DEFAULT_FAIL_ON_PATTERN]
        # Add exit-on-error patterns
        fail_keywords.extend(EXIT_ON_ERROR_PATTERNS)
        # Add custom patterns
        if args.fail_keywords:
            fail_keywords.extend(args.fail_keywords)

    # ============================================================
    # Parse Timeout
    # ============================================================
    try:
        timeout_seconds = parse_timeout(args.timeout)
    except ValueError as e:
        print(f"❌ Error: {e}")
        return 1

    # ============================================================
    # Project Directory Validation
    # ============================================================
    build_dir = args.project_dir.resolve()

    # Verify platformio.ini exists
    if not (build_dir / "platformio.ini").exists():
        print(f"❌ Error: platformio.ini not found in {build_dir}")
        print("   Make sure you're running this from a PlatformIO project directory")
        return 1

    # Set sketch to Validation
    import os

    sketch_path = build_dir / "examples" / "Validation"
    if not sketch_path.exists():
        print(f"❌ Error: Validation sketch not found at {sketch_path}")
        return 1

    os.environ["PLATFORMIO_SRC_DIR"] = str(sketch_path)

    # ============================================================
    # Port Detection
    # ============================================================
    upload_port = args.upload_port
    if not upload_port:
        result = auto_detect_upload_port()
        if not result.ok:
            # Port detection failed - display detailed error and exit
            print(f"{Fore.RED}{'=' * 60}")
            print(f"{Fore.RED}⚠️  FATAL ERROR: PORT DETECTION FAILED")
            print(f"{Fore.RED}{'=' * 60}")
            print(f"\n{Fore.RED}{result.error_message}{Style.RESET_ALL}\n")

            # Display all scanned ports for diagnostics
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
            print(
                f"{Fore.RED}Note: Bluetooth serial ports (BTHENUM) are not supported.{Style.RESET_ALL}\n"
            )
            return 1

        # Port detection succeeded
        upload_port = result.selected_port

    # Ensure upload_port is set at this point (for type checker)
    assert upload_port is not None, "upload_port should be set by auto-detection"

    # ============================================================
    # Auto-Detect Environment from Attached Chip (if not specified)
    # ============================================================
    detected_chip_type: str | None = None
    detected_environment: str | None = None
    was_cached = False

    if not final_environment:
        print("🔍 Detecting attached chip type...")

        # Try fbuild ledger first (faster when cached), but only if fbuild is not disabled
        skip_fbuild_detection = args.no_fbuild
        if skip_fbuild_detection:
            print("   (skipping fbuild ledger due to --no-fbuild)")
        if (
            not skip_fbuild_detection
            and FBUILD_LEDGER_AVAILABLE
            and fbuild_detect_and_cache is not None
        ):
            try:
                # Use cast(Any, ...) to silence pyright errors from unresolved fbuild module
                ledger_result = cast(Any, fbuild_detect_and_cache)(upload_port)
                detected_chip_type: str | None = (
                    str(ledger_result.chip_type) if ledger_result.chip_type else None
                )
                detected_environment: str | None = (
                    str(ledger_result.environment)
                    if ledger_result.environment
                    else None
                )
                was_cached: bool = bool(ledger_result.was_cached)
                final_environment: str | None = detected_environment
                cache_indicator = " (cached)" if was_cached else ""
                print(
                    f"✅ Detected {detected_chip_type} → using environment '{final_environment}'{cache_indicator}"
                )
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                # fbuild ledger failed, fall back to esptool
                print(f"⚠️  fbuild ledger failed ({e}), trying esptool...")
                chip_result = detect_attached_chip(upload_port)
                if chip_result.ok and chip_result.environment:
                    detected_chip_type = chip_result.chip_type
                    detected_environment = chip_result.environment
                    final_environment = detected_environment
                    print(
                        f"✅ Detected {chip_result.chip_type} → using environment '{final_environment}'"
                    )
                else:
                    detected_chip_type = None
                    detected_environment = None
        else:
            # fbuild ledger not available, use esptool directly
            chip_result = detect_attached_chip(upload_port)
            if chip_result.ok and chip_result.environment:
                detected_chip_type = chip_result.chip_type
                detected_environment = chip_result.environment
                final_environment = detected_environment
                print(
                    f"✅ Detected {chip_result.chip_type} → using environment '{final_environment}'"
                )

        # Fall back to platformio.ini default_envs if detection failed
        if not final_environment:
            from ci.util.pio_package_daemon import get_default_environment

            default_env = get_default_environment(str(build_dir))
            if default_env:
                final_environment = default_env
                error_msg = "Chip detection failed"
                print(
                    f"⚠️  {error_msg}, "
                    f"using platformio.ini default: '{final_environment}'"
                )
            else:
                print("⚠️  Chip detection failed and no default_envs in platformio.ini")
        print()

    # ============================================================
    # Platform Mismatch Warning (detected chip vs platformio.ini default_envs)
    # ============================================================
    from ci.util.pio_package_daemon import get_default_environment

    default_env = get_default_environment(str(build_dir))
    if detected_environment and default_env and detected_environment != default_env:
        print(f"{Fore.YELLOW}{'=' * 60}")
        print(f"{Fore.YELLOW}⚠️  PLATFORM MISMATCH WARNING")
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

    # ============================================================
    # Print Compact Configuration Header
    # ============================================================
    print("─" * 60)
    print("FastLED Validation Test Runner")
    print("─" * 60)
    env_str = final_environment or "auto-detect"
    print(f"  Target      {env_str} @ {upload_port}")
    print(f"  Drivers     {', '.join(drivers)}")
    print(f"  Timeout     {timeout_seconds}s")
    print(
        f"  Patterns    {len(expect_keywords)} expect, {len(fail_keywords)} fail, stop on '{STOP_PATTERN}'"
    )
    print("─" * 60)
    print()

    try:
        # Phase 0: Package Installation
        # When --no-fbuild is specified, skip daemon and use simple PIO commands
        if args.no_fbuild:
            print("=" * 60)
            print("PHASE 0: PACKAGE INSTALLATION (--no-fbuild mode)")
            print("=" * 60)
            print("📦 Using direct PlatformIO commands (no daemon)")

            import subprocess

            from ci.compiler.build_utils import get_utf8_env

            cmd = ["pio", "pkg", "install", "--project-dir", str(build_dir)]
            if final_environment:
                cmd.extend(["--environment", final_environment])

            result = subprocess.run(cmd, env=get_utf8_env())
            if result.returncode != 0:
                print("\n❌ Package installation failed")
                return 1
            print("✅ Package installation completed\n")
        else:
            # Use daemon-based approach (default)
            from ci.util.pio_package_client import ensure_packages_installed

            if not ensure_packages_installed(
                build_dir, final_environment, timeout=1800
            ):
                print("\n❌ Package installation failed or timed out")
                return 1

        print()

        # ============================================================
        # PHASE 1: Lint C++ Code (catches ISR errors before compile)
        # ============================================================
        if not args.skip_lint:
            if not run_cpp_lint():
                print("\n❌ Linting failed. Fix issues or use --skip-lint to bypass.")
                return 1
        else:
            print("⚠️  Skipping C++ linting (--skip-lint flag set)")
            print()

        # ============================================================
        # PHASE 2: Compile (NO LOCK - parallelizable)
        # ============================================================
        # Determine if fbuild should be used (default for esp32s3/esp32c6)
        use_fbuild = _should_use_fbuild(
            final_environment, args.use_fbuild, args.no_fbuild
        )
        if use_fbuild:
            if args.use_fbuild:
                print("📦 Using fbuild (--use-fbuild specified)")
            else:
                print("📦 Using fbuild (default for esp32s3/esp32c6)")
        else:
            if args.no_fbuild:
                print("📦 Using PlatformIO (--no-fbuild specified)")
            else:
                print("📦 Using PlatformIO")

        if use_fbuild:
            from ci.util.fbuild_runner import run_fbuild_compile

            if not run_fbuild_compile(build_dir, final_environment, args.verbose):
                return 1
        else:
            if not run_compile(build_dir, final_environment, args.verbose):
                return 1

        # ============================================================
        # PHASES 3-4: Upload + Monitor
        # ============================================================
        # Note: Device locking is handled by fbuild (daemon-based)

        # Clean up orphaned processes holding the serial port
        if upload_port:
            kill_port_users(upload_port)

        # Phase 3: Upload firmware
        if use_fbuild:
            from ci.util.fbuild_runner import run_fbuild_upload

            if not run_fbuild_upload(
                build_dir, final_environment, upload_port, args.verbose
            ):
                return 1
        else:
            if not run_upload(build_dir, final_environment, upload_port, args.verbose):
                return 1

        # Wait for serial port to become available after upload
        # Device reboots after firmware upload, so we need to wait for USB re-enumeration
        if upload_port:
            print(
                "\n⏳ Waiting for device to reboot and serial port to become available..."
            )
            port_ready = False
            max_wait_time = 15.0  # seconds
            start_time = time.time()

            while time.time() - start_time < max_wait_time:
                # Check for interrupt at top of loop (Windows Ctrl+C compatibility)
                if is_interrupted():
                    raise KeyboardInterrupt()

                # Kill any orphaned processes that might be holding the port
                kill_port_users(upload_port)

                try:
                    # Try to open the serial port briefly
                    if args.no_fbuild:
                        # Use pyserial directly when --no-fbuild is specified
                        import serial

                        with serial.Serial(upload_port, 115200, timeout=0.1) as _ser:
                            port_ready = True
                            elapsed = time.time() - start_time
                            print(f"✅ Serial port available after {elapsed:.1f}s")
                            break
                    else:
                        # Use fbuild's SerialMonitor (default)
                        from fbuild.api import SerialMonitor as FbuildSerialMonitor

                        with FbuildSerialMonitor(upload_port, baud_rate=115200) as _mon:
                            port_ready = True
                            elapsed = time.time() - start_time
                            print(f"✅ Serial port available after {elapsed:.1f}s")
                            break
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception:
                    # Port not ready - use interruptible sleep for Windows Ctrl+C
                    for _ in range(5):  # 5 * 0.1s = 0.5s total
                        if is_interrupted():
                            raise KeyboardInterrupt()
                        time.sleep(0.1)

            if not port_ready:
                print(
                    f"{Fore.YELLOW}⚠️  Port not available after {max_wait_time}s, proceeding anyway...{Style.RESET_ALL}"
                )

        # Kill any processes holding the port before RPC operations
        # The port availability check above may have left daemon processes holding the port
        if upload_port:
            kill_port_users(upload_port)
            time.sleep(0.5)  # Brief delay to ensure port is fully released

        # ============================================================
        # Validate RPC Commands Against Device Schema
        # ============================================================
        # Fetch schema from device and validate commands using Pydantic
        # This catches parameter mismatches early before running tests
        # Must run AFTER firmware upload (device needs getSchema method)

        # Skip schema validation on constrained platforms (stack overflow risk)
        # ESP32-C6 (320KB RAM, ~8KB stack) crashes when serializing full RPC schema
        constrained_platforms = ["esp32c6", "esp32c2"]
        skip_schema = final_environment in constrained_platforms

        if skip_schema:
            print(
                f"\n⏭️  Skipping schema validation on {final_environment} (constrained platform)"
            )

        if not skip_schema:
            try:
                from ci.rpc_schema_validator import RpcSchemaValidator

                print("\n🔍 Validating RPC commands against device schema...")
                validator = RpcSchemaValidator(port=upload_port, timeout=10.0)

                validation_errors = []
                for i, cmd in enumerate(json_rpc_commands):
                    method = cmd.get("function") or cmd.get("method")
                    if not method:
                        validation_errors.append(
                            f"Command {i + 1}: Missing 'method' or 'function' field"
                        )
                        continue

                    args = cmd.get("args", [])

                    # Unwrap single-element arrays (RPC system does this automatically)
                    if isinstance(args, list) and len(args) == 1:
                        args = args[0]

                    try:
                        validator.validate_request(method, args)
                    except KeyboardInterrupt:
                        handle_keyboard_interrupt_properly()
                        raise
                    except Exception as e:
                        validation_errors.append(f"Command {i + 1} ({method}): {e}")

                if validation_errors:
                    print(f"\n❌ Schema validation failed:")
                    for error in validation_errors:
                        print(f"  - {error}")
                    print(f"\n💡 Fix: Update command parameters to match device schema")
                    return 1

                print(
                    f"✅ All {len(json_rpc_commands)} RPC commands validated against schema"
                )
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise

            except ImportError:
                print("⚠️  pydantic not available - skipping schema validation")
            except Exception as e:
                # Schema validation is optional - don't fail if device unavailable
                print(f"⚠️  Schema validation skipped: {e}")

        # ============================================================
        # Phase 3.5: Pin Discovery (runs FIRST if enabled)
        # ============================================================
        effective_tx_pin: int | None = None
        effective_rx_pin: int | None = None
        pins_discovered = False

        # Skip pin discovery and GPIO pre-test for SIMD mode (no hardware needed)
        if simd_test_mode:
            print("\n📌 SIMD mode: skipping pin discovery and GPIO pre-test")
        # CLI args take priority - skip discovery if user specified pins
        elif args.tx_pin is not None or args.rx_pin is not None:
            effective_tx_pin = args.tx_pin if args.tx_pin is not None else PIN_TX
            effective_rx_pin = args.rx_pin if args.rx_pin is not None else PIN_RX
            print(
                f"\n📌 Using CLI-specified pins: TX={effective_tx_pin}, RX={effective_rx_pin}"
            )
            # Add setPins RPC command
            set_pins_cmd = {
                "function": "setPins",
                "args": [{"txPin": effective_tx_pin, "rxPin": effective_rx_pin}],
            }
            json_rpc_commands.insert(0, set_pins_cmd)

        # Auto-discover pins if enabled and no CLI override
        elif args.auto_discover_pins:
            print("\n🔍 Auto-discovery enabled - searching for connected pins...")
            success, discovered_tx, discovered_rx = run_pin_discovery(
                upload_port, use_pyserial=args.no_fbuild
            )

            if success and discovered_tx is not None and discovered_rx is not None:
                effective_tx_pin = discovered_tx
                effective_rx_pin = discovered_rx
                pins_discovered = True
                print(
                    f"📌 Using discovered pins: TX={effective_tx_pin}, RX={effective_rx_pin}"
                )
                # Note: findConnectedPins with autoApply=True already applies pins in firmware
                # No need to send setPins RPC command
            else:
                # Fall back to defaults
                effective_tx_pin = PIN_TX
                effective_rx_pin = PIN_RX
                print(
                    f"📌 Using default pins: TX={effective_tx_pin}, RX={effective_rx_pin}"
                )
        else:
            # Auto-discovery disabled, use defaults
            effective_tx_pin = PIN_TX
            effective_rx_pin = PIN_RX
            print(
                f"\n📌 Using default pins: TX={effective_tx_pin}, RX={effective_rx_pin}"
            )

        # ============================================================
        # Phase 3.6: GPIO Connectivity Pre-Test
        # ============================================================
        # Skip GPIO pre-test if pins were just discovered (already verified) or SIMD mode
        if simd_test_mode:
            pass  # Already printed skip message above
        elif pins_discovered:
            print("\n✅ Skipping GPIO pre-test (pins verified during discovery)")
        elif not run_gpio_pretest(
            upload_port,
            effective_tx_pin or PIN_TX,
            effective_rx_pin or PIN_RX,
            use_pyserial=args.no_fbuild,
        ):
            print()
            print(f"{Fore.RED}=" * 60)
            print(f"{Fore.RED}VALIDATION ABORTED - GPIO PRE-TEST FAILED")
            print(f"{Fore.RED}=" * 60)
            print()
            print("The validation cannot proceed without a physical connection")
            print("between the TX and RX pins.")
            print()
            print(
                f"Please connect a jumper wire from GPIO {effective_tx_pin} to GPIO {effective_rx_pin}"
            )
            print("and run the validation again.")
            print()
            return 1

        # Phase 4: Monitor serial output with validation patterns
        # SIMD test mode - simple RPC call instead of full driver test
        if simd_test_mode:
            print()
            print("=" * 60)
            print("SIMD TEST MODE")
            print("=" * 60)
            print("Running SIMD add_sat_u8_16 test...")
            print()

            client: RpcClient | None = None
            try:
                # Use short boot_wait - device already rebooted and we waited for port
                print("   ⏳ Connecting to device...", end="", flush=True)
                client = RpcClient(
                    upload_port, timeout=10.0, use_pyserial=args.no_fbuild
                )
                client.connect(
                    boot_wait=1.0
                )  # Reduced from 3.0s since we already waited
                print(f" {Fore.GREEN}✓{Style.RESET_ALL}")

                print("   📡 Sending test command...", end="", flush=True)
                response = client.send_and_match(
                    "testSimd", match_key="passed", retries=3
                )
                print(f" {Fore.GREEN}✓{Style.RESET_ALL}")

                if response.get("passed", False):
                    print(f"{Fore.GREEN}✅ SIMD TEST PASSED{Style.RESET_ALL}")
                    print(f"   {response.get('message', '')}")
                    return 0
                else:
                    print(f"{Fore.RED}❌ SIMD TEST FAILED{Style.RESET_ALL}")
                    print(f"   {response.get('message', '')}")
                    if "actual" in response:
                        print(f"   Actual:   {response['actual']}")
                        print(f"   Expected: {response['expected']}")
                    return 1

            except RpcTimeoutError:
                print()  # Newline after partial status line
                print(f"{Fore.RED}❌ SIMD TEST TIMEOUT{Style.RESET_ALL}")
                print("   No response from device within 10 seconds")
                return 1
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print()  # Newline after partial status line
                print(f"{Fore.RED}❌ SIMD TEST ERROR: {e}{Style.RESET_ALL}")
                return 1
            finally:
                if client is not None:
                    client.close()

        # ============================================================
        # CUSTOM SERIAL MONITORING - Stay connected throughout
        # ============================================================
        # We handle serial connection ourselves to avoid reconnection/reboot
        # Flow: connect → send RPCs → monitor for stop word → parse results → close

        # Kill port users before starting
        if upload_port:
            kill_port_users(upload_port)
            time.sleep(0.5)

        print()
        print("=" * 60)
        print("MONITORING DEVICE OUTPUT")
        print("=" * 60)

        output_lines: list[str] = []
        stop_word_found: str | None = None
        test_failed = False
        start_time = time.time()

        try:
            # Open serial connection (stays open until we're done)
            print(f"📡 Connecting to {upload_port}...")
            if args.no_fbuild:
                import serial

                ser = serial.Serial(upload_port, 115200, timeout=0.1)
                monitor = ser
            else:
                from fbuild.api import SerialMonitor as FbuildSerialMonitor

                monitor = FbuildSerialMonitor(upload_port, baud_rate=115200)
                monitor.__enter__()

            # Wait for device boot
            print("⏳ Waiting for device boot...")
            time.sleep(3.0)

            # Drain boot output
            print("📥 Draining boot output...")
            boot_lines = 0
            drain_start = time.time()
            while time.time() - drain_start < 2.0:
                if args.no_fbuild:
                    if monitor.in_waiting:  # ty: ignore[possibly-missing-attribute]
                        line = (
                            monitor.readline().decode("utf-8", errors="replace").strip()  # ty: ignore[possibly-missing-attribute]
                        )
                        if line:
                            boot_lines += 1
                else:
                    try:
                        for line in monitor.read_lines(timeout=0.5):  # ty: ignore[possibly-missing-attribute]
                            boot_lines += 1
                            break
                    except KeyboardInterrupt:
                        handle_keyboard_interrupt_properly()
                    except Exception:
                        break
            print(f"   Drained {boot_lines} boot lines")

            # Send RPC commands
            print(f"🔧 Sending {len(json_rpc_commands)} JSON-RPC command(s)...")
            for i, cmd in enumerate(json_rpc_commands, 1):
                cmd_str = json.dumps(cmd, separators=(",", ":"))
                if args.no_fbuild:
                    monitor.write((cmd_str + "\n").encode())
                else:
                    monitor.write(cmd_str + "\n")
                print(
                    f"   ✓ Sent command {i}/{len(json_rpc_commands)}: {cmd['function']}()"
                )
                time.sleep(0.1)  # Brief delay between commands

            # Monitor output for stop word
            print(f"\n👀 Monitoring output (timeout: {timeout_seconds}s)...")
            print("─" * 60)

            while True:
                # Check timeout
                elapsed = time.time() - start_time
                if elapsed >= timeout_seconds:
                    print(f"\n⏱️  Timeout after {timeout_seconds}s")
                    break

                # Check interrupt
                if is_interrupted():
                    raise KeyboardInterrupt()

                # Read line
                line = None
                if args.no_fbuild:
                    if monitor.in_waiting:  # ty: ignore[possibly-missing-attribute]
                        line = (
                            monitor.readline().decode("utf-8", errors="replace").strip()  # ty: ignore[possibly-missing-attribute]
                        )
                else:
                    try:
                        for read_line in monitor.read_lines(timeout=0.1):  # ty: ignore[possibly-missing-attribute]
                            line = read_line
                            break
                    except KeyboardInterrupt:
                        handle_keyboard_interrupt_properly()
                    except Exception:
                        continue

                if not line:
                    continue

                # Store line
                output_lines.append(line)

                # Check for stop words
                if "TEST_COMPLETED_EXIT_OK" in line:
                    stop_word_found = "OK"
                    print(f"\n{Fore.GREEN}✓ TEST_COMPLETED_EXIT_OK{Style.RESET_ALL}")
                    break
                elif "TEST_COMPLETED_EXIT_ERROR" in line:
                    stop_word_found = "ERROR"
                    print(f"\n{Fore.RED}✗ TEST_COMPLETED_EXIT_ERROR{Style.RESET_ALL}")
                    break

                # Check for failure patterns
                for pattern in fail_keywords:
                    if pattern in line and "ERROR" in pattern:
                        # Only flag as error if it's a real error, not test failure message
                        if "register dump" in line or "ClearCommError" in line:
                            test_failed = True
                            print(
                                f"\n{Fore.RED}❌ Device error detected: {line[:80]}{Style.RESET_ALL}"
                            )
                            break

                # Print progress (show test completions)
                if "PASS] Test case" in line or "FAIL] Test case" in line:
                    print(f"  {line[:100]}")

        except KeyboardInterrupt:
            print("\n\n⚠️  Interrupted by user")
            handle_keyboard_interrupt_properly()
            return 130
        except Exception as e:
            print(f"\n{Fore.RED}❌ Serial error: {e}{Style.RESET_ALL}")
            return 1
        finally:
            # Close serial connection
            if args.no_fbuild:
                monitor.close()  # ty: ignore[possibly-missing-attribute]
            else:
                monitor.__exit__(None, None, None)
            print(f"\n✅ Serial connection closed")

        print("─" * 60)
        print(f"📊 Captured {len(output_lines)} output lines")

        # Check if we failed due to device error
        if test_failed:
            return 1

        # Check if no stop word found
        if not stop_word_found:
            print(
                f"\n{Fore.YELLOW}⚠️  No completion stop word found within timeout{Style.RESET_ALL}"
            )
            return 1

        # If stop word found, parse test summary from captured output
        if stop_word_found:
            print()
            print("=" * 60)
            print("FINAL TEST SUMMARY")
            print("=" * 60)

            try:
                # Parse test results from JSON-RPC responses in output
                # Look for RESULT lines with type="test_complete" for overall stats
                # and type="test_case_result" for individual case results
                # Also parse test case headers like "[PASS] Test case PARLIO (2 lanes, 100 LEDs)"
                print("  Parsing test results from captured output...")

                summary = None
                test_complete_data = None
                case_results = []
                current_driver = None
                current_lanes = None
                current_leds = None

                for line in output_lines:
                    # Parse test case headers to extract driver, lanes, LEDs
                    if "Test case" in line and ("PASS" in line or "FAIL" in line):
                        # Example: "[PASS] Test case PARLIO (2 lanes, 100 LEDs) completed successfully"
                        # Example: "[FAIL] Test case PARLIO (5 lanes, 100 LEDs) FAILED: 0/4 tests passed"
                        import re

                        match = re.search(
                            r"Test case (\w+) \((\d+) lane.*?(\d+) LEDs\)", line
                        )
                        if match:
                            current_driver = match.group(1)
                            current_lanes = int(match.group(2))
                            current_leds = int(match.group(3))

                    # Parse JSON results
                    if "RESULT:" in line and "{" in line:
                        # Extract JSON portion
                        json_start = line.index("{")
                        json_str = line[json_start:]
                        try:
                            data = json.loads(json_str)
                            if data.get("type") == "test_complete":
                                test_complete_data = data
                            elif data.get("type") == "test_case_result":
                                # Enrich with driver/lanes/LEDs from test header
                                if current_driver:
                                    data["driver"] = current_driver
                                    data["lanes"] = current_lanes
                                    data["leds"] = current_leds
                                case_results.append(data)
                        except json.JSONDecodeError:
                            continue

                # Build summary from parsed data
                if test_complete_data:
                    total_tests = test_complete_data.get("totalTests", 0)
                    passed_tests = test_complete_data.get("passedTests", 0)
                    failed_tests = total_tests - passed_tests
                    all_passed = test_complete_data.get("passed", False)
                    success_rate = (
                        100.0 * passed_tests / total_tests if total_tests > 0 else 0.0
                    )

                    summary = {
                        "success": True,
                        "allPassed": all_passed,
                        "totalTests": total_tests,
                        "passedTests": passed_tests,
                        "failedTests": failed_tests,
                        "successRate": success_rate,
                        "results": case_results,
                    }

                # Display summary if we successfully parsed the data
                if summary and summary.get("success"):
                    # Display overall statistics
                    all_passed = summary.get("allPassed", False)
                    total_tests = summary.get("totalTests", 0)
                    passed_tests = summary.get("passedTests", 0)
                    failed_tests = summary.get("failedTests", 0)
                    total_cases = summary.get("totalCases", 0)
                    passed_cases = summary.get("passedCases", 0)
                    success_rate = summary.get("successRate", 0.0)

                    print(f"  Total Test Cases: {passed_cases}/{total_cases} passed")
                    print(f"  Total Tests: {passed_tests}/{total_tests} passed")
                    print(f"  Failed Tests: {failed_tests}")
                    print(f"  Success Rate: {success_rate:.1f}%")
                    print()

                    # Display per-case results
                    results = summary.get("results", [])
                    if results and isinstance(results, list):
                        print("  Individual Results:")
                        print(
                            f"    {'Driver':<10} {'Lanes':<6} {'LEDs':<8} {'Passed/Total':<15} {'%':<8} {'Status'}"
                        )
                        print("    " + "-" * 70)

                        for result in results:
                            driver = result.get("driver", "?")
                            lanes = result.get("lanes", 0)
                            leds = result.get("leds", 0)
                            passed = result.get("passedTests", 0)
                            total = result.get("totalTests", 0)
                            ok = result.get("passed", False)
                            pct = 100.0 * passed / total if total > 0 else 0.0
                            skipped = result.get("skipped", False)

                            status_str = ""
                            if skipped:
                                status_str = f"{Fore.YELLOW}SKIPPED{Style.RESET_ALL}"
                            elif ok:
                                status_str = f"{Fore.GREEN}PASS{Style.RESET_ALL}"
                            else:
                                status_str = f"{Fore.RED}FAIL{Style.RESET_ALL}"

                            print(
                                f"    {driver:<10} {lanes:<6} {leds:<8} {passed}/{total:<13} {pct:>6.1f}% {status_str}"
                            )

                    print()
                    print("=" * 60)

                    # Return exit code based on whether all tests passed
                    if all_passed:
                        print(
                            f"{Fore.GREEN}✓ ALL TESTS PASSED{Style.RESET_ALL} - Validation successful"
                        )
                        return 0
                    else:
                        print(
                            f"{Fore.RED}✗ SOME TESTS FAILED{Style.RESET_ALL} - {failed_tests} test(s) failed"
                        )
                        return 1
                else:
                    print(
                        f"{Fore.YELLOW}⚠️  Failed to get test summary via RPC{Style.RESET_ALL}"
                    )
                    # Fall through to default behavior

            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
            except Exception as e:
                print(
                    f"{Fore.YELLOW}⚠️  Error querying test summary: {e}{Style.RESET_ALL}"
                )
                # Fall through to default behavior

        # Fallback: If no stop word found or RPC query failed
        # Use stop word type or success flag to determine exit code
        if stop_word_found == "ERROR":
            print(f"\n{Fore.RED}FAILED{Style.RESET_ALL} Some validation tests failed")
            return 1
        else:
            print(
                f"\n{Fore.GREEN}PASSED{Style.RESET_ALL} All validation tests completed"
            )
            return 0

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        handle_keyboard_interrupt_properly()
        return 130


def main() -> int:
    return run()


if __name__ == "__main__":
    sys.exit(main())
