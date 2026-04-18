"""Shared context, constants, and display helpers for autoresearch."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any

from colorama import Fore, Style


if TYPE_CHECKING:
    from ci.autoresearch.build_driver import BuildDriver


# ============================================================
# Constants
# ============================================================

# GPIO pin definitions (must match AutoResearch.ino)
PIN_TX = 1  # TX pin used by FastLED drivers
PIN_RX = 0  # RX pin used by RMT receiver (ESP32-C6 default; ESP32 uses 2)

# No legacy expect patterns — JSON-RPC test_complete is the authoritative
# completion signal.
DEFAULT_EXPECT_PATTERNS: list[str] = []

# Stop patterns — early exit when test suite completes
STOP_PATTERN = "TEST_COMPLETED_EXIT_(OK|ERROR)"

# Default fail-on pattern
DEFAULT_FAIL_ON_PATTERN = "ERROR"

# Exit-on-error patterns for immediate device failure detection
EXIT_ON_ERROR_PATTERNS = [
    "ClearCommError",  # Device stuck (ISR hogging CPU, crashed, etc.)
    "register dump",  # ESP32 panic/crash with register state dump
]

# Crash patterns checked against ALL stdout output
_STDOUT_CRASH_PATTERNS: list[re.Pattern[str]] = [
    re.compile(r"register dump", re.IGNORECASE),
    re.compile(r"Guru Meditation", re.IGNORECASE),
    re.compile(r"abort\(\) was called", re.IGNORECASE),
    re.compile(r"Panic cause:", re.IGNORECASE),
    re.compile(r"assert failed", re.IGNORECASE),
]

# Input-on-trigger configuration
INPUT_ON_TRIGGER = None  # No legacy START command needed


# ============================================================
# QuietContext
# ============================================================


class QuietContext:
    """Manage quiet-mode output for AI agents.

    In quiet mode, opens a log file for verbose fbuild output redirection.
    Use emit() for compact summary lines (always printed to stdout).
    Use log_file property to redirect subprocess output to the log.

    Does NOT redirect sys.stdout — CrashPatternInterceptor in main()
    needs to see serial output on the real stdout.

    Usage::

        with QuietContext(args.quiet) as qctx:
            run_fbuild_deploy(..., log_file=qctx.log_file)
            qctx.emit("BUILD+FLASH ok 12.3s")
    """

    def __init__(self, quiet: bool, log_path: Path | None = None) -> None:
        self.quiet = quiet
        self.log_path = log_path or Path(".autoresearch_last.log")
        self._log_file: Any = None

    def __enter__(self) -> "QuietContext":
        if self.quiet:
            self._log_file = open(self.log_path, "w", encoding="utf-8")  # noqa: SIM115
        return self

    def __exit__(self, *_args: Any) -> None:
        if self._log_file is not None:
            self._log_file.close()
            self._log_file = None

    @property
    def log_file(self) -> Any:
        """Log file handle (open in quiet mode, None otherwise)."""
        return self._log_file

    def emit(self, msg: str) -> None:
        """Print a compact summary line to stdout (always visible)."""
        print(msg)

    def emit_log_path(self) -> None:
        """Print log file path so agents know where to look on failure."""
        if self.quiet:
            print(f"LOG {self.log_path}")


# ============================================================
# RunContext
# ============================================================


@dataclass
class RunContext:
    """State passed between autoresearch pipeline phases."""

    args: Any  # Args (avoid circular import)
    drivers: list[str]
    json_rpc_commands: list[dict[str, Any]]
    expect_keywords: list[str]
    fail_keywords: list[str]
    timeout_seconds: float
    build_dir: Path

    # Mode flags
    simd_test_mode: bool
    coroutine_test_mode: bool
    net_server_mode: bool
    net_client_mode: bool
    net_loopback_mode: bool
    ota_mode: bool
    ble_mode: bool
    decode_mode: bool
    gpio_only_mode: bool
    parallel_mode: bool

    # Resolved after port/environment detection
    final_environment: str | None = None
    upload_port: str | None = None
    use_fbuild: bool = False

    # Build driver (set during _resolve_port_and_environment)
    build_driver: BuildDriver | None = None

    # Set during pin setup
    serial_iface: Any = None
    crash_decoder: Any = None
    effective_tx_pin: int | None = None
    effective_rx_pin: int | None = None
    discovery_client: Any = None
    pins_discovered: bool = False


# ============================================================
# Display Helpers
# ============================================================


def display_pattern_details(result: dict[str, Any]) -> None:
    """Display per-pattern error details from enriched RPC response."""
    patterns = result.get("patterns")
    if not patterns:
        return

    driver = result.get("driver", "?")
    lane_count = result.get("laneCount", 0)
    lane_sizes = result.get("laneSizes", [])
    total_leds = sum(lane_sizes) if lane_sizes else 0
    frame_count = result.get("frameCount", 1)

    print()
    print("=" * 62)
    header = f"{driver} — {lane_count} lane(s), {total_leds} LEDs"
    if frame_count and frame_count > 1:
        header += f", frames={frame_count}"
    print(header)
    print("=" * 62)

    agg_total_bytes = 0
    agg_mismatched_bytes = 0
    agg_lsb_only = 0

    for pat in patterns:
        name = pat.get("name", "Unknown")
        passed = pat.get("passed", False)
        run_number = pat.get("runNumber")
        status = (
            f"{Fore.GREEN}PASS{Style.RESET_ALL}"
            if passed
            else f"{Fore.RED}FAIL{Style.RESET_ALL}"
        )

        frame_suffix = f" [frame {run_number}]" if run_number else ""
        print(f"\n  {name}{frame_suffix}  {status}")

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


def print_run_summary(ctx: RunContext) -> None:
    """Print the compact configuration header."""
    print("\u2500" * 60)
    print("FastLED AutoResearch Test Runner")
    print("\u2500" * 60)
    env_str = ctx.final_environment or "auto-detect"
    print(f"  Target      {env_str} @ {ctx.upload_port}")
    print(f"  Drivers     {', '.join(ctx.drivers)}")
    print(f"  Timeout     {ctx.timeout_seconds}s")
    print(
        f"  Patterns    {len(ctx.expect_keywords)} expect, {len(ctx.fail_keywords)} fail, stop on '{STOP_PATTERN}'"
    )
    print("\u2500" * 60)
    print()
