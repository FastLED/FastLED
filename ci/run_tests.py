from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Simple test runner for FastLED unit tests.
Discovers and runs test executables in parallel with clean output handling.
"""

import argparse
import os
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from running_process import RunningProcess

from ci.util.test_exceptions import TestExecutionFailedException, TestFailureInfo


_ABORT_EVENT = threading.Event()


_HERE = Path(__file__).parent
_PROJECT_ROOT = _HERE.parent


@dataclass
class TestResult:
    """Result of a test execution"""

    name: str
    success: bool
    duration: float
    output: str
    captured_lines: list[str] = field(default_factory=list)  # type: ignore
    return_code: Optional[int] = None


@dataclass
class Args:
    """Typed arguments for the test runner"""

    test: Optional[str] = None
    verbose: bool = False
    jobs: Optional[int] = None
    enable_stack_trace: bool = False


def _is_test_executable(f: Path) -> bool:
    """Check if a file is a valid test executable"""
    return (
        f.is_file() and f.suffix not in [".o", ".obj", ".pdb"] and os.access(f, os.X_OK)
    )


def _get_test_patterns() -> list[str]:
    """Get test patterns based on platform"""
    # On Windows, check both .exe and no extension (Clang generates without .exe)
    return ["test_*.exe"] if sys.platform == "win32" else ["test_*"]


def _analyze_crash_type(return_code: int, output: str) -> Optional[str]:
    """Analyze crash type based on return code and output"""
    if sys.platform == "win32":
        # Windows-specific exit codes
        if return_code == 127:
            return "🚨 CRASH TYPE: Missing executable dependencies or library loading failure"
        elif return_code == -1073741819 or return_code == 3221225477:  # 0xC0000005
            return "🚨 CRASH TYPE: Access violation (segmentation fault equivalent on Windows)"
        elif return_code == -1073741795 or return_code == 3221225501:  # 0xC0000025
            return "🚨 CRASH TYPE: Stack overflow"
        elif return_code == -1073741510 or return_code == 3221225786:  # 0xC000013A
            return "🚨 CRASH TYPE: Application terminated by Ctrl+C"
        elif return_code == -1073741676 or return_code == 3221225620:  # 0xC0000094
            return "🚨 CRASH TYPE: Integer division by zero"
        elif return_code == -1073740791 or return_code == 3221226505:  # 0xC0000409
            return "🚨 CRASH TYPE: Stack buffer overflow detected by security check"
        elif return_code == -1073740940 or return_code == 3221226356:  # 0xC0000374
            return "🚨 CRASH TYPE: Heap corruption detected"
    else:
        # Unix-like systems
        if return_code == 139:  # 128 + 11 (SIGSEGV)
            return "🚨 CRASH TYPE: Segmentation fault (SIGSEGV)"
        elif return_code == 136:  # 128 + 8 (SIGFPE)
            return "🚨 CRASH TYPE: Floating point exception (SIGFPE)"
        elif return_code == 134:  # 128 + 6 (SIGABRT)
            return "🚨 CRASH TYPE: Process aborted (SIGABRT) - likely assertion failure"
        elif return_code == 132:  # 128 + 4 (SIGILL)
            return "🚨 CRASH TYPE: Illegal instruction (SIGILL)"
        elif return_code == 137:  # 128 + 9 (SIGKILL)
            return "🚨 CRASH TYPE: Process killed (SIGKILL) - possibly out of memory"
        elif return_code == 130:  # 128 + 2 (SIGINT)
            return "🚨 CRASH TYPE: Process interrupted (SIGINT) - Ctrl+C"
        elif return_code == 127:
            return "🚨 CRASH TYPE: Command not found or missing dependencies"

    # Common non-zero exit codes
    if return_code == 1:
        if not output.strip():
            return "🚨 CRASH TYPE: Silent failure - process exited with code 1 but produced no output (possible crash)"
    elif return_code < 0:
        return f"🚨 CRASH TYPE: Process terminated by signal {abs(return_code)}"
    elif return_code > 128:
        signal_num = return_code - 128
        return f"🚨 CRASH TYPE: Process terminated by signal {signal_num}"

    return None


def _dump_post_mortem_stack_trace(
    test_executable: Path, enable_stack_trace: bool
) -> Optional[str]:
    """
    Attempt to get a post-mortem stack trace by running the test under GDB.
    This is used when a test crashes immediately and timeout-based stack traces won't work.

    Returns:
        Optional[str]: GDB output with stack trace, or None if not enabled/failed
    """
    if not enable_stack_trace:
        return None

    try:
        # Create GDB script for running the test and capturing stack trace on crash
        with tempfile.NamedTemporaryFile(
            mode="w+", delete=False, suffix=".gdb"
        ) as gdb_script:
            gdb_script.write("set pagination off\n")
            gdb_script.write("set confirm off\n")
            gdb_script.write("set logging file gdb_output.txt\n")
            gdb_script.write("set logging on\n")
            gdb_script.write("run --minimal\n")  # Run the test with minimal output
            gdb_script.write("bt full\n")  # Backtrace on crash
            gdb_script.write("info registers\n")  # Register info
            gdb_script.write("x/16i $pc\n")  # Disassembly around PC
            gdb_script.write("thread apply all bt full\n")  # All thread backtraces
            gdb_script.write("quit\n")
            gdb_script_path = gdb_script.name

        # Run the test under GDB
        gdb_command = ["gdb", "-batch", "-x", gdb_script_path, str(test_executable)]

        print(f"Running post-mortem stack trace analysis: {' '.join(gdb_command)}")

        gdb_process = subprocess.Popen(
            gdb_command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            cwd=_PROJECT_ROOT,
        )

        gdb_output, _ = gdb_process.communicate(timeout=60)  # 1 minute timeout for GDB

        # Clean up GDB script
        os.unlink(gdb_script_path)

        if gdb_output and gdb_output.strip():
            return gdb_output
        else:
            return "GDB completed but produced no output"

    except subprocess.TimeoutExpired:
        return "GDB analysis timed out after 60 seconds"
    except FileNotFoundError:
        return "GDB not found - install GDB to enable stack trace analysis"
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        return f"Failed to run post-mortem stack trace analysis: {e}"


def discover_tests(build_dir: Path, specific_test: Optional[str] = None) -> list[Path]:
    """Find test executables in the build directory"""
    # Check test directory
    possible_test_dirs = [
        _PROJECT_ROOT / "tests" / "bin",  # Python build system
    ]

    test_dir = None
    for possible_dir in possible_test_dirs:
        if possible_dir.exists():
            # Check if this directory has actual executable test files
            executable_tests = [
                f
                for pattern in _get_test_patterns()
                for f in possible_dir.glob(pattern)
                if _is_test_executable(f)
            ]
            if executable_tests:
                test_dir = possible_dir
                break

    if not test_dir:
        print(f"Error: No test directory found. Checked: {possible_test_dirs}")
        sys.exit(1)

    test_files: list[Path] = []
    for pattern in _get_test_patterns():
        for test_file in test_dir.glob(pattern):
            if not _is_test_executable(test_file):
                continue
            if specific_test:
                # Support both "test_name" and "name" formats (case-insensitive)
                test_stem = test_file.stem
                test_name = test_stem.replace("test_", "")
                if (
                    test_stem.lower() == specific_test.lower()
                    or test_name.lower() == specific_test.lower()
                ):
                    test_files.append(test_file)
            else:
                test_files.append(test_file)

    return test_files


def parse_args() -> Args:
    """Parse command line arguments and return typed Args dataclass"""
    parser = argparse.ArgumentParser(description="Run FastLED unit tests")
    parser.add_argument("--test", help="Run specific test (without test_ prefix)")
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Show all test output"
    )
    parser.add_argument("--jobs", "-j", type=int, help="Number of parallel jobs")
    parser.add_argument(
        "--stack-trace",
        action="store_true",
        help="Enable GDB stack trace dumps on test crashes/timeouts",
    )

    parsed_args = parser.parse_args()

    return Args(
        test=parsed_args.test,
        verbose=parsed_args.verbose,
        jobs=parsed_args.jobs,
        enable_stack_trace=parsed_args.stack_trace,
    )


def run_test(
    test_file: Path, verbose: bool = False, enable_stack_trace: bool = False
) -> TestResult:
    """Run a single test and capture its output"""
    global _ABORT_EVENT
    if _ABORT_EVENT.is_set():
        return TestResult(
            name=test_file.stem,
            success=False,
            duration=0.0,
            output="Test aborted because _ABORT_EVENT was set.",
            captured_lines=[],
        )
    start_time = time.time()

    # Build command with doctest flags
    # Convert to absolute path for Windows compatibility
    test_executable = test_file.resolve()
    cmd = [str(test_executable)]
    if not verbose:
        cmd.append("--minimal")  # Only show output for failures
    captured_lines: list[str] = []
    try:
        # Set timeout when stack traces are enabled to ensure stack trace functionality works
        timeout = (
            120 if enable_stack_trace else None
        )  # 2 minutes timeout for stack traces

        process = RunningProcess(
            command=cmd,
            cwd=_PROJECT_ROOT,
            check=False,
            shell=False,
            auto_run=True,
            timeout=timeout,
        )

        with process.line_iter(timeout=30) as it:
            if _ABORT_EVENT.is_set():
                return TestResult(
                    name=test_file.stem,
                    success=False,
                    duration=time.time() - start_time,
                    output="Test execution interrupted by user",
                    captured_lines=[],
                    return_code=130,  # SIGINT equivalent
                )
            for line in it:
                captured_lines.append(line)
                # Only print output if verbose is enabled
                if verbose:
                    print(line)

        # Wait for process to complete
        process.wait()
        return_code = process.poll()
        success = return_code == 0
        output = process.stdout

        # Add detailed crash analysis for failed tests
        if not success and return_code is not None:
            crash_info = _analyze_crash_type(return_code, output)
            if crash_info:
                output = f"{output}\n{crash_info}"

            # Attempt post-mortem stack trace if enabled and test crashed
            if enable_stack_trace and return_code != 0:
                print(f"\n{'=' * 80}")
                print("ATTEMPTING POST-MORTEM STACK TRACE ANALYSIS")
                print(f"{'=' * 80}")
                stack_trace = _dump_post_mortem_stack_trace(
                    test_executable, enable_stack_trace
                )
                if stack_trace:
                    print("POST-MORTEM STACK TRACE:")
                    print(f"{'=' * 80}")
                    print(stack_trace)
                    print(f"{'=' * 80}")
                    # Also add to output for the test result
                    output = f"{output}\n\nPOST-MORTEM STACK TRACE:\n{stack_trace}"
    except KeyboardInterrupt:
        import _thread
        import warnings

        _thread.interrupt_main()
        warnings.warn("Test execution interrupted by user")
        _ABORT_EVENT.set()
        raise

    except Exception as e:
        success = False
        output = f"Error running test: {e}"
        return_code = None  # Exception case doesn't have a return code

    duration = time.time() - start_time
    return TestResult(
        name=test_file.stem,
        success=success,
        duration=duration,
        output=output,
        captured_lines=captured_lines,
        return_code=return_code,
    )


def main() -> None:
    try:
        args = parse_args()

        # Find build directory
        build_dir = _PROJECT_ROOT / ".build"
        if not build_dir.exists():
            print("Error: Build directory not found. Run compilation first.")
            sys.exit(1)

        # Discover tests
        test_files = discover_tests(build_dir, args.test)
        if not test_files:
            if args.test:
                print(f"Error: No test found matching '{args.test}'")
            else:
                print("Error: No tests found")
            sys.exit(1)

        print(f"Running {len(test_files)} tests...")
        if args.test:
            print(f"Filter: {args.test}")

        # Determine number of parallel jobs
        if os.environ.get("NO_PARALLEL"):
            max_workers = 1
            print(
                "NO_PARALLEL environment variable set - forcing sequential test execution"
            )
        elif args.jobs:
            max_workers = args.jobs
        else:
            import multiprocessing

            max_workers = max(1, multiprocessing.cpu_count() - 1)

        # Run tests in parallel
        start_time = time.time()
        results: list[TestResult] = []
        failed_tests: list[TestResult] = []

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            future_to_test = {
                executor.submit(
                    run_test, test_file, args.verbose, args.enable_stack_trace
                ): test_file
                for test_file in test_files
            }

            completed = 0
            for future in as_completed(future_to_test):
                test_file = future_to_test[future]
                completed += 1
                try:
                    result = future.result()
                    results.append(result)

                    # Show progress
                    status = "PASS" if result.success else "FAIL"
                    print(
                        f"[{completed}/{len(test_files)}] {status} {result.name} ({result.duration:.2f}s)"
                    )

                    # Show output for failures or in verbose mode
                    if not result.success or args.verbose:
                        if result.output:
                            print(result.output)

                    if not result.success:
                        failed_tests.append(result)

                        # Early abort after 3 failures
                        if len(failed_tests) >= 3:
                            _ABORT_EVENT.set()
                            print(
                                "Reached failure threshold (3). Aborting remaining unit tests."
                            )
                            # Cancel remaining futures and stop accepting new work
                            try:
                                executor.shutdown(wait=False, cancel_futures=True)
                            except TypeError:
                                # Python < 3.9: cancel_futures not available
                                pass
                            break

                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    _ABORT_EVENT.set()
                    print("\nTest execution interrupted by user")
                    try:
                        executor.shutdown(wait=False, cancel_futures=True)
                        time.sleep(1.0)
                    except TypeError:
                        pass
                    break
                except Exception as e:
                    print(f"Error running {test_file.name}: {e}")
                    failed_tests.append(
                        TestResult(
                            name=test_file.stem,
                            success=False,
                            duration=0.0,
                            output=str(e),
                            captured_lines=[],
                            return_code=None,  # Exception case
                        )
                    )
                    if len(failed_tests) >= 3:
                        _ABORT_EVENT.set()
                        print(
                            "Reached failure threshold (3) due to errors. Aborting remaining unit tests."
                        )
                        try:
                            executor.shutdown(wait=False, cancel_futures=True)
                        except TypeError:
                            pass
                        break

        # Show summary
        total_duration = time.time() - start_time
        success_count = len(results) - len(failed_tests)

        print("\nTest Summary:")
        print(f"Total tests: {len(results)}")
        print(f"Passed: {success_count}")
        print(f"Failed: {len(failed_tests)}")
        print(f"Total time: {total_duration:.2f}s")

        if failed_tests:
            print("\nFailed tests:")
            failures: list[TestFailureInfo] = []

            # Show first 3 failures in detail, then just list names for the rest
            for i, test in enumerate(failed_tests):
                print(f"  {test.name}")

                # For first 3 failures, show detailed captured output if available
                if i < 3 and test.captured_lines:
                    if len(test.captured_lines) > 100:
                        print(
                            f"    ... (showing last 100 lines of {len(test.captured_lines)} total)"
                        )
                        for line in test.captured_lines[-100:]:
                            print(f"    {line}")
                    else:
                        for line in test.captured_lines:
                            print(f"    {line}")
                    print()  # Add blank line after each failure

                # Extract actual return code from test result if available
                actual_return_code = (
                    test.return_code if test.return_code is not None else 1
                )

                failures.append(
                    TestFailureInfo(
                        test_name=test.name,
                        command=test.name,
                        return_code=actual_return_code,
                        output=(
                            test.output
                            if i < 3
                            else "Output suppressed (failure limit reached)"
                        ),
                        error_type="test_execution_failure",
                    )
                )

            # If more than 3 failures, show suppression message
            if len(failed_tests) > 3:
                suppressed_count = len(failed_tests) - 3
                print(
                    f"\nSuppressing detailed output for {suppressed_count} additional failure(s)."
                )
                print("Use --verbose to see all failure details.")

            raise TestExecutionFailedException(
                f"{len(failed_tests)} test(s) failed", failures
            )

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\nTest execution interrupted by user")
        sys.exit(1)
    except TestExecutionFailedException as e:
        # Print detailed failure information
        print("\n" + "=" * 60)
        print("FASTLED TEST EXECUTION FAILURE DETAILS")
        print("=" * 60)
        print(e.get_detailed_failure_info())
        print("=" * 60)

        # Exit with appropriate code
        sys.exit(1)


if __name__ == "__main__":
    main()
