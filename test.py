#!/usr/bin/env python3

import _thread
import json
import os
import sys
import threading
import time
import traceback
import warnings
from pathlib import Path

from ci.runners.avr8js_runner import run_avr8js_tests
from ci.runners.qemu_runner import run_qemu_tests
from ci.util.fingerprint import FingerprintManager
from ci.util.global_interrupt_handler import (
    signal_interrupt,
    wait_for_cleanup,
)
from ci.util.output_formatter import TimestampFormatter
from ci.util.running_process_manager import RunningProcessManagerSingleton
from ci.util.sccache_config import show_sccache_stats
from ci.util.test_args import parse_args
from ci.util.test_env import (
    dump_thread_stacks,
    setup_environment,
    setup_force_exit,
)
from ci.util.test_runner import runner as test_runner
from ci.util.test_types import (
    process_test_flags,
)
from ci.util.timestamp_print import ts_print


_CANCEL_WATCHDOG = threading.Event()

_TIMEOUT_EVERYTHING = 600


# Platform to emulator backend mapping for --run command
def _load_backends():
    backend_path = Path(__file__).parent / "ci" / "runners" / "backends.json"
    with open(backend_path) as f:
        return json.load(f)


_RUN_PLATFORM_BACKENDS = _load_backends()


if os.environ.get("_GITHUB"):
    _TIMEOUT_EVERYTHING = 1200  # Extended timeout for GitHub Linux builds
    ts_print(
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
        ts_print(f"Watchdog timer expired after {seconds} seconds - forcing exit")

        # Dump outstanding running processes (if any)
        try:
            RunningProcessManagerSingleton.dump_active()
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            ts_print(f"Failed to dump active processes: {e}")

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

        # Handle --no-unity flag
        if args.no_unity:
            ts_print("(--no-unity is assumed by default now)")

        # Default to parallel execution for better performance
        # Users can disable parallel compilation by setting NO_PARALLEL=1 or using --no-parallel
        if os.environ.get("NO_PARALLEL", "0") == "1":
            args.no_parallel = True

        args = process_test_flags(args)

        timeout = _TIMEOUT_EVERYTHING
        # Adjust watchdog timeout based on test configuration
        # Sequential examples compilation can take up to 30 minutes
        if args.examples is not None and args.no_parallel:
            # 35 minutes for sequential examples compilation
            timeout = 2100
            ts_print(
                f"Adjusted watchdog timeout for sequential examples compilation: {timeout} seconds"
            )

        # Set up watchdog timer
        _ = make_watch_dog_thread(seconds=timeout)

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
        # Stack trace messages are only useful when debugging timeouts
        # Don't clutter normal output with this implementation detail

        # Validate conflicting arguments
        if args.no_interactive and args.interactive:
            ts_print(
                "Error: --interactive and --no-interactive cannot be used together",
                file=sys.stderr,
            )
            sys.exit(1)

        # Set up fingerprint caching
        cache_dir = Path(".cache")
        # Determine build mode (default to "quick")
        # IMPORTANT: If --debug is set, use "debug" mode even if --build-mode is not specified
        # This ensures fingerprint caching correctly separates debug builds from quick builds
        build_mode = (
            args.build_mode if args.build_mode else ("debug" if args.debug else "quick")
        )
        fingerprint_manager = FingerprintManager(cache_dir, build_mode=build_mode)

        # Calculate fingerprints
        src_code_change = fingerprint_manager.check_all()
        cpp_test_change = fingerprint_manager.check_cpp(args)
        examples_change = fingerprint_manager.check_examples(args)
        python_test_change = fingerprint_manager.check_python()

        # Handle --run flag (unified emulation interface)
        if args.run is not None:
            if len(args.run) < 1:
                ts_print("Error: --run requires a platform/board (e.g., esp32s3, uno)")
                sys.exit(1)

            platform = args.run[0].lower()

            # Look up backend from mapping table
            backend = None
            for b, platforms in _RUN_PLATFORM_BACKENDS.items():
                if platform in platforms:
                    backend = b
                    break

            if backend is None:
                # Platform not found - show error with supported platforms
                ts_print(f"Error: Unknown platform '{platform}'")
                ts_print()
                ts_print("Supported platforms:")

                # Group platforms by backend
                for b, platforms in _RUN_PLATFORM_BACKENDS.items():
                    ts_print(f"  {b.upper()}: {', '.join(sorted(platforms))}")

                sys.exit(1)

            # Route to appropriate backend
            if backend == "qemu":
                ts_print(f"=== QEMU Testing ({platform}) ===")
                # Convert --run to --qemu format for backward compatibility
                args.qemu = args.run
                run_qemu_tests(args)
                return
            elif backend == "avr8js":
                ts_print(f"=== avr8js Testing ({platform}) ===")
                # Run avr8js tests
                run_avr8js_tests(args)
                return
            else:
                ts_print(
                    f"Error: Unknown backend '{backend}' for platform '{platform}'"
                )
                sys.exit(1)

        # Handle QEMU testing (deprecated - use --run)
        if args.qemu is not None:
            ts_print("=== QEMU Testing ===")
            ts_print("Note: --qemu is deprecated, use --run instead")
            run_qemu_tests(args)
            return

        # Track test success/failure for fingerprint status
        tests_passed = False

        try:
            # Run tests using the test runner with sequential example compilation
            # Check if we need to use sequential execution to avoid resource conflicts
            if not args.unit and not args.examples and not args.py and args.full:
                # Full test mode - use RunningProcessGroup for dependency-based execution

                from ci.util.running_process_group import (
                    ExecutionMode,
                    ProcessExecutionConfig,
                    RunningProcessGroup,
                )
                from ci.util.test_runner import (
                    create_examples_test_process,
                    create_python_test_process,
                )

                # Create Python test process (runs first)
                python_process = create_python_test_process(
                    enable_stack_trace=False, run_slow=True
                )
                python_process.auto_run = False

                # Create examples compilation process
                examples_process = create_examples_test_process(
                    args, not args.no_stack_trace
                )
                examples_process.auto_run = False

                # Configure sequential execution with dependencies
                config = ProcessExecutionConfig(
                    execution_mode=ExecutionMode.SEQUENTIAL_WITH_DEPENDENCIES,
                    verbose=args.verbose,
                    timeout_seconds=2100,  # 35 minutes for sequential examples compilation
                    live_updates=True,  # Enable real-time display
                    display_type="auto",  # Auto-detect best display format
                )

                # Create process group and set up dependency
                group = RunningProcessGroup(config=config, name="FullTestSequence")
                group.add_process(python_process)
                group.add_dependency(
                    examples_process, python_process
                )  # examples depends on python

                try:
                    # Start real-time display for full test mode
                    display_thread = None
                    if not args.verbose and config.live_updates:
                        try:
                            from ci.util.process_status_display import (
                                display_process_status,
                            )

                            display_thread = display_process_status(
                                group,
                                display_type=config.display_type,
                                update_interval=config.update_interval,
                            )
                        except ImportError:
                            pass  # Fall back to normal execution

                    timings = group.run()

                    # Stop display thread if it was started
                    if display_thread:
                        time.sleep(0.5)

                    ts_print("Sequential test execution completed successfully")

                    # Print timing summary
                    if timings:
                        ts_print("\nExecution Summary:")
                        for timing in timings:
                            ts_print(f"  {timing.name}: {timing.duration:.2f}s")
                except KeyboardInterrupt:
                    _thread.interrupt_main()
                    raise
                except Exception as e:
                    ts_print(f"Sequential test execution failed: {e}")
                    sys.exit(1)
            else:
                # Use normal test runner for other cases
                # Force change flags=True when running a specific test to disable fingerprint cache
                # Also force when --no-fingerprint or --force is used
                force_cpp_test_change = (
                    cpp_test_change
                    or (args.test is not None)
                    or args.no_fingerprint
                    or args.force
                )
                force_examples_change = (
                    examples_change
                    or (args.examples is not None)
                    or args.no_fingerprint
                    or args.force
                )
                force_python_test_change = (
                    python_test_change
                    or (args.test is not None)
                    or args.no_fingerprint
                    or args.force
                )
                force_src_code_change = (
                    src_code_change or args.no_fingerprint or args.force
                )

                # Only show cache status when it's enabled (the notable case)
                # When disabled (--no-fingerprint), this is the default so no message needed

                test_runner(
                    args,
                    force_src_code_change,
                    force_cpp_test_change,
                    force_examples_change,
                    force_python_test_change,
                )

            # If we got here, tests passed
            tests_passed = True

        except SystemExit as e:
            # Test runner calls sys.exit() on failure
            if e.code != 0:
                tests_passed = False
            raise
        finally:
            # Only save fingerprints when running ALL tests, not specific tests
            # This prevents running a specific test from marking the full fingerprint as valid
            # Note: args.examples == [] means "all examples" (e.g., --cpp mode), which is OK
            # args.examples with specific items (e.g., ['Blink']) means specific examples only
            running_specific_examples = (
                args.examples is not None and len(args.examples) > 0
            )
            running_all_tests = (
                args.test is None
                and not running_specific_examples
                and not args.no_fingerprint
            )
            if running_all_tests:
                status = "success" if tests_passed else "failure"
                fingerprint_manager.save_all(status)

        # Set up force exit daemon and exit
        _ = setup_force_exit()
        _CANCEL_WATCHDOG.set()

        # Print total execution time
        elapsed_time = time.time() - start_time
        print(f"Total: {elapsed_time:.2f}s")

        sys.exit(0)

    except KeyboardInterrupt:
        # Only notify main thread if we're in a worker thread
        if threading.current_thread() != threading.main_thread():
            from ci.util.global_interrupt_handler import (
                handle_keyboard_interrupt_properly,
            )

            handle_keyboard_interrupt_properly()
        signal_interrupt()
        wait_for_cleanup()
        sys.exit(130)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        # Only notify main thread if we're in a worker thread
        if threading.current_thread() != threading.main_thread():
            from ci.util.global_interrupt_handler import (
                handle_keyboard_interrupt_properly,
            )

            handle_keyboard_interrupt_properly()
        signal_interrupt()
        wait_for_cleanup()
        sys.exit(130)
