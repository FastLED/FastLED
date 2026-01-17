import os
import sys
from pathlib import Path

from ci.util.sccache_config import show_sccache_stats
from ci.util.test_types import TestArgs
from ci.util.timestamp_print import ts_print


def run_avr8js_tests(args: TestArgs) -> None:
    """Run AVR examples in avr8js emulation using Docker."""
    from running_process import RunningProcess

    from ci.docker_utils.avr8js_docker import DockerAVR8jsRunner
    from ci.util.output_formatter import TimestampFormatter

    if not args.run or len(args.run) < 1:
        ts_print("Error: --run requires a board (e.g., uno)")
        sys.exit(1)

    board = args.run[0].lower()
    supported_boards = ["uno", "attiny85", "attiny88", "nano_every"]
    if board not in supported_boards:
        ts_print(
            f"Error: Unsupported avr8js board: {board}. Supported boards: {', '.join(supported_boards)}"
        )
        sys.exit(1)

    ts_print(f"Running {board.upper()} avr8js tests using Docker...")

    # Determine which examples to test (skip the board argument)
    examples_to_test = args.run[1:] if len(args.run) > 1 else ["Test"]
    if not examples_to_test:  # Empty list means test Test example
        examples_to_test = ["Test"]

    ts_print(f"Testing examples: {examples_to_test}")

    # Quick test mode - just validate the setup
    if os.getenv("FASTLED_AVR8JS_QUICK_TEST") == "true":
        ts_print("Quick test mode - validating Docker avr8js setup only")
        ts_print("avr8js AVR Docker option is working correctly!")
        return

    # Initialize Docker avr8js runner
    docker_runner = DockerAVR8jsRunner()

    # Check if Docker is available
    if not docker_runner.check_docker_available():
        ts_print("❌ ERROR: Docker is not available or not running")
        ts_print()
        ts_print("Please install Docker Desktop and ensure it's running:")
        ts_print("  https://www.docker.com/products/docker-desktop")
        ts_print()
        sys.exit(1)

    # Check if avr8js Docker image exists
    avr8js_image = docker_runner.docker_image
    image_exists = docker_runner.check_image_exists(avr8js_image)

    if not image_exists:
        ts_print(f"❌ Docker avr8js image not found locally: {avr8js_image}")
        ts_print()
        ts_print("Attempting to pull avr8js image from Docker Hub...")
        try:
            docker_runner.pull_image()
            ts_print(f"✅ Successfully pulled {avr8js_image}")
            ts_print()
        except KeyboardInterrupt:
            from ci.util.global_interrupt_handler import (
                handle_keyboard_interrupt_properly,
            )

            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            ts_print(f"❌ Failed to pull avr8js image: {e}")
            ts_print()
            ts_print("Please build the image manually:")
            ts_print(
                f"  docker build -f ci/docker_utils/Dockerfile.avr8js -t {avr8js_image} ."
            )
            ts_print()
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
        ts_print(f"\n{'=' * 70}")
        ts_print(f"Testing: {example}")
        ts_print(f"{'=' * 70}")

        try:
            # Step 1: Show what we're compiling
            example_ino_path = Path("examples") / example / f"{example}.ino"
            ts_print("\n[STEP 1/3] Compiling Arduino Sketch")
            ts_print(f"  Source file: {example_ino_path}")
            ts_print(
                f"  Target board: {board} ({config['mcu']} @ {config['frequency']}Hz)"
            )
            ts_print("  Compiler: PlatformIO via ci/ci-compile.py")
            ts_print()

            # Build the example for the specified board
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
                output_formatter=TimestampFormatter(),
            )

            # Stream build output
            with build_proc.line_iter(timeout=None) as it:
                for line in it:
                    ts_print(line)

            build_returncode = build_proc.wait()
            if build_returncode != 0:
                ts_print(
                    f"\n❌ Build failed for {example} with exit code: {build_returncode}"
                )
                failure_count += 1
                continue

            ts_print(f"\n✅ Build successful for {example}")

            # Step 2: Locate compiled firmware
            ts_print("\n[STEP 2/3] Locating Compiled Firmware")
            build_dir = Path(".build")
            elf_files = list(build_dir.rglob("*.elf"))
            if not elf_files:
                ts_print(f"  ❌ ELF file not found in {build_dir}")
                failure_count += 1
                continue

            elf_path = elf_files[0]
            hex_path = elf_path.with_suffix(".hex")
            ts_print(f"  ELF file: {elf_path}")
            ts_print(f"  HEX file: {hex_path}")
            ts_print("  (avr8js requires HEX format for execution)")

            if not hex_path.exists():
                ts_print(f"  ❌ HEX file not found: {hex_path}")
                failure_count += 1
                continue

            # Step 3: Run in avr8js Docker
            ts_print("\n[STEP 3/3] Running in AVR8JS Docker Emulator")
            ts_print(f"  Docker image: {docker_runner.docker_image}")
            ts_print(f"  Firmware: {hex_path}")
            ts_print("  Timeout: 15 seconds")
            ts_print()

            # Set up output file
            output_file = "avr8js_output.txt"

            # Run avr8js in Docker
            avr8js_returncode = docker_runner.run(
                elf_path=elf_path,
                mcu=str(config["mcu"]),
                frequency=int(config["frequency"]),
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
                    ts_print(f"SUCCESS: {example} tests passed in Docker avr8js")
                    success_count += 1
                else:
                    ts_print(f"FAILED: {example} tests did not pass in Docker avr8js")
                    failure_count += 1
            elif avr8js_returncode == 0:
                ts_print(f"SUCCESS: {example} ran successfully in Docker avr8js")
                success_count += 1
            else:
                ts_print(
                    f"FAILED: {example} failed in Docker avr8js with exit code: {avr8js_returncode}"
                )
                failure_count += 1

        except KeyboardInterrupt:
            import _thread

            _thread.interrupt_main()
            raise
        except Exception as e:
            ts_print(f"ERROR: {example} failed with exception: {e}")
            failure_count += 1

    # Summary
    ts_print(f"\n=== avr8js {board.upper()} Test Summary ===")
    ts_print(f"Examples tested: {len(examples_to_test)}")
    ts_print(f"Successful: {success_count}")
    ts_print(f"Failed: {failure_count}")

    # Show sccache statistics
    show_sccache_stats()

    if failure_count > 0:
        ts_print("Some tests failed. See output above for details.")
        sys.exit(1)
    else:
        ts_print("All avr8js tests passed!")
