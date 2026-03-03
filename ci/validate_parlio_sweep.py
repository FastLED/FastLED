#!/usr/bin/env python3
"""PARLIO sweep test for ESP32-C6 - tests multiple LED strip sizes and lane configurations.

This script connects to an already-flashed ESP32-C6 running the Validation.ino
sketch and runs a comprehensive test matrix via JSON-RPC:

Test matrix:
  - LED counts: 1, 10, 25, 50, 75, 100 (finer granularity for threshold detection)
  - Lane configs: 1 lane, 2 lanes, 4 lanes (both legacy and channel API)
  - 2-lane: asymmetric sizes (~25% difference)
  - 4-lane: asymmetric sizes (100%, 90%, 75%, 50%)

Usage:
    # First compile and upload Validation sketch to ESP32-C6:
    bash validate esp32c6 --parlio --legacy --skip-lint  # (or let this script handle it)

    # Then run the sweep test on the already-flashed device:
    uv run python ci/validate_parlio_sweep.py

    # With explicit port:
    uv run python ci/validate_parlio_sweep.py --port COM5

    # Skip compile/upload (device already flashed):
    uv run python ci/validate_parlio_sweep.py --skip-flash

    # Verbose mode:
    uv run python ci/validate_parlio_sweep.py --verbose
"""

from __future__ import annotations

import argparse
import asyncio
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


# Add project root to path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))  # noqa: SPI001

from ci.rpc_client import RpcClient, RpcCrashError, RpcTimeoutError
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.port_utils import auto_detect_upload_port
from ci.util.serial_interface import create_serial_interface


@dataclass
class TestCase:
    """Single test configuration."""

    base_led_count: int
    lane_count: int
    lane_sizes: list[int]
    use_legacy_api: bool
    driver: str = "PARLIO"
    pattern: str = "MSB_LSB_A"
    timing: str = "WS2812B-V5"

    @property
    def label(self) -> str:
        sizes_str = ",".join(str(s) for s in self.lane_sizes)
        api = "legacy" if self.use_legacy_api else "channel"
        return f"{self.driver} {self.lane_count}L [{sizes_str}] ({api})"


@dataclass
class TestResult:
    """Result from a single test case."""

    test_case: TestCase
    passed: bool
    success: bool  # RPC call succeeded
    error: str | None = None
    total_tests: int = 0
    passed_tests: int = 0
    duration_ms: int = 0
    show_duration_ms: int = 0
    raw_response: dict[str, Any] | None = None


def build_test_matrix() -> list[TestCase]:
    """Build the full test matrix: sizes x lanes x lane counts (1, 2, 4).

    Uses finer granularity LED counts (1, 10, 25, 50, 75, 100) to identify
    failure thresholds, especially for multi-lane configurations on
    memory-constrained devices like ESP32-C6.
    """
    base_sizes = [1, 10, 25, 50, 75, 100]
    cases: list[TestCase] = []

    for base_size in base_sizes:
        # 1-lane config: legacy API
        cases.append(
            TestCase(
                base_led_count=base_size,
                lane_count=1,
                lane_sizes=[base_size],
                use_legacy_api=True,
            )
        )

        # 1-lane config: channel API (skip base=1, redundant with legacy)
        if base_size > 1:
            cases.append(
                TestCase(
                    base_led_count=base_size,
                    lane_count=1,
                    lane_sizes=[base_size],
                    use_legacy_api=False,
                )
            )

        # 2-lane config: asymmetric sizes (~25% difference)
        lane2_size = max(1, round(base_size * 0.75))
        if lane2_size == base_size and base_size > 1:
            lane2_size = base_size - 1

        # 2-lane channel API
        cases.append(
            TestCase(
                base_led_count=base_size,
                lane_count=2,
                lane_sizes=[base_size, lane2_size],
                use_legacy_api=False,
            )
        )

        # 2-lane legacy API
        cases.append(
            TestCase(
                base_led_count=base_size,
                lane_count=2,
                lane_sizes=[base_size, lane2_size],
                use_legacy_api=True,
            )
        )

        # 4-lane config: asymmetric sizes (100%, 90%, 75%, 50%)
        lane_ratios = [1.0, 0.9, 0.75, 0.5]
        lane_sizes_4 = [max(1, round(base_size * r)) for r in lane_ratios]

        # 4-lane channel API (asymmetric)
        cases.append(
            TestCase(
                base_led_count=base_size,
                lane_count=4,
                lane_sizes=lane_sizes_4,
                use_legacy_api=False,
            )
        )

        # 4-lane legacy API (asymmetric)
        cases.append(
            TestCase(
                base_led_count=base_size,
                lane_count=4,
                lane_sizes=lane_sizes_4,
                use_legacy_api=True,
            )
        )

    return cases


async def run_sweep(
    port: str,
    verbose: bool = False,
    timeout: float = 120.0,
) -> list[TestResult]:
    """Run the full PARLIO sweep test matrix on the device.

    Args:
        port: Serial port path
        verbose: Enable verbose RPC output
        timeout: Per-test timeout in seconds

    Returns:
        List of TestResult objects
    """
    test_matrix = build_test_matrix()
    results: list[TestResult] = []

    print(f"\n{'=' * 70}")
    print(f"  PARLIO Sweep Test - ESP32-C6")
    print(f"  {len(test_matrix)} test configurations")
    print(f"  Port: {port}")
    print(f"{'=' * 70}")
    print()

    # Print test matrix
    print("Test Matrix:")
    print(f"  {'#':<3} {'Config':<40} {'Sizes':<20} {'API':<10}")
    print(f"  {'-' * 3} {'-' * 40} {'-' * 20} {'-' * 10}")
    for i, tc in enumerate(test_matrix, 1):
        sizes_str = ",".join(str(s) for s in tc.lane_sizes)
        api = "legacy" if tc.use_legacy_api else "channel"
        print(
            f"  {i:<3} {tc.driver} {tc.lane_count}L base={tc.base_led_count:<8} [{sizes_str}]{'':<{15 - len(sizes_str)}} {api}"
        )
    print()

    # Connect to device using pyserial (avoids fbuild daemon port locking issues)
    print(f"Connecting to {port}...")
    iface = create_serial_interface(port, use_pyserial=True)
    client = RpcClient(
        port,
        timeout=timeout,
        verbose=verbose,
        serial_interface=iface,
    )

    try:
        await client.connect(boot_wait=3.0, drain_boot=True)
        print("Connected\n")

        # Ping to verify device is alive
        try:
            ping_response = await client.send("ping", timeout=10.0)
            print(f"Device alive: {ping_response.data}")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"WARNING: Ping failed: {e}")

        print()
        print(f"{'=' * 70}")
        print(f"  Running Tests")
        print(f"{'=' * 70}")

        for i, tc in enumerate(test_matrix, 1):
            print(f"\n[{i}/{len(test_matrix)}] {tc.label}")
            print(f"  Lane sizes: {tc.lane_sizes}")

            # Build runSingleTest params
            test_config: dict[str, Any] = {
                "driver": tc.driver,
                "laneSizes": tc.lane_sizes,
                "pattern": tc.pattern,
                "iterations": 1,
                "timing": tc.timing,
            }
            if tc.use_legacy_api:
                test_config["useLegacyApi"] = True

            try:
                response = await client.send(
                    "runSingleTest",
                    args=test_config,  # type: ignore[arg-type]
                    timeout=timeout,
                )

                test_data = response.data

                if not isinstance(test_data, dict):
                    result = TestResult(
                        test_case=tc,
                        passed=False,
                        success=False,
                        error=f"Unexpected response type: {type(test_data)}",
                        raw_response={"raw": str(test_data)},
                    )
                elif test_data.get("success") and test_data.get("passed"):
                    result = TestResult(
                        test_case=tc,
                        passed=True,
                        success=True,
                        total_tests=test_data.get("totalTests", 0),
                        passed_tests=test_data.get("passedTests", 0),
                        duration_ms=test_data.get("duration_ms", 0),
                        show_duration_ms=test_data.get("show_duration_ms", 0),
                        raw_response=test_data,
                    )
                    print(
                        f"  PASS  ({test_data.get('passedTests', '?')}/{test_data.get('totalTests', '?')} tests, "
                        f"{test_data.get('duration_ms', '?')}ms)"
                    )
                elif test_data.get("success") and not test_data.get("passed"):
                    # Test ran but failed
                    error_msg = ""
                    if "firstFailure" in test_data:
                        failure = test_data["firstFailure"]
                        error_msg = (
                            f"pattern={failure.get('pattern', '?')}, "
                            f"lane={failure.get('lane', '?')}, "
                            f"expected={failure.get('expected', '?')}, "
                            f"actual={failure.get('actual', '?')}"
                        )

                    result = TestResult(
                        test_case=tc,
                        passed=False,
                        success=True,
                        error=error_msg or "Test failed (no details)",
                        total_tests=test_data.get("totalTests", 0),
                        passed_tests=test_data.get("passedTests", 0),
                        duration_ms=test_data.get("duration_ms", 0),
                        show_duration_ms=test_data.get("show_duration_ms", 0),
                        raw_response=test_data,
                    )
                    print(
                        f"  FAIL  ({test_data.get('passedTests', '?')}/{test_data.get('totalTests', '?')} tests)"
                    )
                    if error_msg:
                        print(f"  Error: {error_msg}")

                    # Display pattern details if available
                    if "patterns" in test_data:
                        for pat in test_data["patterns"]:
                            total_leds = pat.get("totalLeds", "?")
                            mismatched = pat.get("mismatchedLeds", "?")
                            mismatched_bytes = pat.get("mismatchedBytes", "?")
                            lsb_only = pat.get("lsbOnlyErrors", 0)
                            status_str = f"leds={total_leds} mismatched={mismatched} bytes={mismatched_bytes}"
                            if lsb_only:
                                status_str += f" lsbOnly={lsb_only}"
                            print(f"    [FAIL] {status_str}")
                            if "errors" in pat:
                                for err in pat["errors"][:3]:
                                    led_idx = err.get("led", err.get("byteIndex", "?"))
                                    expected = err.get("expected", "?")
                                    actual = err.get("actual", "?")
                                    # Handle both array format [R,G,B] and scalar format
                                    if isinstance(expected, list):
                                        exp_str = (
                                            "RGB("
                                            + ",".join(f"0x{v:02x}" for v in expected)
                                            + ")"
                                        )
                                    else:
                                        exp_str = (
                                            f"0x{expected:02x}"
                                            if isinstance(expected, int)
                                            else str(expected)
                                        )
                                    if isinstance(actual, list):
                                        act_str = (
                                            "RGB("
                                            + ",".join(f"0x{v:02x}" for v in actual)
                                            + ")"
                                        )
                                    else:
                                        act_str = (
                                            f"0x{actual:02x}"
                                            if isinstance(actual, int)
                                            else str(actual)
                                        )
                                    print(
                                        f"      LED[{led_idx}]: "
                                        f"expected={exp_str} "
                                        f"actual={act_str}"
                                    )
                else:
                    # RPC call itself failed
                    error_msg = test_data.get("error", "Unknown error")
                    result = TestResult(
                        test_case=tc,
                        passed=False,
                        success=False,
                        error=str(error_msg),
                        raw_response=test_data,
                    )
                    print(f"  ERROR: {error_msg}")

            except RpcCrashError as crash_err:
                result = TestResult(
                    test_case=tc,
                    passed=False,
                    success=False,
                    error=f"DEVICE CRASH: {crash_err}",
                )
                print(f"  CRASH: Device crashed!")
                if crash_err.decoded_lines:
                    for line in crash_err.decoded_lines:
                        print(f"    {line}")
                # Device crashed - stop testing
                results.append(result)
                print("\n  Stopping test sweep due to device crash.")
                break

            except RpcTimeoutError:
                result = TestResult(
                    test_case=tc,
                    passed=False,
                    success=False,
                    error=f"TIMEOUT after {timeout}s",
                )
                print(f"  TIMEOUT: No response within {timeout}s")

            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise

            except Exception as e:
                result = TestResult(
                    test_case=tc,
                    passed=False,
                    success=False,
                    error=str(e),
                )
                print(f"  ERROR: {e}")

            results.append(result)

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    finally:
        await client.close()
        print("\nRPC connection closed")

    return results


def print_summary(results: list[TestResult]) -> None:
    """Print a summary table of all test results."""
    print()
    print(f"{'=' * 70}")
    print(f"  RESULTS SUMMARY")
    print(f"{'=' * 70}")
    print()

    # Header
    print(
        f"  {'#':<3} {'Status':<8} {'Lanes':<6} {'Base':<6} {'Sizes':<15} {'API':<8} {'Tests':<10} {'Time':<10} {'Error'}"
    )
    print(
        f"  {'-' * 3} {'-' * 8} {'-' * 6} {'-' * 6} {'-' * 15} {'-' * 8} {'-' * 10} {'-' * 10} {'-' * 20}"
    )

    passed_count = 0
    failed_count = 0

    for i, r in enumerate(results, 1):
        tc = r.test_case
        status = "PASS" if r.passed else "FAIL"
        sizes_str = ",".join(str(s) for s in tc.lane_sizes)
        api = "legacy" if tc.use_legacy_api else "channel"
        tests_str = f"{r.passed_tests}/{r.total_tests}" if r.total_tests > 0 else "-"
        time_str = f"{r.duration_ms}ms" if r.duration_ms > 0 else "-"
        error_str = (r.error or "")[:40]

        if r.passed:
            passed_count += 1
        else:
            failed_count += 1

        print(
            f"  {i:<3} {status:<8} {tc.lane_count:<6} {tc.base_led_count:<6} {sizes_str:<15} {api:<8} {tests_str:<10} {time_str:<10} {error_str}"
        )

    print()
    total = len(results)
    print(f"  Total: {total} tests | Passed: {passed_count} | Failed: {failed_count}")

    if failed_count > 0:
        print(f"\n  FAILED TESTS:")
        for i, r in enumerate(results, 1):
            if not r.passed:
                tc = r.test_case
                sizes_str = ",".join(str(s) for s in tc.lane_sizes)
                print(f"    [{i}] {tc.label}: {r.error}")

    print()
    if failed_count == 0:
        print("  ALL TESTS PASSED")
    else:
        print(f"  {failed_count} TEST(S) FAILED")
    print(f"{'=' * 70}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="PARLIO sweep test for ESP32-C6 - tests multiple sizes and lane configs"
    )
    parser.add_argument(
        "--port",
        type=str,
        help="Serial port (auto-detected if not specified)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose RPC output",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=120.0,
        help="Per-test timeout in seconds (default: 120)",
    )
    parser.add_argument(
        "--skip-flash",
        action="store_true",
        help="Skip compile/upload (assume device already flashed)",
    )
    args = parser.parse_args()

    # Detect port
    port: str = args.port or ""
    if not port:
        print("Auto-detecting serial port...")
        result = auto_detect_upload_port()
        if not result.ok:
            print(
                "ERROR: No USB serial port found. Plug in the ESP32-C6 or use --port."
            )
            return 1
        port = str(result.selected_port)
        print(f"Found: {port}")

    # Optionally compile and upload
    if not args.skip_flash:
        print("\nCompiling and uploading Validation sketch to ESP32-C6...")
        print("(Use --skip-flash to skip this step if already flashed)\n")
        # Use bash validate to compile and upload, but skip the test phase
        # We just need the firmware on the device
        compile_cmd = [
            "bash",
            "compile",
            "esp32c6",
            "--examples",
            "Validation",
        ]
        print(f"Running: {' '.join(compile_cmd)}")
        compile_result = subprocess.run(compile_cmd, cwd=str(project_root))
        if compile_result.returncode != 0:
            print(f"ERROR: Compile failed with exit code {compile_result.returncode}")
            return 1

    # Run the sweep test
    results = asyncio.run(run_sweep(port, verbose=args.verbose, timeout=args.timeout))

    # Print summary
    print_summary(results)

    # Return exit code
    failed = sum(1 for r in results if not r.passed)
    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
