#!/usr/bin/env python3
"""
Simple test runner for FastLED unit tests.
Discovers and runs test executables in parallel with clean output handling.
"""

import argparse
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

from ci.util.running_process import RunningProcess
from ci.util.test_exceptions import (
    TestExecutionFailedException,
    TestFailureInfo,
)


_HERE = Path(__file__).parent
_PROJECT_ROOT = _HERE.parent


@dataclass
class TestResult:
    """Result of a test execution"""

    name: str
    success: bool
    duration: float
    output: str


def _is_test_executable(f: Path) -> bool:
    """Check if a file is a valid test executable"""
    return (
        f.is_file() and f.suffix not in [".o", ".obj", ".pdb"] and os.access(f, os.X_OK)
    )


def _get_test_patterns() -> List[str]:
    """Get test patterns based on platform"""
    # On Windows, check both .exe and no extension (Clang generates without .exe)
    return ["test_*.exe"] if sys.platform == "win32" else ["test_*"]


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


def run_test(test_file: Path, verbose: bool = False) -> TestResult:
    """Run a single test and capture its output"""
    start_time = time.time()

    # Build command with doctest flags
    # Convert to absolute path for Windows compatibility
    test_executable = test_file.resolve()
    cmd = [str(test_executable)]
    if not verbose:
        cmd.append("--minimal")  # Only show output for failures

    try:
        process = RunningProcess(
            command=cmd,
            cwd=Path(".").absolute(),
            check=False,
            shell=False,
            auto_run=True,
        )

        with process.line_iter(timeout=30) as it:
            for line in it:
                print(line)

        # Wait for process to complete
        process.wait()
        success = process.poll() == 0
        output = process.stdout

    except Exception as e:
        success = False
        output = f"Error running test: {e}"

    duration = time.time() - start_time
    return TestResult(
        name=test_file.stem, success=success, duration=duration, output=output
    )


def main() -> None:
    try:
        parser = argparse.ArgumentParser(description="Run FastLED unit tests")
        parser.add_argument("--test", help="Run specific test (without test_ prefix)")
        parser.add_argument(
            "--verbose", "-v", action="store_true", help="Show all test output"
        )
        parser.add_argument("--jobs", "-j", type=int, help="Number of parallel jobs")
        args = parser.parse_args()

        # Find build directory
        build_dir = Path.cwd() / ".build"
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

                except Exception as e:
                    print(f"Error running {test_file.name}: {e}")
                    failed_tests.append(
                        TestResult(
                            name=test_file.stem,
                            success=False,
                            duration=0.0,
                            output=str(e),
                        )
                    )
                    if len(failed_tests) >= 3:
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
            for test in failed_tests:
                print(f"  {test.name}")
                failures.append(
                    TestFailureInfo(
                        test_name=test.name,
                        command=f"test_{test.name}",
                        return_code=1,  # Failed tests get return code 1
                        output=test.output,
                        error_type="test_execution_failure",
                    )
                )

            raise TestExecutionFailedException(
                f"{len(failed_tests)} test(s) failed", failures
            )

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
