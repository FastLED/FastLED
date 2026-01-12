import os
import sys
from pathlib import Path

from ci.util.sccache_config import show_sccache_stats
from ci.util.test_types import TestArgs
from ci.util.timestamp_print import ts_print


def run_qemu_tests(args: TestArgs) -> None:
    """Run examples in QEMU emulation using Docker."""
    from running_process import RunningProcess

    from ci.docker_utils.qemu_esp32_docker import DockerQEMURunner

    if not args.qemu or len(args.qemu) < 1:
        ts_print("Error: --qemu requires a platform (e.g., esp32s3)")
        sys.exit(1)

    platform = args.qemu[0].lower()
    supported_platforms = ["esp32dev", "esp32c3", "esp32s3"]
    if platform not in supported_platforms:
        ts_print(
            f"Error: Unsupported QEMU platform: {platform}. Supported platforms: {', '.join(supported_platforms)}"
        )
        sys.exit(1)

    ts_print(f"Running {platform.upper()} QEMU tests using Docker...")

    # Determine which examples to test (skip the platform argument)
    examples_to_test = args.qemu[1:] if len(args.qemu) > 1 else ["BlinkParallel"]
    if not examples_to_test:  # Empty list means test all available examples
        examples_to_test = ["BlinkParallel", "RMT5WorkerPool"]

    ts_print(f"Testing examples: {examples_to_test}")

    # Quick test mode - just validate the setup
    if os.getenv("FASTLED_QEMU_QUICK_TEST") == "true":
        ts_print("Quick test mode - validating Docker QEMU setup only")
        ts_print("QEMU ESP32 Docker option is working correctly!")
        return

    # Initialize Docker QEMU runner
    docker_runner = DockerQEMURunner()

    # Check if Docker is available
    if not docker_runner.check_docker_available():
        ts_print("❌ ERROR: Docker is not available or not running")
        ts_print()
        ts_print("Please install Docker Desktop and ensure it's running:")
        ts_print("  https://www.docker.com/products/docker-desktop")
        ts_print()
        sys.exit(1)

    # Check if board Docker image exists (needed for compilation)
    board_image = docker_runner.get_board_image_name(platform)
    image_exists = docker_runner.check_image_exists(board_image)

    if not image_exists:
        ts_print(f"❌ Docker image for {platform} not found locally")
        ts_print()

        # Try pulling from Docker Hub registry first
        ts_print("Attempting to pull prebuilt image from Docker Hub...")
        pull_success = docker_runner.pull_and_tag_board_image(platform)

        if pull_success:
            ts_print("✅ Successfully pulled and configured Docker image")
            ts_print()
            image_exists = True
        else:
            # Pull failed - show build options
            ts_print()
            ts_print("❌ Could not pull image from Docker Hub")
            ts_print()

            # Check if we should auto-build
            if args.build:
                # Auto-build mode (explicit flag)
                ts_print("Auto-build enabled (--build flag)")
                ts_print()
                build_result = docker_runner.build_board_image(platform, progress=True)
                if build_result != 0:
                    ts_print(f"❌ Failed to build Docker image for {platform}")
                    sys.exit(1)
            elif args.interactive:
                # Interactive prompt mode
                response = (
                    input("Docker image missing. Build now? [y/N]: ").strip().lower()
                )
                if response in ["y", "yes"]:
                    ts_print()
                    build_result = docker_runner.build_board_image(
                        platform, progress=True
                    )
                    if build_result != 0:
                        ts_print(f"❌ Failed to build Docker image for {platform}")
                        sys.exit(1)
                else:
                    ts_print("Build cancelled. Please build the image manually.")
                    sys.exit(1)
            else:
                # Default mode - automatically build with warning and delay
                ts_print("⚠️  Docker image not available. Will build automatically.")
                ts_print("   This will take approximately 30 minutes.")
                ts_print("   Press Ctrl+C within 5 seconds to cancel...")
                ts_print()

                import time

                for i in range(5, 0, -1):
                    ts_print(f"   Starting build in {i}...", end="\r")
                    time.sleep(1)
                ts_print()  # New line after countdown
                ts_print()

                ts_print("🔨 Building Docker image automatically...")
                ts_print()
                build_result = docker_runner.build_board_image(platform, progress=True)
                if build_result != 0:
                    ts_print(f"❌ Failed to build Docker image for {platform}")
                    ts_print()
                    ts_print("Options:")
                    ts_print("  1. Try again with verbose logging:")
                    ts_print(
                        f"     bash test --qemu {platform} {' '.join(examples_to_test)} --build"
                    )
                    ts_print()
                    ts_print("  2. Build image separately:")
                    ts_print(f"     bash compile --docker --build {platform} Blink")
                    ts_print()
                    sys.exit(1)

    success_count = 0
    failure_count = 0

    # Test each example
    for example in examples_to_test:
        ts_print(f"\n--- Testing {example} ---")

        try:
            # Build the example for the specified platform with merged binary for QEMU
            ts_print(f"Building {example} for {platform} with merged binary...")
            # Use Docker compilation on Windows to avoid toolchain issues
            # IMPORTANT: Use --local on GitHub Actions to avoid pulling Docker images
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
            # Force --local on GitHub Actions to avoid pulling Docker images
            from running_process import RunningProcess

            from ci.util.github_env import is_github_actions
            from ci.util.output_formatter import TimestampFormatter

            if is_github_actions():
                build_cmd.append("--local")
            elif sys.platform == "win32":
                build_cmd.append("--docker")
            build_proc = RunningProcess(
                build_cmd,
                timeout=600,
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
                    f"Build failed for {example} with exit code: {build_returncode}"
                )
                failure_count += 1
                continue

            ts_print(f"Build successful for {example}")

            # Check if merged binary exists
            merged_bin_path = Path("qemu-build/merged.bin")
            if not merged_bin_path.exists():
                ts_print(f"Merged binary not found: {merged_bin_path}")
                failure_count += 1
                continue

            ts_print(f"Merged binary found: {merged_bin_path}")

            # Run in QEMU using Docker
            ts_print(f"Running {example} in Docker QEMU...")

            # Set up interrupt regex pattern
            interrupt_regex = r"(FL_WARN.*test finished)|(Setup complete - starting blink animation)|(guru meditation)|(abort\(\))|(LoadProhibited)"

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
                ts_print(f"SUCCESS: {example} ran successfully in Docker QEMU")
                success_count += 1
            else:
                ts_print(
                    f"FAILED: {example} failed in Docker QEMU with exit code: {qemu_returncode}"
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
    ts_print(f"\n=== QEMU {platform.upper()} Test Summary ===")
    ts_print(f"Examples tested: {len(examples_to_test)}")
    ts_print(f"Successful: {success_count}")
    ts_print(f"Failed: {failure_count}")

    # Show sccache statistics
    show_sccache_stats()

    if failure_count > 0:
        ts_print("Some tests failed. See output above for details.")
        sys.exit(1)
    else:
        ts_print("All QEMU tests passed!")
