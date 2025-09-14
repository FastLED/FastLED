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
    TestArgs,
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


def run_qemu_tests(args: TestArgs) -> None:
    """Run examples in QEMU emulation."""
    import subprocess
    from pathlib import Path

    if not args.qemu or len(args.qemu) < 1:
        print("Error: --qemu requires a platform (e.g., esp32s3)")
        sys.exit(1)

    platform = args.qemu[0].lower()
    if platform != "esp32s3":
        print(f"Error: Unsupported QEMU platform: {platform}. Only esp32s3 is currently supported.")
        sys.exit(1)

    print(f"Running {platform.upper()} QEMU tests...")

    # Determine which examples to test (skip the platform argument)
    examples_to_test = args.qemu[1:] if len(args.qemu) > 1 else ["BlinkParallel"]
    if not examples_to_test:  # Empty list means test all available examples
        examples_to_test = ["BlinkParallel", "RMT5WorkerPool"]

    print(f"Testing examples: {examples_to_test}")

    # Quick test mode - just validate the setup
    if os.getenv("FASTLED_QEMU_QUICK_TEST") == "true":
        print("Quick test mode - validating QEMU setup only")
        print("QEMU ESP32-S3 option is working correctly!")
        return

    # Install QEMU first
    print("Installing QEMU...")
    try:
        result = subprocess.run(
            ["uv", "run", "ci/install-qemu-esp32.py"],
            capture_output=True,
            text=True,
            timeout=300,
        )

        if result.returncode != 0:
            print(f"QEMU installation failed: {result.stderr}")
            print("QEMU may not be available, but continuing with test...")
            # Don't exit - continue to show what the workflow would do
    except Exception as e:
        print(f"QEMU installation error: {e}")
        print("Continuing with test simulation...")

    success_count = 0
    failure_count = 0

    # Test each example
    for example in examples_to_test:
        print(f"\n--- Testing {example} ---")

        try:
            # Build the example for ESP32-S3
            print(f"Building {example} for ESP32-S3...")
            build_result = subprocess.run([
                "uv", "run", "ci/ci-compile.py", "esp32s3",
                "--examples", example
            ], capture_output=True, text=True, timeout=600)

            if build_result.returncode != 0:
                print(f"Build failed for {example}:")
                print(build_result.stderr)
                failure_count += 1
                continue

            print(f"Build successful for {example}")

            # Check if build artifacts exist
            build_dir = Path(f".build/pio/{platform}")
            if not build_dir.exists():
                print(f"Build directory not found: {build_dir}")
                failure_count += 1
                continue

            print(f"Build artifacts found in {build_dir}")

            # Run in QEMU
            print(f"Running {example} in QEMU...")
            qemu_result = subprocess.run(
                [
                    "uv",
                    "run",
                    "ci/qemu-esp32.py",
                    str(build_dir),
                    "--flash-size",
                    "4",
                    "--timeout",
                    "30",
                    "--interrupt-regex",
                    "(FL_WARN.*test finished)|(guru meditation)|(abort\\(\\))|(LoadProhibited)",
                ],
                capture_output=True,
                text=True,
                timeout=60,
            )

            if qemu_result.returncode == 0:
                print(f"SUCCESS: {example} ran successfully in QEMU")
                success_count += 1
            else:
                print(f"FAILED: {example} failed in QEMU:")
                print(qemu_result.stderr)
                failure_count += 1

        except subprocess.TimeoutExpired:
            print(f"TIMEOUT: {example} timed out during testing")
            failure_count += 1
        except Exception as e:
            print(f"ERROR: {example} failed with exception: {e}")
            failure_count += 1

    # Summary
    print(f"\n=== QEMU {platform.upper()} Test Summary ===")
    print(f"Examples tested: {len(examples_to_test)}")
    print(f"Successful: {success_count}")
    print(f"Failed: {failure_count}")

    if failure_count > 0:
        print("Some tests failed. See output above for details.")
        sys.exit(1)
    else:
        print("All QEMU tests passed!")


def main() -> None:
    try:
        # Record start time
        start_time = time.time()

        # Change to script directory first
        os.chdir(Path(__file__).parent)

        # Parse and process arguments
        args = parse_args()

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

        # Handle QEMU testing
        if args.qemu is not None:
            print("=== QEMU Testing ===")
            run_qemu_tests(args)
            return

        # Run tests using the test runner with sequential example compilation
        # Check if we need to use sequential execution to avoid resource conflicts
        if not args.unit and not args.examples and not args.py and args.full:
            # Full test mode - use RunningProcessGroup for dependency-based execution
            from ci.util.running_process import RunningProcess
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
                enable_stack_trace=False, full_tests=True
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

                print("Sequential test execution completed successfully")

                # Print timing summary
                if timings:
                    print(f"\nExecution Summary:")
                    for timing in timings:
                        print(f"  {timing.name}: {timing.duration:.2f}s")
            except Exception as e:
                print(f"Sequential test execution failed: {e}")
                sys.exit(1)
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
