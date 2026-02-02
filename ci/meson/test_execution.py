"""Individual test execution via Meson build system."""

import re
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional

from running_process import RunningProcess

from ci.meson.compiler import get_meson_executable
from ci.meson.output import (
    _print_banner,
    _print_error,
    _print_success,
)
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter, create_filtering_echo_callback
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class MesonTestResult:
    """Result from running Meson build and tests"""

    success: bool
    duration: float  # Total duration in seconds
    num_tests_run: int  # Number of tests executed
    num_tests_passed: int  # Number of tests that passed
    num_tests_failed: int  # Number of tests that failed

    @staticmethod
    def construct_build_error(duration: float) -> "MesonTestResult":
        """Construct MesonTestResult with error status and duration"""
        out = MesonTestResult(
            success=False,
            duration=duration,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )
        return out


def run_meson_test(
    build_dir: Path, test_name: Optional[str] = None, verbose: bool = False
) -> MesonTestResult:
    """
    Run tests using Meson.

    Args:
        build_dir: Meson build directory
        test_name: Specific test to run (None = all)
        verbose: Enable verbose test output

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    # Import here to avoid circular dependency with compile module
    from ci.meson.compile import _create_error_context_filter

    cmd = [
        get_meson_executable(),
        "test",
        "-C",
        str(build_dir),
        "--print-errorlogs",
    ]

    if verbose:
        cmd.append("-v")

    if test_name:
        cmd.append(test_name)
        _print_banner("Test", "▶️", verbose=verbose)
        print(f"Running: {test_name}")
    else:
        _print_banner("Test", "▶️", verbose=verbose)
        print("Running all tests...")

    start_time = time.time()
    num_passed = 0
    num_failed = 0
    num_run = 0
    expected_total = 0  # Track Meson's expected total from output

    try:
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for tests
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=None,  # Pass current environment with wrapper paths
            output_formatter=TimestampFormatter(),
        )

        # Parse output in real-time to show test progress
        # Pattern matches: "1/143 test_name       OK     0.12s"
        # Note: TimestampFormatter adds "XX.XX " prefix, so we need to skip it
        test_pattern = re.compile(
            r"^[\d.]+\s+(\d+)/(\d+)\s+(\S+)\s+(OK|FAIL|SKIP|TIMEOUT)\s+([\d.]+)s"
        )

        if verbose:
            # Use filtering callback in verbose mode to suppress noise patterns
            echo_callback = create_filtering_echo_callback()
            returncode = proc.wait(echo=echo_callback)
        else:
            # Stream output line by line to show test progress
            with proc.line_iter(timeout=None) as it:
                for line in it:
                    # Try to parse test result line
                    match = test_pattern.match(line)
                    if match:
                        current = int(match.group(1))
                        total = int(match.group(2))
                        test_name_match = match.group(3)
                        status = match.group(4)
                        duration_str = match.group(5)

                        num_run = current
                        expected_total = (
                            total  # Update expected total from Meson output
                        )
                        if status == "OK":
                            num_passed += 1
                            # Show brief progress for passed tests in green
                            _print_success(
                                f"  [{current}/{total}] ✓ {test_name_match} ({duration_str}s)"
                            )
                        elif status == "FAIL":
                            num_failed += 1
                            _print_error(
                                f"  [{current}/{total}] ✗ {test_name_match} FAILED ({duration_str}s)"
                            )
                        elif status == "TIMEOUT":
                            num_failed += 1
                            _print_error(
                                f"  [{current}/{total}] ⏱ {test_name_match} TIMEOUT ({duration_str}s)"
                            )
                    elif verbose or "FAILED" in line or "ERROR" in line:
                        # Show error/important lines
                        _ts_print(f"  {line}")

            returncode = proc.wait()

            # If test failed, show error context
            if returncode != 0:
                # Create error-detecting filter with reasonable limit for test failures
                # (compilation uses limit=3, test failures show more but still capped)
                error_filter: Callable[[str], None] = _create_error_context_filter(
                    context_lines=20,
                    max_unique_errors=15,  # Show first 15 unique failures
                )

                # Process accumulated output to show error context
                output_lines = proc.stdout.splitlines()
                for line in output_lines:
                    error_filter(line)

        duration = time.time() - start_time

        if returncode != 0:
            _print_error(f"[MESON] ❌ Tests failed with return code {returncode}")
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=num_run,
                num_tests_passed=num_passed,
                num_tests_failed=num_failed,
            )

        # When Meson returns success (exit code 0), all tests passed.
        # Use expected_total from Meson's output as the true count, since parallel
        # test execution may cause some output lines to get interleaved and not
        # be parsed by our regex (causing num_passed to undercount).
        actual_passed = expected_total if expected_total > 0 else num_passed
        actual_run = expected_total if expected_total > 0 else num_run

        _print_success(
            f"✅ All tests passed ({actual_passed}/{actual_run} in {duration:.2f}s)"
        )
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=actual_run,
            num_tests_passed=actual_passed,
            num_tests_failed=0,  # Meson returned 0, so no failures
        )

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        duration = time.time() - start_time
        _ts_print(f"[MESON] Test execution failed with exception: {e}")
        return MesonTestResult(
            success=False,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )
