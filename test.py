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
from typing import Optional

import psutil

from ci.util.global_interrupt_handler import (
    is_interrupted,
    notify_main_thread,
    signal_interrupt,
    wait_for_cleanup,
)
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
    calculate_cpp_test_fingerprint,
    calculate_examples_fingerprint,
    calculate_fingerprint,
    calculate_python_test_fingerprint,
    process_test_flags,
)


_CANCEL_WATCHDOG = threading.Event()

_TIMEOUT_EVERYTHING = 600

# Platform to emulator backend mapping for --run command
_RUN_PLATFORM_BACKENDS = {
    # ESP32 platforms use QEMU
    "esp32dev": "qemu",
    "esp32c3": "qemu",
    "esp32s3": "qemu",
    # AVR boards use avr8js
    "uno": "avr8js",
    "attiny85": "avr8js",
    "attiny88": "avr8js",
    "nano_every": "avr8js",
}

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
    """Run examples in QEMU emulation using Docker."""
    from pathlib import Path

    from running_process import RunningProcess

    from ci.docker.qemu_esp32_docker import DockerQEMURunner

    if not args.qemu or len(args.qemu) < 1:
        print("Error: --qemu requires a platform (e.g., esp32s3)")
        sys.exit(1)

    platform = args.qemu[0].lower()
    supported_platforms = ["esp32dev", "esp32c3", "esp32s3"]
    if platform not in supported_platforms:
        print(
            f"Error: Unsupported QEMU platform: {platform}. Supported platforms: {', '.join(supported_platforms)}"
        )
        sys.exit(1)

    print(f"Running {platform.upper()} QEMU tests using Docker...")

    # Determine which examples to test (skip the platform argument)
    examples_to_test = args.qemu[1:] if len(args.qemu) > 1 else ["BlinkParallel"]
    if not examples_to_test:  # Empty list means test all available examples
        examples_to_test = ["BlinkParallel", "RMT5WorkerPool"]

    print(f"Testing examples: {examples_to_test}")

    # Quick test mode - just validate the setup
    if os.getenv("FASTLED_QEMU_QUICK_TEST") == "true":
        print("Quick test mode - validating Docker QEMU setup only")
        print("QEMU ESP32 Docker option is working correctly!")
        return

    # Initialize Docker QEMU runner
    docker_runner = DockerQEMURunner()

    # Check if Docker is available
    if not docker_runner.check_docker_available():
        print("❌ ERROR: Docker is not available or not running")
        print()
        print("Please install Docker Desktop and ensure it's running:")
        print("  https://www.docker.com/products/docker-desktop")
        print()
        sys.exit(1)

    # Check if board Docker image exists (needed for compilation)
    board_image = docker_runner.get_board_image_name(platform)
    image_exists = docker_runner.check_image_exists(board_image)

    if not image_exists:
        print(f"❌ Docker image for {platform} not found locally")
        print()

        # Try pulling from Docker Hub registry first
        print(f"Attempting to pull prebuilt image from Docker Hub...")
        pull_success = docker_runner.pull_and_tag_board_image(platform)

        if pull_success:
            print(f"✅ Successfully pulled and configured Docker image")
            print()
            image_exists = True
        else:
            # Pull failed - show build options
            print()
            print(f"❌ Could not pull image from Docker Hub")
            print()

            # Check if we should auto-build
            if args.build:
                # Auto-build mode (explicit flag)
                print("Auto-build enabled (--build flag)")
                print()
                build_result = docker_runner.build_board_image(platform, progress=True)
                if build_result != 0:
                    print(f"❌ Failed to build Docker image for {platform}")
                    sys.exit(1)
            elif args.interactive:
                # Interactive prompt mode
                response = (
                    input(f"Docker image missing. Build now? [y/N]: ").strip().lower()
                )
                if response in ["y", "yes"]:
                    print()
                    build_result = docker_runner.build_board_image(
                        platform, progress=True
                    )
                    if build_result != 0:
                        print(f"❌ Failed to build Docker image for {platform}")
                        sys.exit(1)
                else:
                    print("Build cancelled. Please build the image manually.")
                    sys.exit(1)
            else:
                # Default mode - automatically build with warning and delay
                print("⚠️  Docker image not available. Will build automatically.")
                print("   This will take approximately 30 minutes.")
                print("   Press Ctrl+C within 5 seconds to cancel...")
                print()

                import time

                for i in range(5, 0, -1):
                    print(f"   Starting build in {i}...", end="\r")
                    time.sleep(1)
                print()  # New line after countdown
                print()

                print("🔨 Building Docker image automatically...")
                print()
                build_result = docker_runner.build_board_image(platform, progress=True)
                if build_result != 0:
                    print(f"❌ Failed to build Docker image for {platform}")
                    print()
                    print("Options:")
                    print(f"  1. Try again with verbose logging:")
                    print(
                        f"     bash test --qemu {platform} {' '.join(examples_to_test)} --build"
                    )
                    print()
                    print(f"  2. Build image separately:")
                    print(f"     bash compile --docker --build {platform} Blink")
                    print()
                    sys.exit(1)

    success_count = 0
    failure_count = 0

    # Test each example
    for example in examples_to_test:
        print(f"\n--- Testing {example} ---")

        try:
            # Build the example for the specified platform with merged binary for QEMU
            print(f"Building {example} for {platform} with merged binary...")
            # Use Docker compilation on Windows to avoid toolchain issues
            build_cmd = [
                "uv",
                "run",
                "ci/ci-compile.py",
                platform,
                "--examples",
                example,
                "--merged-bin",
                "-o",
                "qemu-build/merged.bin",
                "--defines",
                "FASTLED_ESP32_IS_QEMU",
            ]
            if sys.platform == "win32":
                build_cmd.append("--docker")
            build_proc = RunningProcess(
                build_cmd,
                timeout=600,
                auto_run=True,
            )

            # Stream build output
            with build_proc.line_iter(timeout=None) as it:
                for line in it:
                    print(line)

            build_returncode = build_proc.wait()
            if build_returncode != 0:
                print(f"Build failed for {example} with exit code: {build_returncode}")
                failure_count += 1
                continue

            print(f"Build successful for {example}")

            # Check if merged binary exists
            merged_bin_path = Path("qemu-build/merged.bin")
            if not merged_bin_path.exists():
                print(f"Merged binary not found: {merged_bin_path}")
                failure_count += 1
                continue

            print(f"Merged binary found: {merged_bin_path}")

            # Run in QEMU using Docker
            print(f"Running {example} in Docker QEMU...")

            # Set up interrupt regex pattern
            interrupt_regex = "(FL_WARN.*test finished)|(Setup complete - starting blink animation)|(guru meditation)|(abort\\(\\))|(LoadProhibited)"

            # Set up output file for GitHub Actions
            output_file = "qemu_output.log"

            # Determine machine type based on platform
            if platform == "esp32c3":
                machine_type = "esp32c3"
            elif platform == "esp32s3":
                machine_type = "esp32s3"
            else:
                machine_type = "esp32"

            # Run QEMU in Docker with merged binary
            qemu_returncode = docker_runner.run(
                firmware_path=merged_bin_path,
                timeout=30,
                flash_size=4,
                interrupt_regex=interrupt_regex,
                interactive=False,
                output_file=output_file,
                machine=machine_type,
            )

            if qemu_returncode == 0:
                print(f"SUCCESS: {example} ran successfully in Docker QEMU")
                success_count += 1
            else:
                print(
                    f"FAILED: {example} failed in Docker QEMU with exit code: {qemu_returncode}"
                )
                failure_count += 1

        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
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


def run_avr8js_tests(args: TestArgs) -> None:
    """Run AVR examples in avr8js emulation using Docker."""
    from pathlib import Path

    from running_process import RunningProcess

    from ci.docker.avr8js_docker import DockerAVR8jsRunner

    if not args.run or len(args.run) < 1:
        print("Error: --run requires a board (e.g., uno)")
        sys.exit(1)

    board = args.run[0].lower()
    supported_boards = ["uno", "attiny85", "attiny88", "nano_every"]
    if board not in supported_boards:
        print(
            f"Error: Unsupported avr8js board: {board}. Supported boards: {', '.join(supported_boards)}"
        )
        sys.exit(1)

    print(f"Running {board.upper()} avr8js tests using Docker...")

    # Determine which examples to test (skip the board argument)
    examples_to_test = args.run[1:] if len(args.run) > 1 else ["Test"]
    if not examples_to_test:  # Empty list means test Test example
        examples_to_test = ["Test"]

    print(f"Testing examples: {examples_to_test}")

    # Quick test mode - just validate the setup
    if os.getenv("FASTLED_AVR8JS_QUICK_TEST") == "true":
        print("Quick test mode - validating Docker avr8js setup only")
        print("avr8js AVR Docker option is working correctly!")
        return

    # Initialize Docker avr8js runner
    docker_runner = DockerAVR8jsRunner()

    # Check if Docker is available
    if not docker_runner.check_docker_available():
        print("❌ ERROR: Docker is not available or not running")
        print()
        print("Please install Docker Desktop and ensure it's running:")
        print("  https://www.docker.com/products/docker-desktop")
        print()
        sys.exit(1)

    # Check if avr8js Docker image exists
    avr8js_image = docker_runner.docker_image
    image_exists = docker_runner.check_image_exists(avr8js_image)

    if not image_exists:
        print(f"❌ Docker avr8js image not found locally: {avr8js_image}")
        print()
        print("Attempting to pull avr8js image from Docker Hub...")
        try:
            docker_runner.pull_image()
            print(f"✅ Successfully pulled {avr8js_image}")
            print()
        except KeyboardInterrupt:
            raise
        except Exception as e:
            print(f"❌ Failed to pull avr8js image: {e}")
            print()
            print("Please build the image manually:")
            print(f"  docker build -f ci/docker/Dockerfile.avr8js -t {avr8js_image} .")
            print()
            sys.exit(1)

    # Get MCU type and frequency based on board
    mcu_config = {
        "uno": {"mcu": "atmega328p", "frequency": 16000000},
        "attiny85": {"mcu": "attiny85", "frequency": 8000000},
        "attiny88": {"mcu": "attiny88", "frequency": 8000000},
        "nano_every": {"mcu": "atmega4809", "frequency": 16000000},
    }
    config = mcu_config.get(board, {"mcu": "atmega328p", "frequency": 16000000})

    success_count = 0
    failure_count = 0

    # Test each example
    for example in examples_to_test:
        print(f"\n--- Testing {example} ---")

        try:
            # Build the example for the specified board
            print(f"Building {example} for {board}...")
            build_cmd = [
                "uv",
                "run",
                "ci/ci-compile.py",
                board,
                "--examples",
                example,
            ]
            build_proc = RunningProcess(
                build_cmd,
                timeout=300,
                auto_run=True,
            )

            # Stream build output
            with build_proc.line_iter(timeout=None) as it:
                for line in it:
                    print(line)

            build_returncode = build_proc.wait()
            if build_returncode != 0:
                print(f"Build failed for {example} with exit code: {build_returncode}")
                failure_count += 1
                continue

            print(f"Build successful for {example}")

            # Find the compiled .elf file
            build_dir = Path(".build")
            elf_files = list(build_dir.rglob("*.elf"))
            if not elf_files:
                print(f"ELF file not found in {build_dir}")
                failure_count += 1
                continue

            elf_path = elf_files[0]
            print(f"Found ELF file: {elf_path}")

            # Run in avr8js using Docker
            print(f"Running {example} in Docker avr8js...")

            # Set up output file
            output_file = "avr8js_output.txt"

            # Run avr8js in Docker
            avr8js_returncode = docker_runner.run(
                elf_path=elf_path,
                mcu=config["mcu"],
                frequency=config["frequency"],
                timeout=15,
                output_file=output_file,
            )

            # Validate output for Test example
            if example == "Test" and Path(output_file).exists():
                output_content = Path(output_file).read_text(
                    encoding="utf-8", errors="replace"
                )

                # Check for test completion
                if (
                    "Test Summary:" in output_content
                    and "All tests PASSED!" in output_content
                ):
                    print(f"SUCCESS: {example} tests passed in Docker avr8js")
                    success_count += 1
                else:
                    print(f"FAILED: {example} tests did not pass in Docker avr8js")
                    failure_count += 1
            elif avr8js_returncode == 0:
                print(f"SUCCESS: {example} ran successfully in Docker avr8js")
                success_count += 1
            else:
                print(
                    f"FAILED: {example} failed in Docker avr8js with exit code: {avr8js_returncode}"
                )
                failure_count += 1

        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception as e:
            print(f"ERROR: {example} failed with exception: {e}")
            failure_count += 1

    # Summary
    print(f"\n=== avr8js {board.upper()} Test Summary ===")
    print(f"Examples tested: {len(examples_to_test)}")
    print(f"Successful: {success_count}")
    print(f"Failed: {failure_count}")

    if failure_count > 0:
        print("Some tests failed. See output above for details.")
        sys.exit(1)
    else:
        print("All avr8js tests passed!")


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

        # Calculate fingerprint (but don't save until tests pass)
        prev_fingerprint = read_fingerprint()
        fingerprint_data = calculate_fingerprint()
        src_code_change = (
            True
            if prev_fingerprint is None
            else fingerprint_data.hash != prev_fingerprint.hash
        )

        # Set up C++ test-specific fingerprint caching
        cpp_test_fingerprint_file = cache_dir / "cpp_test_fingerprint.json"

        def write_cpp_test_fingerprint(fingerprint: FingerprintResult) -> None:
            fingerprint_dict = {
                "hash": fingerprint.hash,
                "elapsed_seconds": fingerprint.elapsed_seconds,
                "status": fingerprint.status,
            }
            with open(cpp_test_fingerprint_file, "w") as f:
                json.dump(fingerprint_dict, f, indent=2)

        def read_cpp_test_fingerprint() -> FingerprintResult | None:
            if cpp_test_fingerprint_file.exists():
                with open(cpp_test_fingerprint_file, "r") as f:
                    try:
                        data = json.load(f)
                        return FingerprintResult(
                            hash=data.get("hash", ""),
                            elapsed_seconds=data.get("elapsed_seconds"),
                            status=data.get("status"),
                        )
                    except json.JSONDecodeError:
                        print("Invalid C++ test fingerprint file. Recalculating...")
            return None

        # Calculate C++ test fingerprint (but don't save until tests pass)
        prev_cpp_test_fingerprint = read_cpp_test_fingerprint()
        cpp_test_fingerprint_data = calculate_cpp_test_fingerprint(args)
        cpp_test_change = (
            True
            if prev_cpp_test_fingerprint is None
            else not prev_cpp_test_fingerprint.should_skip(cpp_test_fingerprint_data)
        )

        # Set up examples test fingerprint caching
        examples_fingerprint_file = cache_dir / "examples_fingerprint.json"

        def write_examples_fingerprint(fingerprint: FingerprintResult) -> None:
            fingerprint_dict = {
                "hash": fingerprint.hash,
                "elapsed_seconds": fingerprint.elapsed_seconds,
                "status": fingerprint.status,
            }
            with open(examples_fingerprint_file, "w") as f:
                json.dump(fingerprint_dict, f, indent=2)

        def read_examples_fingerprint() -> FingerprintResult | None:
            if examples_fingerprint_file.exists():
                with open(examples_fingerprint_file, "r") as f:
                    try:
                        data = json.load(f)
                        return FingerprintResult(
                            hash=data["hash"],
                            elapsed_seconds=data["elapsed_seconds"],
                            status=data["status"],
                        )
                    except (json.JSONDecodeError, KeyError):
                        return None
            return None

        # Calculate examples fingerprint (but don't save until tests pass)
        prev_examples_fingerprint = read_examples_fingerprint()
        examples_fingerprint_data = calculate_examples_fingerprint()
        examples_change = (
            True
            if prev_examples_fingerprint is None
            else not prev_examples_fingerprint.should_skip(examples_fingerprint_data)
        )

        # Set up Python test fingerprint caching
        python_test_fingerprint_file = cache_dir / "python_test_fingerprint.json"

        def write_python_test_fingerprint(fingerprint: FingerprintResult) -> None:
            fingerprint_dict = {
                "hash": fingerprint.hash,
                "elapsed_seconds": fingerprint.elapsed_seconds,
                "status": fingerprint.status,
            }
            with open(python_test_fingerprint_file, "w") as f:
                json.dump(fingerprint_dict, f, indent=2)

        def read_python_test_fingerprint() -> FingerprintResult | None:
            if python_test_fingerprint_file.exists():
                with open(python_test_fingerprint_file, "r") as f:
                    try:
                        data = json.load(f)
                        return FingerprintResult(
                            hash=data["hash"],
                            elapsed_seconds=data["elapsed_seconds"],
                            status=data["status"],
                        )
                    except (json.JSONDecodeError, KeyError):
                        return None
            return None

        # Calculate Python test fingerprint (but don't save until tests pass)
        prev_python_test_fingerprint = read_python_test_fingerprint()
        python_test_fingerprint_data = calculate_python_test_fingerprint()
        python_test_change = (
            True
            if prev_python_test_fingerprint is None
            else not prev_python_test_fingerprint.should_skip(
                python_test_fingerprint_data
            )
        )

        # Handle --run flag (unified emulation interface)
        if args.run is not None:
            if len(args.run) < 1:
                print("Error: --run requires a platform/board (e.g., esp32s3, uno)")
                sys.exit(1)

            platform = args.run[0].lower()

            # Look up backend from mapping table
            backend = _RUN_PLATFORM_BACKENDS.get(platform)

            if backend is None:
                # Platform not found - show error with supported platforms
                print(f"Error: Unknown platform '{platform}'")
                print()
                print("Supported platforms:")

                # Group platforms by backend
                qemu_platforms = [
                    p for p, b in _RUN_PLATFORM_BACKENDS.items() if b == "qemu"
                ]
                avr8js_boards = [
                    p for p, b in _RUN_PLATFORM_BACKENDS.items() if b == "avr8js"
                ]

                if qemu_platforms:
                    print(f"  ESP32 (QEMU): {', '.join(sorted(qemu_platforms))}")
                if avr8js_boards:
                    print(f"  AVR (avr8js): {', '.join(sorted(avr8js_boards))}")

                sys.exit(1)

            # Route to appropriate backend
            if backend == "qemu":
                print(f"=== QEMU Testing ({platform}) ===")
                # Convert --run to --qemu format for backward compatibility
                args.qemu = args.run
                run_qemu_tests(args)
                return
            elif backend == "avr8js":
                print(f"=== avr8js Testing ({platform}) ===")
                # Run avr8js tests
                run_avr8js_tests(args)
                return
            else:
                print(f"Error: Unknown backend '{backend}' for platform '{platform}'")
                sys.exit(1)

        # Handle QEMU testing (deprecated - use --run)
        if args.qemu is not None:
            print("=== QEMU Testing ===")
            print("Note: --qemu is deprecated, use --run instead")
            run_qemu_tests(args)
            return

        # Helper function to save all fingerprints with a given status
        def save_fingerprints_with_status(status: str) -> None:
            """Save all fingerprints with the specified status (success/failure)"""
            fingerprint_data.status = status
            cpp_test_fingerprint_data.status = status
            examples_fingerprint_data.status = status
            python_test_fingerprint_data.status = status

            write_fingerprint(fingerprint_data)
            write_cpp_test_fingerprint(cpp_test_fingerprint_data)
            write_examples_fingerprint(examples_fingerprint_data)
            write_python_test_fingerprint(python_test_fingerprint_data)

        # Track test success/failure for fingerprint status
        tests_passed = False

        try:
            # Run tests using the test runner with sequential example compilation
            # Check if we need to use sequential execution to avoid resource conflicts
            if not args.unit and not args.examples and not args.py and args.full:
                # Full test mode - use RunningProcessGroup for dependency-based execution
                from running_process import RunningProcess

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
                except KeyboardInterrupt:
                    _thread.interrupt_main()
                    raise
                except Exception as e:
                    print(f"Sequential test execution failed: {e}")
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
                    examples_change or args.no_fingerprint or args.force
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

                if args.no_fingerprint or args.force:
                    print("Fingerprint caching disabled (--no-fingerprint or --force)")

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
            # Always save fingerprints with appropriate status
            status = "success" if tests_passed else "failure"
            save_fingerprints_with_status(status)

        # Set up force exit daemon and exit
        daemon_thread = setup_force_exit()
        _CANCEL_WATCHDOG.set()

        # Print total execution time
        elapsed_time = time.time() - start_time
        print(f"\nTotal execution time: {elapsed_time:.2f} seconds")

        sys.exit(0)

    except KeyboardInterrupt:
        signal_interrupt()
        wait_for_cleanup()
        sys.exit(130)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        signal_interrupt()
        wait_for_cleanup()
        sys.exit(130)
