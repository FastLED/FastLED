#!/usr/bin/env python3
"""Meson build system integration for FastLED example compilation and execution."""

import os
import re
import sys
import time
from pathlib import Path
from typing import Optional

from running_process import RunningProcess

from ci.util.build_lock import libfastled_build_lock
from ci.util.meson_runner import (
    MesonTestResult,
    check_meson_installed,
    perform_ninja_maintenance,
    setup_meson_build,
)
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


def compile_examples(
    build_dir: Path,
    examples: list[str] | None = None,
    verbose: bool = False,
    parallel: bool = True,
) -> bool:
    """
    Compile FastLED examples using Meson.

    Args:
        build_dir: Meson build directory
        examples: List of example names to compile (None = all)
        verbose: Enable verbose compilation output
        parallel: Enable parallel compilation (default: True)

    Returns:
        True if compilation successful, False otherwise
    """
    # Build command
    cmd = ["meson", "compile", "-C", str(build_dir)]

    if verbose:
        cmd.append("-v")

    if not parallel:
        # Sequential compilation (-j 1)
        cmd.extend(["-j", "1"])

    # Determine targets to build
    if examples is None:
        # Build all examples via the alias target
        cmd.append("examples-host")
        _ts_print(f"[MESON] Compiling all examples...")
    else:
        # Build specific example targets
        for example_name in examples:
            cmd.append(f"example-{example_name}")
        _ts_print(f"[MESON] Compiling {len(examples)} examples: {', '.join(examples)}")

    try:
        # Use RunningProcess for streaming output
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment
            output_formatter=TimestampFormatter(),
        )

        returncode = proc.wait(echo=verbose)

        if returncode != 0:
            _ts_print(
                f"[MESON] Example compilation failed with return code {returncode}",
                file=sys.stderr,
            )

            # Show output if not already shown
            if not verbose and proc.stdout:
                _ts_print("[MESON] Compilation output:", file=sys.stderr)
                _ts_print(proc.stdout, file=sys.stderr)

            return False

        _ts_print(f"[MESON] Example compilation successful")
        return True

    except KeyboardInterrupt:
        raise
    except Exception as e:
        _ts_print(
            f"[MESON] Example compilation failed with exception: {e}", file=sys.stderr
        )
        return False


def run_examples(
    build_dir: Path,
    examples: list[str] | None = None,
    verbose: bool = False,
    timeout: int = 30,
) -> MesonTestResult:
    """
    Run compiled FastLED examples using Meson test runner.

    Args:
        build_dir: Meson build directory
        examples: List of example names to run (None = all)
        verbose: Enable verbose test output
        timeout: Timeout per example in seconds (default: 30)

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    # Build command
    cmd = ["meson", "test", "-C", str(build_dir), "--print-errorlogs"]

    if verbose:
        cmd.append("-v")

    # Set timeout
    cmd.extend(["--timeout-multiplier", str(timeout / 30.0)])  # 30s is default

    # Determine which tests to run
    if examples is None:
        # Run all tests in the 'examples' suite
        cmd.extend(["--suite", "examples"])
        _ts_print(f"[MESON] Running all examples...")
    else:
        # Run specific examples by name pattern
        # Meson test names are "example-<name>"
        for example_name in examples:
            # Add each example as a separate test argument
            cmd.append(f"example-{example_name}")
        _ts_print(f"[MESON] Running {len(examples)} examples: {', '.join(examples)}")

    start_time = time.time()
    num_passed = 0
    num_failed = 0
    num_run = 0

    try:
        # Use RunningProcess for streaming output
        proc = RunningProcess(
            cmd,
            timeout=1800,  # 30 minute total timeout
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),
            output_formatter=TimestampFormatter(),
        )

        # Parse output in real-time to show test progress
        # Pattern matches: "1/96 example-Blink       OK              0.12s"
        # Note: Meson uses variable whitespace between fields
        test_pattern = re.compile(
            r"^(\d+)/(\d+)\s+(example-\S+)\s+(OK|FAIL|SKIP|TIMEOUT)\s+([\d.]+)s",
            re.MULTILINE,
        )

        returncode = proc.wait(echo=verbose)

        # Parse the complete output to extract test results (works for both verbose and non-verbose)
        output = proc.stdout

        for line in output.splitlines():
            # Strip timestamp prefix from TimestampFormatter (e.g., "0.63 " at start)
            # Timestamps are in format: "0.63 " or "10.32 " (seconds with decimal)
            line_without_timestamp = re.sub(r"^\d+\.\d+\s+", "", line)

            # Try to parse test result line (works for both verbose and non-verbose)
            match = test_pattern.match(line_without_timestamp)
            if match:
                current = int(match.group(1))
                total = int(match.group(2))
                test_name = match.group(3)
                status = match.group(4)
                duration_str = match.group(5)

                num_run = current
                if status == "OK":
                    num_passed += 1
                    if not verbose:
                        # Show brief progress for passed tests in non-verbose mode
                        _ts_print(
                            f"  [{current}/{total}] ✓ {test_name} ({duration_str}s)"
                        )
                elif status == "FAIL":
                    num_failed += 1
                    if not verbose:
                        _ts_print(
                            f"  [{current}/{total}] ✗ {test_name} FAILED ({duration_str}s)"
                        )
                elif status == "TIMEOUT":
                    num_failed += 1
                    if not verbose:
                        _ts_print(
                            f"  [{current}/{total}] ⏱ {test_name} TIMEOUT ({duration_str}s)"
                        )

        # If test failed and we didn't see individual test results, show error context
        if returncode != 0 and num_run == 0 and output:
            _ts_print("[MESON] Example execution output:", file=sys.stderr)
            _ts_print(output, file=sys.stderr)

        duration = time.time() - start_time

        if returncode != 0:
            _ts_print(
                f"[MESON] Examples failed with return code {returncode}",
                file=sys.stderr,
            )
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=num_run,
                num_tests_passed=num_passed,
                num_tests_failed=num_failed,
            )

        _ts_print(
            f"[MESON] All examples passed ({num_passed}/{num_run} examples in {duration:.2f}s)"
        )
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )

    except KeyboardInterrupt:
        raise
    except Exception as e:
        duration = time.time() - start_time
        _ts_print(
            f"[MESON] Example execution failed with exception: {e}", file=sys.stderr
        )
        return MesonTestResult(
            success=False,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )


def run_meson_examples(
    source_dir: Path,
    build_dir: Path,
    examples: list[str] | None = None,
    clean: bool = False,
    verbose: bool = False,
    no_pch: bool = False,
    parallel: bool = True,
    full: bool = False,
) -> MesonTestResult:
    """
    Complete Meson example build and execution workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory
        examples: List of example names to compile/run (None = all)
        clean: Clean build directory before setup
        verbose: Enable verbose output
        no_pch: Disable precompiled headers (NOT IMPLEMENTED - PCH always enabled)
        parallel: Enable parallel compilation (default: True)
        full: Execute examples after compilation (default: False)

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    start_time = time.time()

    # Check if Meson is installed
    if not check_meson_installed():
        _ts_print("[MESON] Error: Meson build system is not installed", file=sys.stderr)
        _ts_print("[MESON] Install with: pip install meson ninja", file=sys.stderr)
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # Clean if requested
    if clean and build_dir.exists():
        _ts_print(f"[MESON] Cleaning build directory: {build_dir}")
        import shutil

        shutil.rmtree(build_dir)

    # Setup build
    if not setup_meson_build(source_dir, build_dir, reconfigure=False):
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # Perform periodic maintenance on Ninja dependency database
    perform_ninja_maintenance(build_dir)

    # Compile examples with build lock to prevent conflicts
    try:
        with libfastled_build_lock(timeout=600):  # 10 minute timeout
            if not compile_examples(
                build_dir, examples=examples, verbose=verbose, parallel=parallel
            ):
                return MesonTestResult(success=False, duration=time.time() - start_time)
    except TimeoutError as e:
        _ts_print(f"[MESON] {e}", file=sys.stderr)
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # If full mode, run the examples
    if full:
        result = run_examples(build_dir, examples=examples, verbose=verbose, timeout=60)
        return result
    else:
        # Just compilation, return success
        duration = time.time() - start_time
        _ts_print(f"[MESON] Example compilation complete in {duration:.2f}s")
        return MesonTestResult(success=True, duration=duration)


if __name__ == "__main__":
    # Simple CLI for testing
    import argparse

    parser = argparse.ArgumentParser(
        description="Meson example compilation and execution runner for FastLED"
    )
    parser.add_argument(
        "examples", nargs="*", help="Specific examples to compile/run (None = all)"
    )
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument(
        "--no-pch", action="store_true", help="Disable precompiled headers (ignored)"
    )
    parser.add_argument(
        "--no-parallel", action="store_true", help="Disable parallel compilation"
    )
    parser.add_argument(
        "--full", action="store_true", help="Execute examples after compilation"
    )
    parser.add_argument("--build-dir", default=".build/meson", help="Build directory")

    args = parser.parse_args()

    source_dir = Path.cwd()
    build_dir = Path(args.build_dir)

    examples_list = args.examples if args.examples else None

    result = run_meson_examples(
        source_dir=source_dir,
        build_dir=build_dir,
        examples=examples_list,
        clean=args.clean,
        verbose=args.verbose,
        no_pch=args.no_pch,
        parallel=not args.no_parallel,
        full=args.full,
    )

    sys.exit(0 if result.success else 1)
