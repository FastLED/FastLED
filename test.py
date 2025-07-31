#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import queue
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Protocol, cast

from typeguard import typechecked

from ci.ci.running_process import RunningProcess

# Import the new test runner
from ci.ci.test_runner import runner as test_runner
from ci.ci.test_types import (
    FingerprintResult,
    TestArgs,
    TestCategories,
    calculate_fingerprint,
    determine_test_categories,
    fingerprint_code_base,
    process_test_flags,
)


_PIO_CHECK_ENABLED = False

# Default to non-interactive mode for safety unless explicitly in interactive environment
# This prevents hanging on prompts when running in automated environments
_IS_GITHUB = True  # Default to non-interactive behavior
if os.environ.get("FASTLED_INTERACTIVE") == "true":
    _IS_GITHUB = False  # Only enable interactive mode if explicitly requested

# Set environment variable to ensure all subprocesses also run in non-interactive mode
os.environ["FASTLED_CI_NO_INTERACTIVE"] = "true"
if not _IS_GITHUB:
    os.environ.pop(
        "FASTLED_CI_NO_INTERACTIVE", None
    )  # Remove if interactive mode is enabled


def run_command(cmd: List[str], **kwargs: Any) -> None:
    """Run a command and handle errors"""
    try:
        subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


def parse_args() -> TestArgs:
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description="Run FastLED tests")
    parser.add_argument(
        "--cpp",
        action="store_true",
        help="Run C++ tests only (equivalent to --unit --examples, suppresses Python tests)",
    )
    parser.add_argument("--unit", action="store_true", help="Run C++ unit tests only")
    parser.add_argument("--py", action="store_true", help="Run Python tests only")
    parser.add_argument(
        "test", type=str, nargs="?", default=None, help="Specific C++ test to run"
    )

    # Create mutually exclusive group for compiler selection
    compiler_group = parser.add_mutually_exclusive_group()
    compiler_group.add_argument(
        "--clang", action="store_true", help="Use Clang compiler"
    )
    compiler_group.add_argument(
        "--gcc", action="store_true", help="Use GCC compiler (default on non-Windows)"
    )

    parser.add_argument(
        "--clean", action="store_true", help="Clean build before compiling"
    )
    parser.add_argument(
        "--no-interactive",
        action="store_true",
        help="Force non-interactive mode (no confirmation prompts)",
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Enable interactive mode (allows confirmation prompts)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose output showing all test details",
    )
    parser.add_argument(
        "--quick", action="store_true", help="Enable quick mode with FASTLED_ALL_SRC=1"
    )
    parser.add_argument(
        "--no-stack-trace",
        action="store_true",
        help="Disable stack trace dumping on timeout",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Enable static analysis (IWYU, clang-tidy) - auto-enables --cpp and --clang",
    )
    parser.add_argument(
        "--examples",
        nargs="*",
        help="Run example compilation tests only (optionally specify example names). Use with --full for complete compilation + linking + execution",
    )
    parser.add_argument(
        "--no-pch",
        action="store_true",
        help="Disable precompiled headers (PCH) when running example compilation tests",
    )
    parser.add_argument(
        "--cache",
        "--sccache",
        action="store_true",
        help="Enable sccache/ccache when running example compilation tests (disabled by default for faster clean builds)",
    )
    parser.add_argument(
        "--unity",
        action="store_true",
        help="Enable UNITY build mode for examples - compile all source files as a single unit for improved performance",
    )
    parser.add_argument(
        "--full",
        action="store_true",
        help="Run full integration tests including compilation + linking + program execution",
    )
    parser.add_argument(
        "--legacy",
        action="store_true",
        help="Use legacy CMake system instead of new Python API (8x slower)",
    )

    args = parser.parse_args()

    # Convert argparse.Namespace to TestArgs dataclass
    test_args = TestArgs(
        cpp=args.cpp,
        unit=args.unit,
        py=args.py,
        test=args.test,
        clang=args.clang,
        gcc=args.gcc,
        clean=args.clean,
        no_interactive=args.no_interactive,
        interactive=args.interactive,
        verbose=args.verbose,
        quick=args.quick,
        no_stack_trace=args.no_stack_trace,
        check=args.check,
        examples=args.examples,
        no_pch=args.no_pch,
        cache=args.cache,
        unity=args.unity,
        full=args.full,
        legacy=args.legacy,
    )

    # Auto-enable --cpp when a specific test is provided
    if test_args.test and not test_args.cpp:
        test_args.cpp = True
        print(f"Auto-enabled --cpp mode for specific test: {test_args.test}")
        # Also enable --unit when a specific test is provided without any other flags
        if (
            not test_args.unit
            and not test_args.examples
            and not test_args.py
            and not test_args.full
        ):
            test_args.unit = True
            print(f"Auto-enabled --unit mode for specific test: {test_args.test}")

    # Auto-enable --cpp and --clang when --check is provided
    if test_args.check:
        if not test_args.cpp:
            test_args.cpp = True
            print("Auto-enabled --cpp mode for static analysis (--check)")
        if not test_args.clang and not test_args.gcc:
            test_args.clang = True
            print("Auto-enabled --clang compiler for static analysis (--check)")

    # Auto-enable --cpp and --quick when --examples is provided
    if test_args.examples is not None:
        if not test_args.cpp:
            test_args.cpp = True
            print("Auto-enabled --cpp mode for example compilation (--examples)")
        if not test_args.quick:
            test_args.quick = True
            print(
                "Auto-enabled --quick mode for faster example compilation (--examples)"
            )

    # Handle --full flag behavior
    if test_args.full:
        if test_args.examples is not None:
            # --examples --full: Run examples with full compilation+linking+execution
            print("Full examples mode: compilation + linking + program execution")
        else:
            # --full alone: Run integration tests
            if not test_args.cpp:
                test_args.cpp = True
                print("Auto-enabled --cpp mode for full integration tests (--full)")
            print("Full integration tests: compilation + linking + program execution")

    # Default to Clang on Windows unless --gcc is explicitly passed
    if sys.platform == "win32" and not test_args.gcc and not test_args.clang:
        test_args.clang = True
        print("Windows detected: defaulting to Clang compiler (use --gcc to override)")
    elif test_args.gcc:
        print("Using GCC compiler")
    elif test_args.clang:
        print("Using Clang compiler")

    return test_args


from ci.ci.test_types import determine_test_categories, process_test_flags


def _make_pio_check_cmd() -> List[str]:
    return [
        "pio",
        "check",
        "--skip-packages",
        "--src-filters=+<src/>",
        "--severity=medium",
        "--fail-on-defect=high",
        "--flags",
        "--inline-suppr --enable=all --std=c++17",
    ]


def make_compile_uno_test_process(enable_stack_trace: bool = True) -> RunningProcess:
    """Create a process to compile the uno tests"""
    cmd = [
        "uv",
        "run",
        "-m",
        "ci.ci-compile.py",
        "uno",
        "--examples",
        "Blink",
        "--no-interactive",
    ]
    # shell=True wasn't working for some reason
    # cmd = cmd + ['||'] + cmd
    # return RunningProcess(cmd, echo=False, auto_run=not _IS_GITHUB, shell=True)
    return RunningProcess(
        cmd, auto_run=not _IS_GITHUB, enable_stack_trace=enable_stack_trace
    )


from ci.ci.test_types import calculate_fingerprint, fingerprint_code_base


def build_cpp_test_command(args: TestArgs) -> str:
    """Build the C++ test command based on arguments"""
    cmd_list = ["uv", "run", "python", "-m", "ci.compiler.cpp_test_run"]

    if args.clang:
        cmd_list.append("--clang")

    if args.test:
        cmd_list.append("--test")
        cmd_list.append(args.test)
    if args.clean:
        cmd_list.append("--clean")
    if args.verbose:
        cmd_list.append("--verbose")  # Always pass verbose flag when enabled
    if args.check:
        cmd_list.append("--check")
    if args.legacy:
        cmd_list.append("--legacy")

    return subprocess.list2cmdline(cmd_list)


def main() -> None:
    try:
        # Start a watchdog timer to kill the process if it takes too long (10 minutes)
        def watchdog_timer():
            time.sleep(600)  # 10 minutes
            print("Watchdog timer expired after 10 minutes - forcing exit")
            os._exit(2)  # Exit with error code 2 to indicate timeout

        watchdog = threading.Thread(
            target=watchdog_timer, daemon=True, name="WatchdogTimer"
        )
        watchdog.start()

        args = parse_args()
        args = process_test_flags(args)

        # Handle --no-interactive flag
        if args.no_interactive:
            global _IS_GITHUB
            _IS_GITHUB = True
            os.environ["FASTLED_CI_NO_INTERACTIVE"] = "true"
            os.environ["GITHUB_ACTIONS"] = (
                "true"  # This ensures all subprocess also run in non-interactive mode
            )

        # Handle --interactive flag
        if args.interactive:
            _IS_GITHUB = False
            os.environ.pop("FASTLED_CI_NO_INTERACTIVE", None)
            os.environ.pop("GITHUB_ACTIONS", None)

        # Handle --quick flag
        if args.quick:
            os.environ["FASTLED_ALL_SRC"] = "1"
            print("Quick mode enabled. FASTLED_ALL_SRC=1")

        # Handle legacy flag - set environment variable for subprocesses
        if args.legacy:
            os.environ["USE_CMAKE"] = "1"
            print("Legacy mode enabled. Using CMake build system.")
        else:
            # Ensure USE_CMAKE is not set
            os.environ.pop("USE_CMAKE", None)

        # Handle stack trace control
        enable_stack_trace = not args.no_stack_trace
        if enable_stack_trace:
            print("Stack trace dumping enabled for test timeouts")
        else:
            print("Stack trace dumping disabled for test timeouts")

        # Validate conflicting arguments
        if args.no_interactive and args.interactive:
            print(
                "Error: --interactive and --no-interactive cannot be used together",
                file=sys.stderr,
            )
            sys.exit(1)

        # Change to script directory
        os.chdir(Path(__file__).parent)

        cache_dir = Path(".cache")
        cache_dir.mkdir(exist_ok=True)
        fingerprint_file = cache_dir / "fingerprint.json"

        def write_fingerprint(fingerprint: FingerprintResult) -> None:
            # Convert dataclass to dict for JSON serialization
            fingerprint_dict = {
                "hash": fingerprint.hash,
                "elapsed_seconds": fingerprint.elapsed_seconds,
                "status": fingerprint.status,
            }
            with open(fingerprint_file, "w") as f:
                json.dump(fingerprint_dict, f, indent=2)

        def read_fingerprint() -> FingerprintResult | None:
            if fingerprint_file.exists():
                with open(fingerprint_file, "r") as f:
                    try:
                        data = json.load(f)
                        return FingerprintResult(
                            hash=data.get("hash", ""),
                            elapsed_seconds=data.get("elapsed_seconds"),
                            status=data.get("status"),
                        )
                    except json.JSONDecodeError:
                        print("Invalid fingerprint file. Recalculating...")
            return None

        prev_fingerprint = read_fingerprint()
        # Calculate fingerprint
        fingerprint_data = calculate_fingerprint()
        src_code_change: bool
        if prev_fingerprint is None:
            src_code_change = True
        else:
            try:
                src_code_change = fingerprint_data.hash != prev_fingerprint.hash
            except Exception:
                print("Invalid fingerprint file. Recalculating...")
                src_code_change = True
        # print(f"Fingerprint: {fingerprint_result['hash']}")

        # Create .cache directory if it doesn't exist

        # Save the fingerprint to a file as JSON

        write_fingerprint(fingerprint_data)

        # Run tests using the new test runner
        test_runner(args)

        # Force exit daemon thread remains at the end
        def force_exit():
            time.sleep(1)
            print("Force exit daemon thread invoked")
            os._exit(1)

        daemon_thread = threading.Thread(
            target=force_exit, daemon=True, name="ForceExitDaemon"
        )
        daemon_thread.start()
        sys.exit(0)
    except KeyboardInterrupt:
        sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)


if __name__ == "__main__":
    main()
