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

from ci.util.test_args import parse_args
from ci.util.test_commands import run_command
from ci.util.test_env import (
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
from ci.util.watchdog_state import get_active_processes


_CANCEL_WATCHDOG = threading.Event()


def dump_main_thread_stack() -> None:
    """Dump stack trace of the main thread and process tree info"""
    print("\n=== MAIN THREAD STACK TRACE ===")
    for thread in threading.enumerate():
        if thread.name == "MainThread":
            print(f"\nThread {thread.name}:")
            if thread.ident is not None:
                frame = sys._current_frames().get(thread.ident)
                if frame:
                    traceback.print_stack(frame)
    print("=== END STACK TRACE ===\n")

    # Dump process tree information
    print("\n=== PROCESS TREE INFO ===")
    print(get_process_tree_info(os.getpid()))
    print("=== END PROCESS TREE INFO ===\n")


def make_watch_dog_thread(
    seconds: int = 60,
) -> threading.Thread:  # 60 seconds default timeout
    def watchdog_timer() -> None:
        time.sleep(seconds)
        if _CANCEL_WATCHDOG.is_set():
            return

        warnings.warn(f"Watchdog timer expired after {seconds} seconds.")

        # Get current active processes to show which command is stuck
        active_processes = get_active_processes()

        dump_main_thread_stack()
        print(f"Watchdog timer expired after {seconds} seconds - forcing exit")

        if active_processes:
            print(f"\n🚨 STUCK SUBPROCESS COMMANDS:")
            for i, cmd in enumerate(active_processes, 1):
                print(f"  {i}. {cmd}")
        else:
            print("\n🚨 NO ACTIVE SUBPROCESSES DETECTED - MAIN PROCESS LIKELY HUNG")

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

        # Set up watchdog timer
        watchdog = make_watch_dog_thread(seconds=300)

        # Parse and process arguments
        args = parse_args()
        args = process_test_flags(args)

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
            python_process = create_python_test_process(not args.no_stack_trace)

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

            # Wait for Python tests to complete (which will trigger examples compilation)
            python_return_code = python_process.wait()
            if python_return_code != 0:
                sys.exit(python_return_code)

            # Wait for examples compilation to complete
            examples_return_code = examples_process.wait()
            if examples_return_code != 0:
                sys.exit(examples_return_code)

            # Display timing summary for sequential execution
            from ci.util.test_runner import ProcessTiming, _format_timing_summary, _get_friendly_test_name
            timings = []
            if python_process.duration is not None:
                timings.append(ProcessTiming(
                    name=_get_friendly_test_name(python_process.command),
                    duration=python_process.duration,
                    command=str(python_process.command)
                ))
            if examples_process.duration is not None:
                timings.append(ProcessTiming(
                    name=_get_friendly_test_name(examples_process.command),
                    duration=examples_process.duration,
                    command=str(examples_process.command)
                ))
            
            if timings:
                summary = _format_timing_summary(timings)
                print(summary)

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
