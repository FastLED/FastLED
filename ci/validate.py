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

    # GPIO-only mode (no driver flag)
    uv run ci/validate.py teensy41              # GPIO toggle capture test on Teensy
    uv run ci/validate.py                       # Auto-detect device, GPIO-only

    # Driver selection (optional)
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
import asyncio
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any, cast

from colorama import Fore, Style, init

# Import phase functions from debug_attached
from ci.debug_attached import (
    run_compile,
    run_cpp_lint,
    run_monitor,
    run_upload,
)
from ci.rpc_client import RpcClient, RpcCrashError, RpcTimeoutError
from ci.util.crash_trace_decoder import CrashTraceDecoder
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
from ci.validate_ble import run_ble_validation
from ci.validate_net import run_net_loopback_validation, run_net_validation
from ci.validate_ota import run_ota_validation


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


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface


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


def display_pattern_details(result: dict[str, Any]) -> None:
    """Display per-pattern error details from enriched RPC response.

    Parses the 'patterns' array from runSingleTest response and displays
    byte-level corruption stats, LSB error analysis, and first N error examples.
    """
    patterns = result.get("patterns")
    if not patterns:
        return

    driver = result.get("driver", "?")
    lane_count = result.get("laneCount", 0)
    lane_sizes = result.get("laneSizes", [])
    total_leds = sum(lane_sizes) if lane_sizes else 0

    print()
    print("=" * 62)
    print(f"{driver} — {lane_count} lane(s), {total_leds} LEDs")
    print("=" * 62)

    agg_total_bytes = 0
    agg_mismatched_bytes = 0
    agg_lsb_only = 0

    for pat in patterns:
        name = pat.get("name", "Unknown")
        passed = pat.get("passed", False)
        status = (
            f"{Fore.GREEN}PASS{Style.RESET_ALL}"
            if passed
            else f"{Fore.RED}FAIL{Style.RESET_ALL}"
        )

        print(f"\n  {name}  {status}")

        if passed:
            num_leds = pat.get("totalLeds", 0)
            print(f"    All {num_leds} LEDs match")
            agg_total_bytes += pat.get("totalBytes", num_leds * 3)
            continue

        num_leds = pat.get("totalLeds", 0)
        mismatched_leds = pat.get("mismatchedLeds", 0)
        total_bytes = pat.get("totalBytes", num_leds * 3)
        mismatched_bytes = pat.get("mismatchedBytes", 0)
        lsb_only = pat.get("lsbOnlyErrors", 0)

        agg_total_bytes += total_bytes
        agg_mismatched_bytes += mismatched_bytes
        agg_lsb_only += lsb_only

        byte_pct = 100.0 * mismatched_bytes / total_bytes if total_bytes > 0 else 0.0
        lsb_pct = 100.0 * lsb_only / mismatched_bytes if mismatched_bytes > 0 else 0.0

        print(f"    Mismatched LEDs:  {mismatched_leds}/{num_leds}")
        print(
            f"    Byte corruption:  {mismatched_bytes}/{total_bytes} bytes ({byte_pct:.1f}%)"
        )
        print(
            f"    1-bit LSB errors: {lsb_only}/{mismatched_bytes} mismatched ({lsb_pct:.1f}%)"
        )

        errors = pat.get("errors", [])
        if errors:
            print(f"    First {len(errors)} errors:")
            for e in errors:
                led_idx = e.get("led", 0)
                exp = e.get("expected", [0, 0, 0])
                act = e.get("actual", [0, 0, 0])
                xor_vals = [exp[i] ^ act[i] for i in range(3)]
                print(
                    f"      LED[{led_idx}]  "
                    f"expected (0x{exp[0]:02X},0x{exp[1]:02X},0x{exp[2]:02X}) "
                    f"got (0x{act[0]:02X},0x{act[1]:02X},0x{act[2]:02X})  "
                    f"XOR (0x{xor_vals[0]:02X},0x{xor_vals[1]:02X},0x{xor_vals[2]:02X})"
                )

    # Aggregate summary
    print()
    print("-" * 62)
    print("  AGGREGATE")
    if agg_total_bytes > 0:
        agg_byte_pct = 100.0 * agg_mismatched_bytes / agg_total_bytes
        print(
            f"    Byte corruption: {agg_byte_pct:.1f}% ({agg_mismatched_bytes}/{agg_total_bytes} total bytes)"
        )
    if agg_mismatched_bytes > 0:
        agg_lsb_pct = 100.0 * agg_lsb_only / agg_mismatched_bytes
        print(
            f"    1-bit LSB errors: {agg_lsb_pct:.1f}% of mismatches ({agg_lsb_only}/{agg_mismatched_bytes})"
        )
        if agg_lsb_pct == 100.0:
            print(f"    Diagnosis: All corrupted bits are LSB 0->1 flips")
        elif agg_lsb_pct > 90.0:
            print(f"    Diagnosis: Predominantly LSB corruption ({agg_lsb_pct:.1f}%)")
        else:
            print(f"    Diagnosis: Mixed corruption pattern")
    elif agg_total_bytes > 0:
        print(f"    No byte-level corruption detected")
    print("-" * 62)


# ============================================================
# GPIO Connectivity Pre-Test
# ============================================================


async def run_gpio_pretest(
    port: str,
    tx_pin: int = PIN_TX,
    rx_pin: int = PIN_RX,
    timeout: float = 15.0,
    serial_interface: "SerialInterface | None" = None,
) -> bool:
    """Test GPIO connectivity between TX and RX pins before running validation (async).

    This pre-test uses a simple pullup/drive-low pattern to verify that
    the TX and RX pins are physically connected via a jumper wire.

    Args:
        port: Serial port path
        tx_pin: TX pin number (default: PIN_TX)
        rx_pin: RX pin number (default: PIN_RX)
        timeout: Timeout in seconds for response
        serial_interface: Pre-created SerialInterface (defaults to fbuild if None)

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
        # Create client and manually connect with longer boot_wait for slower platforms
        # ESP32-C6 RISC-V platform needs extra time for channel engine initialization
        print("\n" + "=" * 60)
        print("RPC CLIENT DEBUG OUTPUT")
        print("=" * 60)
        client = RpcClient(
            port, timeout=timeout, serial_interface=serial_interface, verbose=True
        )
        await client.connect(
            boot_wait=15.0, drain_boot=False
        )  # Wait 15s for initialization, don't drain to preserve RESULT JSON
        try:
            print()
            print("=" * 60)
            print("PING TEST (verify basic RPC works)")
            print("=" * 60)
            try:
                ping_response = await client.send("ping", retries=3)
                print(f"✅ Ping successful: {ping_response.data}")
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print(f"❌ Ping failed: {e}")
                print(
                    "   RPC communication is not working - device may not be responding"
                )
                return False

            print()
            print("=" * 60)
            print("GPIO CONNECTIVITY TEST")
            print("=" * 60)
            print("  Sending GPIO test command...")

            response = await client.send_and_match(
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
        finally:
            await client.close()

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


async def run_pin_discovery(
    port: str,
    start_pin: int = 0,
    end_pin: int = 8,
    timeout: float = 15.0,
    serial_interface: "SerialInterface | None" = None,
) -> tuple[bool, int | None, int | None, "RpcClient | None"]:
    """Auto-discover connected pin pairs by probing adjacent GPIO pins (async).

    This function calls the findConnectedPins RPC to search for a jumper wire
    connection between adjacent pin pairs.

    Args:
        port: Serial port path
        start_pin: Start of pin range to search (default: 0)
        end_pin: End of pin range to search (default: 21)
        timeout: Timeout in seconds for response
        serial_interface: Pre-created SerialInterface (defaults to fbuild if None)

    Returns:
        Tuple of (success, tx_pin, rx_pin, client) where:
        - success: True if pins were found and auto-applied
        - tx_pin: Discovered TX pin number (or None if not found)
        - rx_pin: Discovered RX pin number (or None if not found)
        - client: RpcClient instance (kept open for reuse, or None on error)
    """
    print()
    print("=" * 60)
    print("PIN DISCOVERY (Auto-Detect Connected Pins)")
    print("=" * 60)
    print(f"Searching for jumper wire connection in GPIO range {start_pin}-{end_pin}")
    print()

    client: RpcClient | None = None
    try:
        print("  Waiting for device to boot...")
        print("\n" + "=" * 60)
        print("RPC CLIENT DEBUG OUTPUT")
        print("=" * 60)
        client = RpcClient(
            port, timeout=timeout, serial_interface=serial_interface, verbose=True
        )
        await client.connect(boot_wait=15.0, drain_boot=True)

        print()
        print("=" * 60)
        print("PING TEST (verify basic RPC works)")
        print("=" * 60)
        try:
            ping_response = await client.send("ping", timeout=30.0, retries=3)
            print(f"✅ Ping successful: {ping_response.data}")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"❌ Ping failed: {e}")
            print("   RPC communication is not working - device may not be responding")
            if client:
                await client.close()
            return (False, None, None, None)

        print()
        print("=" * 60)
        print("PIN DISCOVERY")
        print("=" * 60)
        print("  Probing adjacent pin pairs for jumper wire connection...")

        response = await client.send_and_match(
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
            print(f"   Found connected pins: TX (GPIO {tx_pin}) → RX (GPIO {rx_pin})")
            if auto_applied:
                print(f"   {Fore.CYAN}Pins auto-applied to firmware{Style.RESET_ALL}")
            print()
            # Return client along with pins - keep connection open!
            return (True, tx_pin, rx_pin, client)
        else:
            print()
            print(
                f"{Fore.YELLOW}⚠️  PIN DISCOVERY: No connection found{Style.RESET_ALL}"
            )
            print()
            print(f"   {response.get('message', 'No connected pin pairs detected')}")
            print()
            print(f"   {Fore.YELLOW}Falling back to default pins{Style.RESET_ALL}")
            print()
            # Return client even on failure - keep connection open!
            return (False, None, None, client)

    except RpcTimeoutError:
        print()
        print(f"{Fore.YELLOW}⚠️  PIN DISCOVERY TIMEOUT{Style.RESET_ALL}")
        print(f"   No response within {timeout} seconds, falling back to default pins")
        print()
        if client:
            await client.close()
        return (False, None, None, None)
    except KeyboardInterrupt:
        if client:
            await client.close()
        handle_keyboard_interrupt_properly()
        raise
    except (RuntimeError, OSError) as e:
        print()
        print(f"{Fore.YELLOW}⚠️  PIN DISCOVERY ERROR{Style.RESET_ALL}")
        print(f"   Serial error: {e}")
        print("   Falling back to default pins")
        print()
        if client:
            await client.close()
        return (False, None, None, None)
    except Exception as e:
        print()
        print(f"{Fore.YELLOW}⚠️  PIN DISCOVERY ERROR{Style.RESET_ALL}")
        print(f"   Unexpected error: {e}")
        print("   Falling back to default pins")
        print()
        if client:
            await client.close()
        return (False, None, None, None)


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
    lcd_rgb: bool
    object_fled: bool
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
    clean: bool

    # Strip size configuration (NEW - Phase 8)
    strip_sizes: str | None

    # Lane configuration (NEW)
    lanes: str | None

    # Per-lane LED counts (NEW)
    lane_counts: str | None

    # Color pattern (NEW)
    color_pattern: str | None

    # Legacy API testing
    legacy: bool

    # Chipset selection
    chipset: str

    # Network validation modes
    net_server: bool
    net_client: bool
    net: bool

    # OTA validation mode
    ota: bool

    # BLE validation mode
    ble: bool

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
    --parlio      Test only PARLIO driver
    --rmt         Test only RMT driver
    --spi         Test only SPI driver
    --uart        Test only UART driver
    --object-fled Test only ObjectFLED DMA driver (Teensy 4.x)
    --all         Test all drivers

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
        driver_group = parser.add_argument_group(
            "Driver Selection (optional — omit for GPIO-only mode)"
        )
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
            "--lcd-rgb",
            action="store_true",
            help="Test only LCD RGB driver (ESP32-P4 only)",
        )
        driver_group.add_argument(
            "--object-fled",
            action="store_true",
            help="Test only ObjectFLED DMA driver (Teensy 4.x only)",
        )
        driver_group.add_argument(
            "--all",
            action="store_true",
            help="Test all drivers (equivalent to --parlio --rmt --spi --uart --i2s --lcd-rgb --object-fled)",
        )
        driver_group.add_argument(
            "--simd",
            action="store_true",
            help="Test SIMD operations only (no LED drivers)",
        )

        # Network validation modes
        net_group = parser.add_argument_group(
            "Network Validation (ESP32 WiFi + HTTP)",
            "Test ESP32 WiFi soft AP and HTTP server/client functionality.",
        )
        net_group.add_argument(
            "--net-server",
            action="store_true",
            help="ESP32 starts WiFi AP + HTTP server; host connects and validates endpoints",
        )
        net_group.add_argument(
            "--net-client",
            action="store_true",
            help="ESP32 starts WiFi AP; host starts HTTP server; ESP32 fetches from host",
        )
        net_group.add_argument(
            "--net",
            action="store_true",
            help="Self-contained loopback test: ESP32 starts HTTP server, then GETs localhost (no WiFi needed)",
        )

        # OTA validation mode
        ota_group = parser.add_argument_group(
            "OTA Validation (ESP32 WiFi + OTA HTTP)",
            "Test ESP32 OTA (Over-The-Air) firmware update web interface.",
        )
        ota_group.add_argument(
            "--ota",
            action="store_true",
            help="ESP32 starts WiFi AP + OTA HTTP server; host validates auth and update endpoints",
        )

        # BLE validation mode
        ble_group = parser.add_argument_group(
            "BLE Validation (ESP32 BLE GATT)",
            "Test ESP32 BLE GATT server with JSON-RPC ping/pong over BLE.",
        )
        ble_group.add_argument(
            "--ble",
            action="store_true",
            help="ESP32 starts BLE GATT server; host connects via Bleak and validates ping/pong",
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
            help="Use fbuild for compile and upload instead of PlatformIO (default for esp32s3 and esp32c6)",
        )
        parser.add_argument(
            "--no-fbuild",
            action="store_true",
            help="Force PlatformIO even for esp32s3 (disables fbuild default)",
        )
        parser.add_argument(
            "--clean",
            action="store_true",
            help="Clean build before compiling (removes cached build artifacts)",
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

        # Legacy API testing
        parser.add_argument(
            "--legacy",
            action="store_true",
            help="Test using legacy template addLeds API (WS2812B<PIN>) instead of Channel API. Single-lane only, pin must be 0-8.",
        )

        # Chipset selection
        parser.add_argument(
            "--chipset",
            choices=["ws2812", "ucs7604"],
            default="ws2812",
            help="Chipset timing to use for validation (default: ws2812). ucs7604 uses UCS7604-800KHZ timing with 16-bit encoding.",
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
            lcd_rgb=parsed.lcd_rgb,
            object_fled=parsed.object_fled,
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
            clean=parsed.clean,
            strip_sizes=parsed.strip_sizes,  # NEW - Phase 8
            lanes=parsed.lanes,  # NEW
            lane_counts=parsed.lane_counts,  # NEW
            color_pattern=parsed.color_pattern,  # NEW
            legacy=parsed.legacy,
            chipset=parsed.chipset,
            net_server=parsed.net_server,
            net_client=parsed.net_client,
            net=parsed.net,
            ota=parsed.ota,
            ble=parsed.ble,
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


def _is_native_platform(environment: str | None) -> bool:
    """Check if the target is the native/stub (host) platform."""
    if not environment:
        return False
    return environment.lower() in ("native", "stub", "host")


async def _run_native_validation(args: Args, build_mode: str = "quick") -> int:
    """Run validation on native/stub platform via Meson compile + execute.

    Native validation is self-contained: the compiled binary discovers stub
    drivers, runs validateChipsetTiming() for each, and exits 0 (pass) or 1 (fail).
    No serial port, upload, or JSON-RPC needed.
    """
    from ci.util.meson_example_runner import run_meson_examples

    project_dir = args.project_dir.resolve()
    build_dir = project_dir / ".build" / "meson"

    # Banner
    print("─" * 60)
    print("FastLED Validation — Native Platform")
    print("─" * 60)
    print(f"  Mode        Compile + Run (Meson, {build_mode})")
    print(f"  Build dir   {project_dir / '.build' / f'meson-{build_mode}'}")
    print("─" * 60)
    print()

    # Phase 1: Optional lint
    if not args.skip_lint:
        from ci.debug_attached import run_cpp_lint

        if not run_cpp_lint():
            print("\n❌ Linting failed. Fix issues or use --skip-lint to bypass.")
            return 1
    else:
        print("⚠️  Skipping C++ linting (--skip-lint flag set)\n")

    # Phase 2+3: Meson setup + compile + run
    print("=" * 60)
    print("COMPILE + EXECUTE (Meson — native host)")
    print("=" * 60)

    result = run_meson_examples(
        source_dir=project_dir,
        build_dir=build_dir,
        examples=["Validation"],
        verbose=args.verbose,
        build_mode=build_mode,
        full=True,  # Compile AND execute
    )

    print()
    print("=" * 60)
    if result.success:
        print(f"{Fore.GREEN}✓ NATIVE VALIDATION SUCCEEDED{Style.RESET_ALL}")
    else:
        print(f"{Fore.RED}✗ NATIVE VALIDATION FAILED{Style.RESET_ALL}")
    print("=" * 60)

    return 0 if result.success else 1


def _try_teensy_bootloader_upload(build_dir: Path, environment: str | None) -> bool:
    """Try to detect Teensy in HalfKay bootloader mode and upload firmware.

    Teensy uses HID (not serial) for bootloader uploads. When the device is in
    bootloader mode (program button pressed), teensy_loader_cli can flash it
    even though no serial port is visible.

    Returns True if firmware was successfully uploaded.
    """
    if not environment:
        return False

    # Find teensy_loader_cli
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

    # Find the firmware hex
    hex_path = build_dir / ".pio" / "build" / environment / "firmware.hex"
    if not hex_path.exists():
        return False

    # Map environment to MCU name for teensy_loader_cli
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

    # Try a non-blocking check (no -w flag) to see if bootloader is present
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
# Main Entry Point
# ============================================================


async def run(args: Args | None = None) -> int:  # pyright: ignore[reportGeneralTypeIssues]
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

    # If --lcd-rgb is specified but no environment, force esp32p4
    # LCD RGB driver is only available on ESP32-P4
    if args.lcd_rgb and not final_environment:
        final_environment = "esp32p4"
        print(
            "ℹ️  --lcd-rgb flag requires ESP32-P4, auto-selecting 'esp32p4' environment"
        )

    # ============================================================
    # Driver Selection Validation
    # ============================================================
    drivers: list[str] = []

    # SIMD test mode - special case, no drivers needed
    simd_test_mode = args.simd

    # Check if any driver flags were specified
    if args.all:
        drivers = ["PARLIO", "RMT", "SPI", "UART", "I2S", "LCD_RGB", "OBJECTFLED"]
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
        if args.lcd_rgb:
            drivers.append("LCD_RGB")
        if args.object_fled:
            drivers.append("OBJECTFLED")

    # Network validation modes
    net_server_mode = args.net_server
    net_client_mode = args.net_client
    net_loopback_mode = args.net

    # OTA validation mode
    ota_mode = args.ota

    # BLE validation mode
    ble_mode = args.ble

    # Validate mutual exclusivity of net/ota/ble modes with driver modes
    if (
        net_server_mode or net_client_mode or net_loopback_mode or ota_mode or ble_mode
    ) and (drivers or simd_test_mode):
        print(
            f"{Fore.RED}❌ Error: --net/--net-server/--net-client/--ota/--ble cannot be combined with driver flags or --simd{Style.RESET_ALL}"
        )
        return 1
    net_mode_count = sum([net_server_mode, net_client_mode, net_loopback_mode])
    if net_mode_count > 1:
        print(
            f"{Fore.RED}❌ Error: --net, --net-server, and --net-client are mutually exclusive{Style.RESET_ALL}"
        )
        return 1
    if ota_mode and net_mode_count > 0:
        print(
            f"{Fore.RED}❌ Error: --ota cannot be combined with --net/--net-server/--net-client{Style.RESET_ALL}"
        )
        return 1
    if ble_mode and (net_mode_count > 0 or ota_mode):
        print(
            f"{Fore.RED}❌ Error: --ble cannot be combined with --net/--net-server/--net-client/--ota{Style.RESET_ALL}"
        )
        return 1

    # GPIO-only mode: no drivers, not SIMD, not net, not OTA, not BLE — just run GPIO pre-test
    gpio_only_mode = (
        not drivers
        and not simd_test_mode
        and not net_server_mode
        and not net_client_mode
        and not net_loopback_mode
        and not ota_mode
        and not ble_mode
    )
    if gpio_only_mode:
        print(
            f"\n{Fore.CYAN}ℹ️  No driver specified — running GPIO-only mode (pin discovery + toggle capture test){Style.RESET_ALL}"
        )

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

    # Enforce single-lane for --legacy mode
    if args.legacy:
        if max_lanes is not None and max_lanes > 1:
            print(
                "❌ Error: --legacy only supports single-lane (pin 0-8). Remove --lanes or use --lanes 1"
            )
            return 1
        # Force single lane
        min_lanes = 1
        max_lanes = 1
        print(
            "ℹ️  Legacy API mode: using WS2812B<PIN> template path (single-lane, pin 0-8)"
        )

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
        set_lane_sizes_cmd = {"method": "setLaneSizes", "params": [per_lane_counts]}
        rpc_commands_list.append(set_lane_sizes_cmd)
        print(
            f"ℹ️  Setting per-lane LED counts: {', '.join(str(c) for c in per_lane_counts)} ({len(per_lane_counts)} lanes)"
        )

    # Add custom color command if specified (NEW)
    # NOTE: This requires firmware support for a setSolidColor or setCustomPattern RPC command
    # For now, we'll add a placeholder that the firmware can implement
    if custom_color is not None:
        r, g, b = custom_color
        # RPC format: {"method": "setSolidColor", "params": [{"r": R, "g": G, "b": B}]}
        set_color_cmd = {
            "method": "setSolidColor",
            "params": [{"r": r, "g": g, "b": b}],
        }
        rpc_commands_list.append(set_color_cmd)
        print(
            "⚠️  Note: setSolidColor RPC command requires firmware support (may need implementation)"
        )

    # Map chipset CLI arg to timing name for firmware
    chipset_timing_map = {
        "ws2812": "WS2812B-V5",
        "ucs7604": "UCS7604-800KHZ",
    }
    timing_name = chipset_timing_map.get(args.chipset, "WS2812B-V5")

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
                # Format: {"method":"runSingleTest","params":{"driver":"PARLIO","laneSizes":[100,100],"pattern":"MSB_LSB_A","iterations":1}}
                # NOTE: params is the config object directly (not wrapped in array).
                # rpc_client.send() wraps it once: wrapped_args = [params] = [config_object]
                # Firmware receives params[0] = config_object (the fl::Json& args it expects)
                test_config: dict[str, Any] = {
                    "driver": driver,
                    "laneSizes": lane_sizes,
                    "pattern": "MSB_LSB_A",  # Default pattern
                    "iterations": 1,  # Default iterations
                    "timing": timing_name,
                }
                if args.legacy:
                    test_config["useLegacyApi"] = True
                rpc_command = {"method": "runSingleTest", "params": test_config}
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
    # Native Platform — bypass PlatformIO/serial/RPC entirely
    # ============================================================
    if _is_native_platform(final_environment):
        return await _run_native_validation(args)

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
    sketch_path = build_dir / "examples" / "Validation"
    if not sketch_path.exists():
        print(f"❌ Error: Validation sketch not found at {sketch_path}")
        return 1

    os.environ["PLATFORMIO_SRC_DIR"] = str(sketch_path)

    # ============================================================
    # Port Detection
    # ============================================================
    is_teensy = final_environment is not None and "teensy" in final_environment.lower()
    upload_port = args.upload_port
    if not upload_port:
        # Wait up to 60 seconds for a USB serial port to appear.
        # Devices like Teensy may take time to enumerate after power-cycle or
        # firmware upload, so poll with a fast exit on first detection.
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
                f"⏳ No USB serial port found yet. Waiting up to {max_wait_s}s for device..."
            )
            deadline = time.monotonic() + max_wait_s
            last_msg_at = 0.0
            while time.monotonic() < deadline:
                time.sleep(poll_interval_s)
                result = auto_detect_upload_port()
                if result.ok:
                    elapsed = max_wait_s - (deadline - time.monotonic())
                    print(
                        f"✅ USB serial port detected after {elapsed:.1f}s: {result.selected_port}"
                    )
                    break
                # For Teensy: also try to detect HalfKay bootloader and upload
                # firmware directly via teensy_loader_cli (uses HID, not serial)
                if is_teensy:
                    teensy_result = _try_teensy_bootloader_upload(
                        build_dir, final_environment
                    )
                    if teensy_result:
                        # Teensy was programmed via bootloader, wait for serial
                        print(
                            "✅ Firmware uploaded via Teensy bootloader, waiting for serial port..."
                        )
                        serial_deadline = time.monotonic() + 15
                        while time.monotonic() < serial_deadline:
                            time.sleep(1.0)
                            result = auto_detect_upload_port()
                            if result.ok:
                                print(
                                    f"✅ Teensy serial port detected: {result.selected_port}"
                                )
                                break
                        break
                # Print progress every 5 seconds
                now = time.monotonic()
                if now - last_msg_at >= 5.0:
                    remaining = deadline - now
                    print(
                        f"   Still waiting... {int(remaining)}s remaining",
                        flush=True,
                    )
                    last_msg_at = now

        if not result.ok:
            # Port detection failed after waiting - display detailed error and exit
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
            if is_teensy:
                print(
                    f"{Fore.YELLOW}Teensy hint: Hold the PROGRAM button while plugging in USB to force bootloader mode.{Style.RESET_ALL}"
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
        # Determine if fbuild should be used (default for esp32s3 and esp32c6)
        use_fbuild = _should_use_fbuild(
            final_environment, args.use_fbuild, args.no_fbuild
        )
        if use_fbuild:
            if args.use_fbuild:
                print("📦 Using fbuild (--use-fbuild specified)")
            else:
                print("📦 Using fbuild (default for esp32s3 and esp32c6)")
        else:
            if args.no_fbuild:
                print("📦 Using PlatformIO (--no-fbuild specified)")
            else:
                print("📦 Using PlatformIO")

        if use_fbuild:
            from ci.util.fbuild_runner import run_fbuild_compile

            if not run_fbuild_compile(
                build_dir, final_environment, args.verbose, clean=args.clean
            ):
                return 1
        else:
            if not run_compile(
                build_dir, final_environment, args.verbose, clean=args.clean
            ):
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
                    # Use pyserial to avoid event loop conflicts with fbuild
                    import serial

                    with serial.Serial(upload_port, 115200, timeout=0.1) as _ser:
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
        constrained_platforms = ["esp32c6", "esp32c2", "teensy41", "teensy40"]
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
                    method = cmd.get("method")
                    if not method:
                        validation_errors.append(
                            f"Command {i + 1}: Missing 'method' or 'function' field"
                        )
                        continue

                    args = cmd.get("params", [])

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

        # Create serial interface - use pyserial unless fbuild is active
        # fbuild's WebSocket-based serial has high latency (~25s) on some platforms
        if not use_fbuild:
            from ci.util.serial_interface import create_serial_interface

            serial_iface: SerialInterface | None = create_serial_interface(
                port=upload_port, use_pyserial=True
            )
        else:
            serial_iface = None  # Let RpcClient default to fbuild

        # Create crash trace decoder for inline stack trace decoding.
        crash_decoder: CrashTraceDecoder | None = None
        if final_environment:
            crash_decoder = CrashTraceDecoder(
                build_dir, final_environment, use_fbuild=use_fbuild
            )

        # Store discovery client for reuse (keep connection open!)
        discovery_client: RpcClient | None = None

        # Skip pin discovery and GPIO pre-test for SIMD, network, and OTA modes
        if simd_test_mode:
            print("\n📌 SIMD mode: skipping pin discovery and GPIO pre-test")
        elif net_server_mode or net_client_mode or net_loopback_mode:
            print("\n📌 Network mode: skipping pin discovery and GPIO pre-test")
        elif ota_mode:
            print("\n📌 OTA mode: skipping pin discovery and GPIO pre-test")
        elif ble_mode:
            print("\n📌 BLE mode: skipping pin discovery and GPIO pre-test")
        # CLI args take priority - skip discovery if user specified pins
        elif args.tx_pin is not None or args.rx_pin is not None:
            effective_tx_pin = args.tx_pin if args.tx_pin is not None else PIN_TX
            effective_rx_pin = args.rx_pin if args.rx_pin is not None else PIN_RX
            print(
                f"\n📌 Using CLI-specified pins: TX={effective_tx_pin}, RX={effective_rx_pin}"
            )
            # Add setPins RPC command
            set_pins_cmd = {
                "method": "setPins",
                "params": [{"txPin": effective_tx_pin, "rxPin": effective_rx_pin}],
            }
            json_rpc_commands.insert(0, set_pins_cmd)

        # Auto-discover pins if enabled and no CLI override
        elif args.auto_discover_pins:
            print("\n🔍 Auto-discovery enabled - searching for connected pins...")
            (
                success,
                discovered_tx,
                discovered_rx,
                discovery_client,
            ) = await run_pin_discovery(upload_port, serial_interface=serial_iface)

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
                # Close discovery client if we got one but discovery failed
                if discovery_client:
                    await discovery_client.close()
                    discovery_client = None
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
        # Skip GPIO pre-test if pins were just discovered (already verified), SIMD, net, or OTA mode
        if simd_test_mode:
            pass  # Already printed skip message above
        elif net_server_mode or net_client_mode or net_loopback_mode:
            pass  # Already printed skip message above
        elif ota_mode:
            pass  # Already printed skip message above
        elif ble_mode:
            pass  # Already printed skip message above
        elif pins_discovered:
            print("\n✅ Skipping GPIO pre-test (pins verified during discovery)")
        elif not await run_gpio_pretest(
            upload_port,
            effective_tx_pin or PIN_TX,
            effective_rx_pin or PIN_RX,
            serial_interface=serial_iface,  # Use same backend as pin discovery
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

        # GPIO-only mode: GPIO pre-test passed, no driver tests to run
        if gpio_only_mode:
            print()
            print("=" * 60)
            print(f"{Fore.GREEN}✓ GPIO-ONLY VALIDATION SUCCEEDED{Style.RESET_ALL}")
            print("=" * 60)
            print("Pin discovery and GPIO connectivity pre-test passed.")
            print("No driver tests requested — skipping RPC test matrix.")
            print()
            return 0

        # ============================================================
        # Network Validation Mode (--net-server or --net-client)
        # ============================================================
        if net_server_mode or net_client_mode:
            return await run_net_validation(
                upload_port=upload_port,
                serial_iface=serial_iface,
                net_server_mode=net_server_mode,
                net_client_mode=net_client_mode,
                timeout=timeout_seconds,
            )

        # ============================================================
        # Network Loopback Mode (--net)
        # ============================================================
        if net_loopback_mode:
            return await run_net_loopback_validation(
                upload_port=upload_port,
                serial_iface=serial_iface,
                timeout=timeout_seconds,
            )

        # ============================================================
        # OTA Validation Mode (--ota)
        # ============================================================
        if ota_mode:
            # Compute firmware path based on build system
            firmware_path: Path | None = None
            if final_environment is not None:
                if use_fbuild:
                    firmware_path = (
                        build_dir
                        / ".fbuild"
                        / "build"
                        / final_environment
                        / "firmware.bin"
                    )
                else:
                    firmware_path = (
                        build_dir
                        / ".pio"
                        / "build"
                        / final_environment
                        / "firmware.bin"
                    )
            return await run_ota_validation(
                upload_port=upload_port,
                serial_iface=serial_iface,
                timeout=timeout_seconds,
                firmware_path=firmware_path,
            )

        # ============================================================
        # BLE Validation Mode (--ble)
        # ============================================================
        if ble_mode:
            return await run_ble_validation(
                upload_port=upload_port,
                serial_iface=serial_iface,
                timeout=timeout_seconds,
            )

        # SIMD test mode - run comprehensive SIMD test suite via RPC
        if simd_test_mode:
            print()
            print("=" * 60)
            print("SIMD TEST MODE - Comprehensive Test Suite")
            print("=" * 60)
            print()

            client: RpcClient | None = None
            try:
                # Use short boot_wait - device already rebooted and we waited for port
                print("   Connecting to device...", end="", flush=True)
                client = RpcClient(
                    upload_port, timeout=30.0, serial_interface=serial_iface
                )
                await client.connect(
                    boot_wait=1.0
                )  # Reduced from 3.0s since we already waited
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
                if response.get("passed", False):
                    print(
                        f"{Fore.GREEN}SIMD TEST PASSED ({total} tests){Style.RESET_ALL}"
                    )
                    return 0
                else:
                    print(
                        f"{Fore.RED}SIMD TEST FAILED"
                        f" ({failed_count}/{total} failures){Style.RESET_ALL}"
                    )
                    return 1

            except RpcTimeoutError:
                print()  # Newline after partial status line
                print(f"{Fore.RED}SIMD TEST TIMEOUT{Style.RESET_ALL}")
                print("   No response from device within 30 seconds")
                return 1
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                print()  # Newline after partial status line
                print(f"{Fore.RED}SIMD TEST ERROR: {e}{Style.RESET_ALL}")
                return 1
            finally:
                if client is not None:
                    await client.close()

        # ============================================================
        # RPC CLIENT - Use RpcClient for reliable communication
        # ============================================================
        # Flow: connect → send RPCs via client.send() → get results from response

        # Kill port users before starting
        if upload_port:
            kill_port_users(upload_port)
            time.sleep(0.5)

        print()
        print("=" * 60)
        print("EXECUTING VALIDATION TESTS VIA RPC")
        print("=" * 60)

        test_failed = False
        stop_word_found: str | None = None

        client: RpcClient | None = None
        try:
            # Reuse existing connection from pin discovery, or create new one
            if discovery_client:
                client = discovery_client
            else:
                # Create new RPC client
                print(f"📡 Connecting to {upload_port}...")
                client = RpcClient(
                    upload_port,
                    timeout=timeout_seconds,
                    serial_interface=serial_iface,
                    verbose=True,  # Enable verbose mode to see debug output
                    crash_decoder=crash_decoder,
                )
                await client.connect(boot_wait=3.0, drain_boot=True)
            print(f"{Fore.GREEN}✓ Connected{Style.RESET_ALL}")

            # Send RPC commands sequentially
            print(f"\n🔧 Executing {len(json_rpc_commands)} RPC command(s)...")
            print("─" * 60)

            for i, cmd in enumerate(json_rpc_commands, 1):
                method = cmd.get("method", "unknown")
                params = cmd.get("params", [])

                print(f"\n[{i}/{len(json_rpc_commands)}] Calling {method}()...")

                try:
                    # Send RPC request with extended timeout for test execution
                    # runSingleTest can take 5-10 seconds for validation tests
                    response = await client.send(
                        method,
                        args=params if params else [],
                        timeout=120.0,  # Allow up to 2 minutes for test
                    )

                    # Parse response data
                    test_data = response.data

                    if not isinstance(test_data, dict):
                        print(
                            f"{Fore.YELLOW}⚠️  Unexpected response type: {type(test_data)}{Style.RESET_ALL}"
                        )
                        print(f"   Response: {test_data}")
                        continue

                    # Check test results
                    if test_data.get("success") and test_data.get("passed"):
                        stop_word_found = "OK"
                        print(f"{Fore.GREEN}✅ Test passed{Style.RESET_ALL}")

                        # Display test summary
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
                            print(f"   Duration: {duration_str}")

                        # Display per-pattern details if available
                        if "patterns" in test_data:
                            display_pattern_details(test_data)

                    elif test_data.get("success") and not test_data.get("passed"):
                        stop_word_found = "ERROR"
                        test_failed = True
                        print(f"{Fore.RED}❌ Test failed{Style.RESET_ALL}")

                        # Display failure details
                        if "firstFailure" in test_data:
                            failure = test_data["firstFailure"]
                            print(
                                f"   Failed pattern: {failure.get('pattern', 'unknown')}"
                            )
                            print(f"   Lane: {failure.get('lane', 'unknown')}")
                            print(f"   Expected: {failure.get('expected', 'unknown')}")
                            print(f"   Actual: {failure.get('actual', 'unknown')}")

                        # Display per-pattern details if available
                        if "patterns" in test_data:
                            display_pattern_details(test_data)

                    elif test_data.get("success") is False:
                        stop_word_found = "ERROR"
                        test_failed = True
                        print(f"{Fore.RED}❌ RPC command failed{Style.RESET_ALL}")
                        if "error" in test_data:
                            print(f"   Error: {test_data['error']}")

                    else:
                        # Generic success response (e.g., ping, status, setup commands)
                        stop_word_found = "OK"
                        print(f"{Fore.GREEN}✓ Command completed{Style.RESET_ALL}")
                        if test_data:
                            print(f"   Response: {test_data}")

                except RpcCrashError as crash_err:
                    print(
                        f"{Fore.RED}❌ Device crashed during {method}(){Style.RESET_ALL}"
                    )
                    # Decoded trace already printed by RpcClient.
                    if not crash_err.decoded_lines:
                        print("   (no decoded stack trace available)")
                    test_failed = True
                    stop_word_found = "ERROR"
                    break

                except RpcTimeoutError:
                    print(f"{Fore.RED}❌ RPC timeout{Style.RESET_ALL}")
                    print(f"   No response within {120}s")
                    test_failed = True
                    stop_word_found = "ERROR"
                    break

                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise

                except Exception as e:
                    print(f"{Fore.RED}❌ RPC error: {e}{Style.RESET_ALL}")
                    test_failed = True
                    stop_word_found = "ERROR"
                    break

        except KeyboardInterrupt:
            print("\n\n⚠️  Interrupted by user")
            handle_keyboard_interrupt_properly()
            return 130
        except Exception as e:
            print(f"\n{Fore.RED}❌ RPC client error: {e}{Style.RESET_ALL}")
            return 1
        finally:
            # Close RPC client
            if client is not None:
                await client.close()
                print(f"\n✅ RPC connection closed")

        print("─" * 60)

        # Check test results
        if test_failed:
            print(f"\n{Fore.RED}❌ VALIDATION FAILED{Style.RESET_ALL}")
            return 1

        if not stop_word_found:
            print(
                f"\n{Fore.YELLOW}⚠️  No test completion signal received{Style.RESET_ALL}"
            )
            return 1

        # If stop word found, validation succeeded
        if stop_word_found == "OK":
            print()
            print("=" * 60)
            print(f"{Fore.GREEN}✓ VALIDATION SUCCEEDED{Style.RESET_ALL}")
            print("=" * 60)
            return 0
        elif stop_word_found == "ERROR":
            print()
            print("=" * 60)
            print(f"{Fore.RED}✗ VALIDATION FAILED{Style.RESET_ALL}")
            print("=" * 60)
            return 1

        # Legacy parsing code removed - RPC responses already contain test results
        # This section is kept for backward compatibility but should not be reached
        if False:  # Disabled legacy code
            try:
                for line in []:
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
    return asyncio.run(run())


if __name__ == "__main__":
    sys.exit(main())
