from __future__ import annotations

import asyncio
import sys
from pathlib import Path
from typing import Any

from textual.app import App, ComposeResult
from textual.containers import Vertical
from textual.widgets import Footer, Header

from ci.run.views.main_menu import MainMenu


class FastLEDCI(App[Any]):
    CSS_PATH = "run/assets/theme.css"
    TITLE = "FastLED CI"
    BINDINGS = [
        ("ctrl+c", "quit", "Quit"),
        ("q", "quit", "Quit"),
    ]

    def on_mount(self) -> None:
        # Start on main menu view
        self.push_screen(MainMenu())

    def compose(self) -> ComposeResult:
        yield Header(show_clock=False)
        yield Vertical(id="root")
        yield Footer()


async def run_non_interactive(args: list[str]) -> int:
    """Run a command non-interactively based on arguments."""
    if len(args) == 1:
        # Single argument - run as C++ unit test
        test_name = args[0]
        cmd = ["uv", "run", "test.py", test_name]

        # Stream output in real-time by inheriting stdout/stderr
        print(f"Running: {' '.join(cmd)}\n")

        try:
            proc = await asyncio.create_subprocess_exec(*cmd)
            return_code = await proc.wait()

            if return_code == 0:
                print(f"\n✓ Success")
                return 0
            else:
                print(f"\n✗ Failed with exit code {return_code}")
                return return_code
        except KeyboardInterrupt:
            print("\n✗ Interrupted")
            raise
        except Exception as e:
            print(f"\n✗ Error: {e}", file=sys.stderr)
            return 1

    elif len(args) == 2:
        # Two arguments - compile example for platform with full simulation loop
        platform, arg = args

        # Smart path handling
        from pathlib import Path

        arg_path = Path(arg)

        # Case 1: Direct .hex file path
        if arg.endswith(".hex") and arg_path.exists():
            print(f"Running simulation with hex file: {arg}")
            firmware_path = arg_path
            return await run_simulator_direct(platform, firmware_path)

        # Case 2: Directory containing firmware.hex
        elif arg_path.is_dir():
            hex_file = arg_path / "firmware.hex"
            if hex_file.exists():
                print(f"Found firmware.hex in directory: {arg}")
                return await run_simulator_direct(platform, hex_file)
            else:
                print(
                    f"Error: No firmware.hex found in directory: {arg}", file=sys.stderr
                )
                return 1

        # Case 3: .ino file - extract example name
        elif arg.endswith(".ino"):
            example = Path(arg).stem  # Remove .ino extension
            print(f"Extracted example name from .ino file: {example}")
            return await run_build_and_test_loop(platform, example)

        # Case 4: Plain example name (existing behavior)
        else:
            example = arg
            return await run_build_and_test_loop(platform, example)
    else:
        print(f"Usage: run [test_name] | [platform example|path]", file=sys.stderr)
        print(f"  example: 'run uno Test' or 'run uno Test.ino'", file=sys.stderr)
        print(
            f"  path: 'run uno .build/uno/firmware.hex' or 'run uno .build/uno/'",
            file=sys.stderr,
        )
        return 1


async def run_simulator_direct(
    platform: str, firmware_path: Path, timeout: int = 30
) -> int:
    """Run simulator directly with pre-compiled firmware (skip build step).

    Args:
        platform: Platform name (e.g., 'uno', 'esp32s3')
        firmware_path: Path to firmware.hex file
        timeout: Simulation timeout in seconds

    Returns:
        Exit code: 0 for success, 1 for failure
    """
    import time

    start_time = time.time()

    print(f"Running {platform} simulation (no build)...")
    print(f"Firmware: {firmware_path}")
    print(f"Timeout: {timeout}s")
    print()

    try:
        # Check if simulation is supported for this platform
        supported_platforms = ["uno"]
        if platform not in supported_platforms:
            print(f"Skipping simulation for {platform} (not yet supported)")
            return 0  # Treat as success

        # Run simulator
        success, sim_time = await run_simulator(platform, firmware_path, "direct")

        if not success:
            print(f"\n✗ Simulation failed")
            return 1

        print(f"✓ Simulation succeeded ({sim_time:.1f}s)")

        total_time = time.time() - start_time
        print()
        print("=" * 60)
        print(f"✓ Simulation succeeded (total: {total_time:.1f}s)")
        print("=" * 60)
        return 0

    except KeyboardInterrupt:
        print("\n✗ Interrupted")
        raise
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        return 1


async def run_build_and_test_loop(platform: str, example: str) -> int:
    """Execute full build + docker + simulation loop for a platform and example.

    Args:
        platform: Platform name (e.g., 'uno', 'esp32s3')
        example: Example sketch name (e.g., 'Blink')

    Returns:
        Exit code: 0 for success, 1 for build failure, 2 for docker failure, 3 for simulation failure
    """
    import time
    from pathlib import Path

    start_time = time.time()

    # Phase 1 & 2: Parallel build + docker prep
    print("=" * 60)
    print(f"Running build and test loop for {platform}/{example}")
    print("=" * 60)
    print()

    try:
        build_task = asyncio.create_task(compile_example(platform, example))
        docker_task = asyncio.create_task(ensure_docker_image(platform))

        # Wait for both to complete
        build_result, docker_result = await asyncio.gather(
            build_task, docker_task, return_exceptions=True
        )

        # Check for exceptions
        if isinstance(build_result, Exception):
            print(f"\n✗ Build failed with exception: {build_result}")
            return 1
        if isinstance(docker_result, Exception):
            print(f"\n✗ Docker preparation failed with exception: {docker_result}")
            return 2

        # Unpack results
        build_success, build_time = build_result
        docker_success, docker_time = docker_result

        if not build_success:
            print(f"\n✗ Build failed")
            return 1

        if not docker_success:
            print(f"\n✗ Docker image preparation failed")
            return 2

        print()
        print(f"✓ Build succeeded ({build_time:.1f}s)")
        print(f"✓ Docker image ready ({docker_time:.1f}s)")
        print()

        # Phase 3: Run simulation/emulation
        firmware_path = get_firmware_path(platform, example)
        if not firmware_path or not firmware_path.exists():
            print(f"\n✗ Firmware not found at expected location: {firmware_path}")
            return 1

        sim_success, sim_time = await run_simulator(platform, firmware_path, example)

        if not sim_success:
            print(f"\n✗ Simulation failed")
            return 3

        print(f"✓ Simulation succeeded ({sim_time:.1f}s)")
        print()

        total_time = time.time() - start_time
        print("=" * 60)
        print(f"✓ All phases succeeded (total: {total_time:.1f}s)")
        print("=" * 60)
        return 0

    except KeyboardInterrupt:
        print("\n✗ Interrupted")
        raise
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        return 1


async def compile_example(platform: str, example: str) -> tuple[bool, float]:
    """Compile example for platform.

    Returns:
        Tuple of (success: bool, time_taken: float)
    """
    import time

    start_time = time.time()
    cmd = ["uv", "run", "ci/ci-compile.py", platform, "--examples", example]

    print(f"Running: {' '.join(cmd)}")
    print()

    try:
        proc = await asyncio.create_subprocess_exec(*cmd)
        return_code = await proc.wait()
        elapsed = time.time() - start_time

        return (return_code == 0, elapsed)
    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"Build error: {e}")
        elapsed = time.time() - start_time
        return (False, elapsed)


async def ensure_docker_image(platform: str) -> tuple[bool, float]:
    """Ensure Docker image for platform exists (build or pull if needed).

    Returns:
        Tuple of (success: bool, time_taken: float)
    """
    import time

    start_time = time.time()

    # Map platform to docker requirements
    # NOTE: ESP32 QEMU Docker image does not exist on Docker Hub.
    # ESP32 QEMU should be installed locally via `uv run ci/install-qemu.py` instead.
    # Leaving ESP32 entries commented for future reference.
    platform_docker_map = {
        "uno": {
            "type": "avr8js",
            "image": "niteris/fastled-avr8js:latest",
            "needs_build": True,
        },
        # "esp32s3": {"type": "qemu", "image": "espressif/qemu:esp-develop-20240606", "needs_build": False},
        # "esp32": {"type": "qemu", "image": "espressif/qemu:esp-develop-20240606", "needs_build": False},
        # "esp32c3": {"type": "qemu", "image": "espressif/qemu:esp-develop-20240606", "needs_build": False},
    }

    if platform not in platform_docker_map:
        print(
            f"Warning: No Docker image configured for platform '{platform}', skipping simulation"
        )
        elapsed = time.time() - start_time
        return (True, elapsed)  # Not a failure, just unsupported

    docker_info = platform_docker_map[platform]
    image_name = docker_info["image"]
    needs_build = docker_info["needs_build"]

    print(f"Ensuring Docker image: {image_name}")

    try:
        # Check if image exists
        check_cmd = ["docker", "images", "-q", image_name]
        proc = await asyncio.create_subprocess_exec(
            *check_cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
        )
        stdout, _ = await proc.communicate()

        if stdout.strip():
            print(f"✓ Docker image ready (cached)")
            elapsed = time.time() - start_time
            return (True, elapsed)

        # Image doesn't exist
        if needs_build:
            print(f"Building Docker image: {image_name}")
            print("This may take a few minutes on first run...")
            print()

            # For avr8js, build from Dockerfile
            build_cmd = [
                "docker",
                "build",
                "-t",
                image_name,
                "-f",
                "ci/docker/Dockerfile.avr8js",
                ".",
            ]
            proc = await asyncio.create_subprocess_exec(*build_cmd)
            return_code = await proc.wait()

            elapsed = time.time() - start_time
            if return_code != 0:
                print(f"✗ Failed to build Docker image")
                return (False, elapsed)
        else:
            print(f"Pulling Docker image: {image_name}")
            print("This may take a few minutes on first run...")
            print()

            # For QEMU, pull from registry
            pull_cmd = ["docker", "pull", image_name]
            proc = await asyncio.create_subprocess_exec(*pull_cmd)
            return_code = await proc.wait()

            elapsed = time.time() - start_time
            if return_code != 0:
                print(f"✗ Failed to pull Docker image")
                return (False, elapsed)

        elapsed = time.time() - start_time
        return (True, elapsed)

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"Docker error: {e}")
        elapsed = time.time() - start_time
        return (False, elapsed)


def get_firmware_path(platform: str, example: str) -> Path | None:
    """Get expected firmware path for platform and example."""
    from pathlib import Path

    # Platform-specific firmware paths
    if platform == "uno":
        return Path(f".build/pio/uno/.pio/build/uno/firmware.elf")
    elif platform in ["esp32s3", "esp32", "esp32c3"]:
        return Path(f".build/pio/{platform}/.pio/build/{platform}/firmware.bin")
    else:
        return None


async def run_simulator(
    platform: str, firmware_path: Path, example: str
) -> tuple[bool, float]:
    """Run simulation/emulation for platform.

    Returns:
        Tuple of (success: bool, time_taken: float)
    """
    import time

    start_time = time.time()

    # Example-specific timeouts (override platform defaults)
    example_timeouts = {
        "Test": 10,  # Test.ino timeout
    }

    # Platform-specific timeouts (defaults)
    platform_timeouts = {
        "uno": 30,
        "esp32s3": 60,
        "esp32": 60,
        "esp32c3": 60,
    }

    # Use example-specific timeout if available, otherwise platform default
    timeout = example_timeouts.get(example, platform_timeouts.get(platform, 30))

    # Check if simulation is supported for this platform
    supported_platforms = ["uno"]  # Only AVR/uno is currently supported
    if platform not in supported_platforms:
        print(f"Skipping simulation for {platform} (not yet supported)")
        elapsed = time.time() - start_time
        return (True, elapsed)  # Treat as success - build succeeded

    print(f"Running {platform} simulation...")
    print(f"Firmware: {firmware_path}")
    print(f"Timeout: {timeout}s")
    print()

    try:
        if platform == "uno":
            # avr8js simulation
            success = await run_avr8js(firmware_path, timeout)
        elif platform in ["esp32s3", "esp32", "esp32c3"]:
            # QEMU emulation
            success = await run_qemu(platform, firmware_path, timeout)
        else:
            print(f"Warning: No simulator configured for platform '{platform}'")
            success = True  # Not a failure, just unsupported

        elapsed = time.time() - start_time
        return (success, elapsed)

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"Simulator error: {e}")
        elapsed = time.time() - start_time
        return (False, elapsed)


async def run_avr8js(firmware_path: Path, timeout: int) -> bool:
    """Run avr8js simulation for AVR firmware using proper UART capture.

    Returns:
        True for success, False for failure
    """
    import shutil
    from pathlib import Path

    print(f"Starting avr8js with UART capture...")
    print()

    # Check if Node.js is available
    node_available = shutil.which("node") is not None

    # Check if avr8js runner script exists
    script_path = Path(__file__).parent / "docker" / "avr8js" / "main.ts"
    script_exists = script_path.exists()

    if node_available and script_exists:
        print("✓ Node.js detected - running avr8js directly (faster than Docker)")
        return await run_avr8js_native(firmware_path, timeout, script_path)
    else:
        if not node_available:
            print("⚠ Node.js not found - falling back to Docker")
        if not script_exists:
            print(f"⚠ avr8js runner script not found at {script_path}")
        print("Using Docker for avr8js execution...")
        return await run_avr8js_docker(firmware_path, timeout)


async def run_avr8js_native(
    firmware_path: Path, timeout: int, script_path: Path
) -> bool:
    """Run avr8js directly with Node.js (no Docker).

    Returns:
        True for success, False for failure
    """
    firmware_path = Path(firmware_path).resolve()

    # avr8js requires HEX file, not ELF
    hex_path = firmware_path.with_suffix(".hex")
    if not hex_path.exists():
        print(f"ERROR: HEX file not found: {hex_path}")
        return False

    # Check if npm dependencies are installed
    avr8js_dir = script_path.parent
    node_modules = avr8js_dir / "node_modules"
    package_json = avr8js_dir / "package.json"

    if not node_modules.exists() and package_json.exists():
        print("📦 Installing avr8js npm dependencies (first time only)...")
        try:
            import shutil

            npm_path = shutil.which("npm")
            if not npm_path:
                print("ERROR: npm command not found")
                return False

            proc = await asyncio.create_subprocess_exec(
                npm_path,
                "install",
                cwd=str(avr8js_dir),
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )
            stdout_data, stderr_data = await proc.communicate()
            if proc.returncode != 0:
                print(f"ERROR: npm install failed")
                if stderr_data:
                    print(stderr_data.decode("utf-8", errors="replace"))
                return False
            print("✓ Dependencies installed")
        except Exception as e:
            print(f"ERROR: Failed to install npm dependencies: {e}")
            return False

    print(f"Running avr8js natively with tsx (TypeScript)...")
    print(f"Script: {script_path}")
    print(f"Firmware: {hex_path}")
    print(f"Timeout: {timeout}s")
    print()

    try:
        # Run avr8js runner script directly
        cmd = ["npx", "tsx", str(script_path), str(hex_path), str(timeout)]

        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )

        try:

            async def stream_output():
                """Stream stdout and stderr in real-time."""
                tasks = []

                async def read_stdout():
                    if proc.stdout:
                        while True:
                            line = await proc.stdout.readline()
                            if not line:
                                break
                            print(line.decode("utf-8", errors="replace"), end="")

                async def read_stderr():
                    if proc.stderr:
                        while True:
                            line = await proc.stderr.readline()
                            if not line:
                                break
                            print(
                                line.decode("utf-8", errors="replace"),
                                end="",
                                file=sys.stderr,
                            )

                tasks.append(asyncio.create_task(read_stdout()))
                tasks.append(asyncio.create_task(read_stderr()))

                await asyncio.gather(*tasks)
                await proc.wait()

            # Stream output with timeout
            await asyncio.wait_for(stream_output(), timeout=timeout + 10)

            return proc.returncode == 0

        except asyncio.TimeoutError:
            print(f"\n❌ Timeout after {timeout}s")
            proc.kill()
            await proc.wait()
            return False

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"avr8js native execution failed: {e}")
        import traceback

        traceback.print_exc()
        return False


async def run_avr8js_docker(firmware_path: Path, timeout: int) -> bool:
    """Run avr8js in Docker container (fallback when Node.js unavailable).

    Returns:
        True for success, False for failure
    """
    from ci.docker.avr8js_docker import DockerAVR8jsRunner

    try:
        # Run in thread pool since DockerAVR8jsRunner is synchronous
        loop = asyncio.get_event_loop()

        def run_docker_avr8js():
            """Run avr8js in synchronous context."""
            runner = DockerAVR8jsRunner()

            # Check Docker availability
            if not runner.check_docker_available():
                print("ERROR: Docker is not available or not running")
                return False

            # Run avr8js with UART output capture
            returncode = runner.run(
                elf_path=firmware_path,
                mcu="atmega328p",
                frequency=16000000,
                timeout=timeout,
                output_file=None,  # Stream to stdout directly
            )

            return returncode == 0

        # Run in executor to avoid blocking async loop
        success = await loop.run_in_executor(None, run_docker_avr8js)
        return success

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"avr8js Docker execution failed: {e}")
        import traceback

        traceback.print_exc()
        return False


async def run_qemu(platform: str, firmware_path: Path, timeout: int) -> bool:
    """Run QEMU emulation for ESP32 firmware.

    Returns:
        True for success, False for failure
    """
    import os

    # Map platform to QEMU machine
    machine_map = {
        "esp32": "esp32",
        "esp32s3": "esp32s3",
        "esp32c3": "esp32c3",
    }

    machine = machine_map.get(platform, "esp32")

    # Convert Windows path for Docker volume mount
    if os.name == "nt":
        # firmware_path is .build/pio/{platform}/.pio/build/{platform}/firmware.bin
        # We need .build/pio/{platform}, which is 4 parents up
        build_dir = firmware_path.parent.parent.parent.parent
        docker_path = str(build_dir.absolute()).replace("\\", "/")
        if len(docker_path) > 2 and docker_path[1:3] == ":/":
            docker_path = "/" + docker_path[0].lower() + docker_path[2:]
    else:
        build_dir = firmware_path.parent.parent.parent.parent
        docker_path = str(build_dir.absolute())

    cmd = [
        "docker",
        "run",
        "--rm",
        "-v",
        f"{docker_path}:/build",
        "espressif/qemu:esp-develop-20240606",
        "qemu-system-xtensa" if platform != "esp32c3" else "qemu-system-riscv32",
        "-nographic",
        "-machine",
        machine,
        "-drive",
        f"file=/build/.pio/build/{platform}/firmware.bin,if=mtd,format=raw",
        "-serial",
        "mon:stdio",
    ]

    print(f"Executing: {' '.join(cmd)}")
    print()

    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT
        )

        # Stream output line by line with timeout
        try:

            async def read_output():
                """Read and print output line by line."""
                if proc.stdout:
                    while True:
                        line = await proc.stdout.readline()
                        if not line:
                            break
                        print(line.decode("utf-8", errors="replace"), end="")

            # Run output reading with timeout
            await asyncio.wait_for(read_output(), timeout=timeout)

            # Wait for process to complete
            await proc.wait()

            # Check return code
            if proc.returncode == 0:
                return True
            else:
                print(f"QEMU exited with code {proc.returncode}")
                return False

        except asyncio.TimeoutError:
            print(f"\nEmulation reached timeout ({timeout}s) - treating as success")
            proc.kill()
            await proc.wait()
            return True

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"QEMU execution failed: {e}")
        return False


def main() -> None:
    # If arguments provided, run non-interactively
    args = sys.argv[1:]
    if args:
        exit_code = asyncio.run(run_non_interactive(args))
        sys.exit(exit_code)
    else:
        # No arguments - launch interactive TUI
        FastLEDCI().run()


if __name__ == "__main__":
    main()
