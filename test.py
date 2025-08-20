#!/usr/bin/env python3

import json
import os
import sys
import threading
import time
import traceback
import warnings
from pathlib import Path
from typing import Optional

import psutil

from ci.util.running_process_manager import RunningProcessManagerSingleton
from ci.util.test_args import parse_args
from ci.util.test_commands import run_command
from ci.util.test_env import (
    dump_thread_stacks,
    get_process_tree_info,
    setup_environment,
    setup_force_exit,
    setup_watchdog,
)
from ci.util.test_runner import runner as test_runner
from ci.util.test_types import (
    FingerprintResult,
    calculate_fingerprint,
    process_test_flags,
)


_CANCEL_WATCHDOG = threading.Event()

_TIMEOUT_EVERYTHING = 600

if os.environ.get("_GITHUB"):
    _TIMEOUT_EVERYTHING = 1200  # Extended timeout for GitHub Linux builds
    print(
        f"GitHub Windows environment detected - using extended timeout: {_TIMEOUT_EVERYTHING} seconds"
    )


def make_watch_dog_thread(
    seconds: int,
) -> threading.Thread:  # 60 seconds default timeout
    def watchdog_timer() -> None:
        time.sleep(seconds)
        if _CANCEL_WATCHDOG.is_set():
            return

        warnings.warn(f"Watchdog timer expired after {seconds} seconds.")

        dump_thread_stacks()
        print(f"Watchdog timer expired after {seconds} seconds - forcing exit")

        # Dump outstanding running processes (if any)
        try:
            RunningProcessManagerSingleton.dump_active()
        except Exception as e:
            print(f"Failed to dump active processes: {e}")

        traceback.print_stack()
        time.sleep(0.5)

        os._exit(2)  # Exit with error code 2 to indicate timeout (SIGTERM)

    thr = threading.Thread(target=watchdog_timer, daemon=True, name="WatchdogTimer")
    thr.start()
    return thr


def main() -> None:
    try:
        # Record start time
        start_time = time.time()

        # Change to script directory first
        os.chdir(Path(__file__).parent)

        # Parse and process arguments
        args = parse_args()

        # Force sequential execution to avoid out of memory errors
        args.no_parallel = True

        args = process_test_flags(args)

        timeout = _TIMEOUT_EVERYTHING
        # Adjust watchdog timeout based on test configuration
        # Sequential examples compilation can take up to 30 minutes
        if args.examples is not None and args.no_parallel:
            # 35 minutes for sequential examples compilation
            timeout = 2100
            print(
                f"Adjusted watchdog timeout for sequential examples compilation: {timeout} seconds"
            )

        # Set up watchdog timer
        watchdog = make_watch_dog_thread(seconds=timeout)

        # Handle --no-interactive flag
        if args.no_interactive:
            os.environ["FASTLED_CI_NO_INTERACTIVE"] = "true"
            os.environ["GITHUB_ACTIONS"] = (
                "true"  # This ensures all subprocess also run in non-interactive mode
            )

        # Handle --interactive flag
        if args.interactive:
            os.environ.pop("FASTLED_CI_NO_INTERACTIVE", None)
            os.environ.pop("GITHUB_ACTIONS", None)

        # Set up remaining environment based on arguments
        setup_environment(args)

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

        # Set up fingerprint caching
        cache_dir = Path(".cache")
        cache_dir.mkdir(exist_ok=True)
        fingerprint_file = cache_dir / "fingerprint.json"

        def write_fingerprint(fingerprint: FingerprintResult) -> None:
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

        # Calculate and save fingerprint
        prev_fingerprint = read_fingerprint()
        fingerprint_data = calculate_fingerprint()
        src_code_change = (
            True
            if prev_fingerprint is None
            else fingerprint_data.hash != prev_fingerprint.hash
        )
        write_fingerprint(fingerprint_data)

        # Run tests using the test runner with sequential example compilation
        # Check if we need to use sequential execution to avoid resource conflicts
        if not args.unit and not args.examples and not args.py and args.full:
            # Full test mode - use sequential execution for example compilation
            from ci.util.running_process import RunningProcess
            from ci.util.test_runner import (
                create_examples_test_process,
                create_python_test_process,
            )

            # Create Python test process (runs first)
            python_process = create_python_test_process(
                enable_stack_trace=False, full_tests=True
            )

            # Create examples compilation process with auto_run=False
            examples_process = create_examples_test_process(
                args, not args.no_stack_trace
            )
            examples_process.auto_run = False

            # Set up callback to start examples compilation after Python tests complete
            def start_examples_compilation():
                print("Python tests completed - starting examples compilation...")
                examples_process.run()

            python_process.on_complete = start_examples_compilation

            # Start the Python test process (since auto_run=False)
            python_process.run()

            if args.verbose:
                with python_process.line_iter(timeout=None) as line_iter:
                    for line in line_iter:
                        print(line)

            # Wait for Python tests to complete (which will trigger examples compilation)
            python_return_code = python_process.wait()
            if python_return_code != 0:
                python_stdout = python_process.stdout
                print(f"Python test process stdout: {python_stdout}")
                sys.exit(python_return_code)

            if args.verbose:
                with examples_process.line_iter(timeout=None) as line_iter:
                    for line in line_iter:
                        print(line)

            # Wait for examples compilation to complete
            examples_return_code = examples_process.wait()
            if examples_return_code != 0:
                sys.exit(examples_return_code)

            print("Sequential test execution completed successfully")
        else:
            # Use normal test runner for other cases
            test_runner(args, src_code_change)

        # Set up force exit daemon and exit
        daemon_thread = setup_force_exit()
        _CANCEL_WATCHDOG.set()

        # Print total execution time
        elapsed_time = time.time() - start_time
        print(f"\nTotal execution time: {elapsed_time:.2f} seconds")

        sys.exit(0)

    except KeyboardInterrupt:
        sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)


if __name__ == "__main__":
    main()
