#!/usr/bin/env python3
"""
Simple test runner for FastLED unit tests.
Discovers and runs test executables in parallel with clean output handling.
"""

import argparse
import os
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

from ci.util.running_process import RunningProcess
from ci.util.test_exceptions import (
    TestExecutionFailedException,
    TestFailureInfo,
)


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
    captured_lines: List[str] = field(default_factory=list)  # type: ignore
    return_code: Optional[int] = None


@dataclass
class Args:
    """Typed arguments for the test runner"""

    test: Optional[str] = None
    verbose: bool = False
    jobs: Optional[int] = None


def _is_test_executable(f: Path) -> bool:
    """Check if a file is a valid test executable"""
    return (
        f.is_file() and f.suffix not in [".o", ".obj", ".pdb"] and os.access(f, os.X_OK)
    )


def _get_test_patterns() -> List[str]:
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


def discover_tests(build_dir: Path, specific_test: Optional[str] = None) -> List[Path]:
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

    test_files: List[Path] = []
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

    parsed_args = parser.parse_args()

    return Args(
        test=parsed_args.test,
        verbose=parsed_args.verbose,
        jobs=parsed_args.jobs,
    )


def run_test(test_file: Path, verbose: bool = False) -> TestResult:
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
    captured_lines: List[str] = []
    try:
        process = RunningProcess(
            command=cmd,
            cwd=_PROJECT_ROOT,
            check=False,
            shell=False,
            auto_run=True,
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
        results: List[TestResult] = []
        failed_tests: List[TestResult] = []

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            future_to_test = {
                executor.submit(run_test, test_file, args.verbose): test_file
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
                        output=test.output
                        if i < 3
                        else "Output suppressed (failure limit reached)",
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
