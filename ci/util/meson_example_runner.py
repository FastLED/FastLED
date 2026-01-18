from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""Meson build system integration for FastLED example compilation and execution."""

import sys
import time
from pathlib import Path

from running_process import RunningProcess

from ci.util.build_lock import libfastled_build_lock
from ci.util.meson_runner import (
    MesonTestResult,
    check_meson_installed,
    perform_ninja_maintenance,
    setup_meson_build,
)
from ci.util.output_formatter import TimestampFormatter, create_filtering_echo_callback
from ci.util.timestamp_print import ts_print as _ts_print


def compile_examples(
    build_dir: Path,
    examples: list[str] | None = None,
    verbose: bool = False,
    parallel: bool = True,
    build_mode: str = "quick",
) -> bool:
    """
    Compile FastLED examples using Meson.

    Args:
        build_dir: Meson build directory
        examples: List of example names to compile (None = all)
        verbose: Enable verbose compilation output
        parallel: Enable parallel compilation (default: True)
        build_mode: Build mode ("quick", "debug", or "release")

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
    # Note: process_group already shows "Compiling: <examples>" status
    if examples is None:
        # Build all examples via the alias target
        cmd.append("examples-host")
    else:
        # Build specific example targets
        for example_name in examples:
            cmd.append(f"example-{example_name}")

    try:
        # Use RunningProcess for streaming output
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout
            auto_run=True,
            check=False,  # We'll check returncode manually
            # DLLs are auto-deployed by clang-tool-chain 1.0.31+ to output directory
            output_formatter=TimestampFormatter(),
        )

        # Use filtering callback in verbose mode to suppress noise patterns
        echo_callback = create_filtering_echo_callback() if verbose else False
        returncode = proc.wait(echo=echo_callback)

        if returncode != 0:
            _ts_print(
                f"Compilation failed (return code {returncode})",
                file=sys.stderr,
            )

            # Show output if not already shown
            if not verbose and proc.stdout:
                _ts_print("Compilation output:", file=sys.stderr)
                _ts_print(proc.stdout, file=sys.stderr)

            return False

        # Note: Don't print "Compilation successful" - it's redundant
        # The transition to "Running:" phase implies compilation succeeded
        return True

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"Compilation failed: {e}", file=sys.stderr)
        return False


def run_examples(
    build_dir: Path,
    examples: list[str] | None = None,
    verbose: bool = False,
    timeout: int = 30,
    build_mode: str = "quick",
) -> MesonTestResult:
    """
    Run compiled FastLED examples using Meson test runner.

    Args:
        build_dir: Meson build directory
        examples: List of example names to run (None = all)
        verbose: Enable verbose test output
        timeout: Timeout per example in seconds (default: 30)
        build_mode: Build mode ("quick", "debug", or "release")

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
        if not verbose:
            _ts_print("Running all examples...")
    else:
        # Run specific examples by name pattern
        # Meson test names are "example-<name>"
        for example_name in examples:
            # Add each example as a separate test argument
            cmd.append(f"example-{example_name}")
        # In verbose mode, meson output shows running tests - skip redundant message
        if not verbose:
            _ts_print(f"Running: {', '.join(examples)}")

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
            # DLLs are auto-deployed by clang-tool-chain 1.0.31+ to output directory
            output_formatter=TimestampFormatter(),
        )

        # Use filtering callback in verbose mode to suppress noise patterns
        echo_callback = create_filtering_echo_callback() if verbose else False
        returncode = proc.wait(echo=echo_callback)

        if returncode != 0:
            _ts_print(
                f"Examples failed (return code {returncode})",
                file=sys.stderr,
            )
            return MesonTestResult(
                success=False,
                duration=time.time() - start_time,
                num_tests_run=num_run,
                num_tests_passed=num_passed,
                num_tests_failed=num_failed,
            )

        # Note: Don't print "All examples passed" - the results table shows pass/fail status
        return MesonTestResult(
            success=True,
            duration=time.time() - start_time,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        duration = time.time() - start_time
        _ts_print(f"Execution failed: {e}", file=sys.stderr)
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
    debug: bool = False,
    build_mode: str | None = None,
    no_pch: bool = False,
    parallel: bool = True,
    full: bool = False,
) -> MesonTestResult:
    """
    Complete Meson example build and execution workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory (will be made mode-specific)
        examples: List of example names to compile/run (None = all)
        clean: Clean build directory before setup
        verbose: Enable verbose output
        debug: Enable debug mode (full symbols + sanitizers)
        build_mode: Override build mode ('quick', 'debug', 'release')
        no_pch: Disable precompiled headers (NOT IMPLEMENTED - PCH always enabled)
        parallel: Enable parallel compilation (default: True)
        full: Execute examples after compilation (default: False)

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    start_time = time.time()

    # Determine build mode (build_mode parameter takes precedence over debug flag)
    if build_mode is None:
        build_mode = "debug" if debug else "quick"

    # Validate build mode
    valid_modes = ["quick", "debug", "release"]
    if build_mode not in valid_modes:
        _ts_print(
            f"[MESON] Error: Invalid build mode: {build_mode}. "
            f"Valid modes: {', '.join(valid_modes)}",
            file=sys.stderr,
        )
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Construct mode-specific build directory
    # This enables caching per mode when source unchanged but flags differ
    # Example: .build/meson -> .build/meson-debug
    original_build_dir = build_dir
    build_dir = build_dir.parent / f"{build_dir.name}-{build_mode}"

    # Build directory removed from output - build mode already shown in Config line
    # and full path rarely useful to users (just noise)

    # Check if Meson is installed
    if not check_meson_installed():
        _ts_print("[MESON] Error: Meson build system is not installed", file=sys.stderr)
        _ts_print("[MESON] Install with: pip install meson ninja", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Clean if requested
    if clean and build_dir.exists():
        _ts_print(f"[MESON] Cleaning build directory: {build_dir}")
        import shutil

        shutil.rmtree(build_dir)

    # Setup build with explicit build_mode to ensure proper cache invalidation
    if not setup_meson_build(
        source_dir,
        build_dir,
        debug=(build_mode == "debug"),
        reconfigure=False,
        build_mode=build_mode,
        verbose=verbose,
    ):
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Perform periodic maintenance on Ninja dependency database
    perform_ninja_maintenance(build_dir)

    # Compile examples with build lock to prevent conflicts
    try:
        with libfastled_build_lock(timeout=600):  # 10 minute timeout
            if not compile_examples(
                build_dir,
                examples=examples,
                verbose=verbose,
                parallel=parallel,
                build_mode=build_mode,
            ):
                duration = time.time() - start_time
                out: MesonTestResult = MesonTestResult.construct_build_error(
                    time.time() - start_time
                )
                return out
    except TimeoutError as e:
        _ts_print(f"[MESON] {e}", file=sys.stderr)
        duration = time.time() - start_time
        out: MesonTestResult = MesonTestResult.construct_build_error(duration)
        return out

    # If full mode, run the examples
    if full:
        result = run_examples(
            build_dir,
            examples=examples,
            verbose=verbose,
            timeout=60,
            build_mode=build_mode,
        )
        return result
    else:
        # Just compilation, return success
        duration = time.time() - start_time
        _ts_print(f"Compilation complete ({duration:.2f}s)")
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )


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
        "--debug",
        action="store_true",
        help="Enable debug mode (full symbols + sanitizers)",
    )
    parser.add_argument(
        "--build-mode",
        type=str,
        choices=["quick", "debug", "release"],
        default=None,
        help="Override build mode (default: quick, or debug if --debug flag set)",
    )
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
        debug=args.debug,
        build_mode=args.build_mode,
        no_pch=args.no_pch,
        parallel=not args.no_parallel,
        full=args.full,
    )

    sys.exit(0 if result.success else 1)
