#!/usr/bin/env python3
"""FastLED Validation Test Runner - Hardware-in-the-loop validation with JSON-RPC support.

This script runs the Validation.ino sketch with comprehensive pattern matching and
JSON-RPC driver selection. It extends debug_attached.py with validation-specific features:

- Pre-configured expect/fail patterns for hardware testing
- JSON-RPC driver selection (--parlio, --rmt, --spi, --uart)
- Smart pattern matching for test matrix validation
- Early exit on test completion (--stop pattern)

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
import sys
from dataclasses import dataclass
from pathlib import Path

from colorama import Fore, Style, init

# Import phase functions from debug_attached
from ci.debug_attached import (
    run_compile,
    run_cpp_lint,
    run_monitor,
    run_upload,
)
from ci.util.build_lock import BuildLock
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.json_rpc_handler import parse_json_rpc_commands
from ci.util.port_utils import auto_detect_upload_port, kill_port_users
from ci.util.sketch_resolver import parse_timeout


# Initialize colorama
init(autoreset=True)


# ============================================================
# Validation-Specific Configuration
# ============================================================

# Default expect patterns - simplified to essential checks only
DEFAULT_EXPECT_PATTERNS = [
    "TX Pin: 0",  # Hardware setup verification
    "RX Pin: 1",  # Hardware setup verification
    "DRIVER_ENABLED: PARLIO",  # Parlio driver availability (key driver)
    "VALIDATION_READY: true",  # Test ready indicator
]

# Stop pattern - early exit when test suite completes successfully
STOP_PATTERN = "VALIDATION_SUITE_COMPLETE"

# Default fail-on pattern
DEFAULT_FAIL_ON_PATTERN = "ERROR"

# Exit-on-error patterns for immediate device failure detection
EXIT_ON_ERROR_PATTERNS = [
    "ClearCommError",  # Device stuck (ISR hogging CPU, crashed, etc.)
    "register dump",  # ESP32 panic/crash with register state dump
]

# Input-on-trigger configuration
# Wait for VALIDATION_READY pattern, then send START command with 10s timeout
INPUT_ON_TRIGGER = "VALIDATION_READY:START:10"


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
    all: bool

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
            "--all",
            action="store_true",
            help="Test all drivers (equivalent to --parlio --rmt --spi --uart)",
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

        parsed = parser.parse_args()

        # Convert argparse.Namespace to Args dataclass
        return Args(
            environment_positional=parsed.environment_positional,
            parlio=parsed.parlio,
            rmt=parsed.rmt,
            spi=parsed.spi,
            uart=parsed.uart,
            all=parsed.all,
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
        )


# ============================================================
# Main Entry Point
# ============================================================


def run(args: Args | None = None) -> int:
    """Main entry point."""
    args = args or Args.parse_args()
    assert args is not None

    # ============================================================
    # Environment Resolution
    # ============================================================
    # Resolve environment: positional argument takes precedence over --env flag
    final_environment = args.environment_positional or args.environment

    # ============================================================
    # Driver Selection Validation
    # ============================================================
    drivers: list[str] = []

    # Check if any driver flags were specified
    if args.all:
        drivers = ["PARLIO", "RMT", "SPI", "UART"]
    else:
        if args.parlio:
            drivers.append("PARLIO")
        if args.rmt:
            drivers.append("RMT")
        if args.spi:
            drivers.append("SPI")
        if args.uart:
            drivers.append("UART")

    # MANDATORY: At least one driver must be specified
    if not drivers:
        print(Fore.RED + "=" * 60)
        print(Fore.RED + "ERROR: No LED driver specified.")
        print(Fore.RED + "=" * 60)
        print(
            f"\n{Fore.RED}You must specify at least one driver to test.{Style.RESET_ALL}\n"
        )
        print("Available driver options:")
        print("  --parlio    Test parallel I/O driver")
        print("  --rmt       Test RMT (Remote Control) driver")
        print("  --spi       Test SPI driver")
        print("  --uart      Test UART driver")
        print("  --all       Test all drivers")
        print("\nExample commands:")
        print("  uv run ci/validate.py --parlio")
        print("  uv run ci/validate.py esp32s3 --parlio")
        print("  uv run ci/validate.py --rmt --spi")
        print("  uv run ci/validate.py --all")
        print()
        return 1

    # Build JSON-RPC command for driver selection
    driver_list_str = ", ".join([f'"{d}"' for d in drivers])
    json_rpc_cmd_str = f'{{"function":"setDrivers","args":[{driver_list_str}]}}'

    # Parse JSON-RPC command
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
            print(Fore.RED + "=" * 60)
            print(Fore.RED + "⚠️  FATAL ERROR: PORT DETECTION FAILED")
            print(Fore.RED + "=" * 60)
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

    # ============================================================
    # Print Configuration Summary
    # ============================================================
    print("FastLED Validation Test Runner")
    print("=" * 60)
    print(f"Project: {build_dir}")
    print(f"Sketch: examples/Validation")
    if final_environment:
        print(f"Environment: {final_environment}")
    print(f"Upload port: {upload_port}")
    print("=" * 60)
    print()

    print("Validation Configuration:")
    print(f"  Drivers: {', '.join(drivers)} (via JSON-RPC setDrivers)")
    print(f"  Expect patterns: {len(expect_keywords)}")
    print(f"  Fail patterns: {len(fail_keywords)}")
    print(f"  Stop pattern: {STOP_PATTERN}")
    print(f"  Input trigger: {INPUT_ON_TRIGGER}")
    print(f"  Timeout: {timeout_seconds}s")
    print("=" * 60)
    print()

    try:
        # ============================================================
        # PHASE 0: Package Installation (GLOBAL LOCK via daemon)
        # ============================================================
        print("=" * 60)
        print("PHASE 0: ENSURING PACKAGES INSTALLED")
        print("=" * 60)

        from ci.util.pio_package_client import ensure_packages_installed

        if not ensure_packages_installed(build_dir, final_environment, timeout=1800):
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
        if not run_compile(build_dir, final_environment, args.verbose):
            return 1

        # ============================================================
        # PHASES 3-4: Upload + Monitor (DEVICE LOCK)
        # ============================================================
        lock = BuildLock(lock_name="device_debug", use_global=True)
        if not lock.acquire(timeout=600.0):  # 10 minute timeout
            print(
                "\n❌ Another agent is currently using the device for debug operations."
            )
            print("   Please retry later once the debug operation has completed.")
            print(f"   Lock file: {lock.lock_file}")
            return 1

        try:
            # Clean up orphaned processes holding the serial port
            if upload_port:
                kill_port_users(upload_port)

            # Phase 3: Upload firmware
            if not run_upload(build_dir, final_environment, upload_port, args.verbose):
                return 1

            # Phase 4: Monitor serial output with validation patterns
            success, output, rpc_handler = run_monitor(
                build_dir,
                final_environment,
                upload_port,
                args.verbose,
                timeout_seconds,
                fail_keywords,
                expect_keywords,
                stream=False,
                input_on_trigger=INPUT_ON_TRIGGER,
                device_error_keywords=None,  # Use defaults
                stop_keyword=STOP_PATTERN,
                json_rpc_commands=json_rpc_commands,
            )

            if not success:
                return 1

            phases_completed = "three" if args.skip_lint else "four"
            print(f"\n✅ All {phases_completed} phases completed successfully")
            print("✅ Validation test suite PASSED")
            return 0

        finally:
            # Always release the device lock
            lock.release()

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        handle_keyboard_interrupt_properly()
        return 130


def main() -> int:
    return run()


if __name__ == "__main__":
    sys.exit(main())
