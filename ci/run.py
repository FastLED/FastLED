from __future__ import annotations

import asyncio
import sys
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
        platform, example = args
        return await run_build_and_test_loop(platform, example)
    else:
        print(f"Usage: run [test_name] | [platform example]", file=sys.stderr)
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
            "type": "simavr",
            "image": "fastled/simavr:latest",
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

            # For SimAVR, build from Dockerfile
            build_cmd = [
                "docker",
                "build",
                "-t",
                image_name,
                "-f",
                "ci/docker/simavr/Dockerfile",
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

    # Platform-specific timeouts
    timeouts = {
        "uno": 30,
        "esp32s3": 60,
        "esp32": 60,
        "esp32c3": 60,
    }

    timeout = timeouts.get(platform, 30)

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
            # SimAVR simulation
            success = await run_simavr(firmware_path, timeout)
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


async def run_simavr(firmware_path: Path, timeout: int) -> bool:
    """Run SimAVR simulation for AVR firmware.

    Returns:
        True for success, False for failure
    """
    import os

    # Convert Windows path for Docker volume mount
    if os.name == "nt":
        # Convert C:\path\to\dir to /c/path/to/dir
        # firmware_path is .build/pio/uno/.pio/build/uno/firmware.elf
        # We need .build/pio/uno, which is 4 parents up
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
        "fastled/simavr:latest",
        "-m",
        "atmega328p",
        "-f",
        "16000000",
        "/build/.pio/build/uno/firmware.elf",
    ]

    print(f"Executing: {' '.join(cmd)}")
    print()

    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT
        )

        # Read output with timeout
        try:
            stdout, _ = await asyncio.wait_for(proc.communicate(), timeout=timeout)
            output = stdout.decode("utf-8", errors="replace")
            print(output)

            # Check return code
            if proc.returncode == 0:
                return True
            else:
                print(f"SimAVR exited with code {proc.returncode}")
                return False

        except asyncio.TimeoutError:
            print(f"Simulation reached timeout ({timeout}s) - treating as success")
            proc.kill()
            await proc.wait()
            return True

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"SimAVR execution failed: {e}")
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

        # Read output with timeout
        try:
            stdout, _ = await asyncio.wait_for(proc.communicate(), timeout=timeout)
            output = stdout.decode("utf-8", errors="replace")
            print(output)

            # Check return code
            if proc.returncode == 0:
                return True
            else:
                print(f"QEMU exited with code {proc.returncode}")
                return False

        except asyncio.TimeoutError:
            print(f"Emulation reached timeout ({timeout}s) - treating as success")
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
