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


@dataclass
class TestResult:
    """Result of a test execution"""

    name: str
    success: bool
    duration: float
    output: str


def discover_tests(build_dir: Path, specific_test: Optional[str] = None) -> List[Path]:
    """Find test executables in the build directory"""
    test_dir = build_dir / "fled" / "unit"
    if not test_dir.exists():
        print(f"Error: Test directory not found: {test_dir}")
        sys.exit(1)

    test_files: List[Path] = []
    # Use appropriate extension based on platform
    test_pattern = "test_*.exe" if sys.platform == "win32" else "test_*"
    for test_file in test_dir.glob(test_pattern):
        if not test_file.is_file():
            continue
        if not os.access(test_file, os.X_OK):
            continue
        # Skip object files and PDB files
        if test_file.suffix in [".o", ".obj", ".pdb"]:
            continue
        if specific_test:
            # Support both "test_name" and "name" formats
            test_stem = test_file.stem
            test_name = test_stem.replace("test_", "")
            if test_stem == specific_test or test_name == specific_test:
                test_files.append(test_file)
        else:
            test_files.append(test_file)

    return test_files


def run_test(test_file: Path, verbose: bool = False) -> TestResult:
    """Run a single test and capture its output"""
    start_time = time.time()

    # Build command with doctest flags
    cmd = [str(test_file)]
    if not verbose:
        cmd.append("--minimal")  # Only show output for failures

    try:
        # Start process with pipe for stdout/stderr
        # Get current environment and ensure PATH includes Windows system directories
        env = os.environ.copy()
        if sys.platform == "win32":
            # Add Windows system directories to PATH if not already present
            system32 = os.path.join(
                os.environ.get("SystemRoot", r"C:\Windows"), "System32"
            )
            if system32 not in env.get("PATH", ""):
                env["PATH"] = system32 + os.pathsep + env.get("PATH", "")

        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Redirect stderr to stdout
            bufsize=1,  # Line buffered
            universal_newlines=True,  # Text mode
            encoding="utf-8",
            errors="replace",
            env=env,
            shell=True if sys.platform == "win32" else False,  # Use shell on Windows
        )

        # Read output line by line
        output_lines: List[str] = []
        try:
            while True:
                if process.stdout is None:
                    break

                line = process.stdout.readline()
                if not line and process.poll() is not None:
                    break

                output_lines.append(line)
                if verbose:
                    # Show output in real-time if verbose mode
                    sys.stdout.write(line)
                    sys.stdout.flush()

        finally:
            # Always close stdout
            if process.stdout:
                process.stdout.close()

        # Wait for process to complete
        process.wait()
        success = process.returncode == 0
        output = "".join(output_lines)

    except Exception as e:
        success = False
        output = f"Error running test: {e}"

    duration = time.time() - start_time
    return TestResult(
        name=test_file.stem, success=success, duration=duration, output=output
    )


def main() -> None:
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
    if args.jobs:
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

            except Exception as e:
                print(f"Error running {test_file.name}: {e}")
                failed_tests.append(
                    TestResult(
                        name=test_file.stem, success=False, duration=0.0, output=str(e)
                    )
                )

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
        for test in failed_tests:
            print(f"  {test.name}")
        sys.exit(1)


if __name__ == "__main__":
    main()
