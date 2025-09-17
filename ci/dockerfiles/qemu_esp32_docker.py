#!/usr/bin/env python3
"""
Docker-based QEMU ESP32 Runner

Runs ESP32 firmware in QEMU using Docker containers for better isolation
and consistency across different environments.
"""

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from ci.dockerfiles.DockerManager import DockerManager


def get_docker_env() -> Dict[str, str]:
    """Get environment for Docker commands, handling Git Bash/MSYS2 path conversion."""
    env = os.environ.copy()
    # Set UTF-8 encoding environment variables for Windows
    env["PYTHONIOENCODING"] = "utf-8"
    env["PYTHONUTF8"] = "1"
    # Only set MSYS_NO_PATHCONV if we're in a Git Bash/MSYS2 environment
    if (
        "MSYSTEM" in os.environ
        or os.environ.get("TERM") == "xterm"
        or "bash.exe" in os.environ.get("SHELL", "")
    ):
        env["MSYS_NO_PATHCONV"] = "1"
    return env


def run_subprocess_safe(
    cmd: List[str], **kwargs: Any
) -> subprocess.CompletedProcess[Any]:
    """Run subprocess with safe UTF-8 handling and error replacement."""
    kwargs.setdefault("encoding", "utf-8")
    kwargs.setdefault("errors", "replace")
    kwargs.setdefault("env", get_docker_env())
    return subprocess.run(cmd, **kwargs)  # type: ignore[misc]


class DockerQEMURunner:
    """Runner for ESP32 QEMU emulation using Docker containers."""

    DEFAULT_IMAGE = "espressif/idf:latest"
    ALTERNATIVE_IMAGE = "espressif/idf:release-v5.2"

    def __init__(self, docker_image: Optional[str] = None):
        """Initialize Docker QEMU runner.

        Args:
            docker_image: Docker image to use, defaults to espressif/qemu
        """
        self.docker_manager = DockerManager()
        self.docker_image = docker_image or self.DEFAULT_IMAGE
        self.container_name = None
        # Use Linux-style paths for all containers since we're using Ubuntu/Alpine
        self.firmware_mount_path = "/workspace/firmware"

    def check_docker_available(self) -> bool:
        """Check if Docker is available and running."""
        try:
            result = run_subprocess_safe(
                ["docker", "version"], capture_output=True, text=True, timeout=5
            )
            return result.returncode == 0
        except (subprocess.SubprocessError, FileNotFoundError):
            return False

    def pull_image(self):
        """Pull the Docker image if not already available."""
        print(f"Ensuring Docker image {self.docker_image} is available...")
        try:
            # Check if image exists locally
            result = run_subprocess_safe(
                ["docker", "images", "-q", self.docker_image],
                capture_output=True,
                text=True,
            )
            if not result.stdout.strip():
                # Image doesn't exist, pull it directly using docker command
                print(f"Pulling Docker image: {self.docker_image}")
                result = run_subprocess_safe(
                    ["docker", "pull", self.docker_image],
                    capture_output=True,
                    text=True,
                    check=True,
                )
                print(f"Successfully pulled {self.docker_image}")
            else:
                print(f"Image {self.docker_image} already available locally")
        except subprocess.CalledProcessError as e:
            print(f"Warning: Failed to pull image {self.docker_image}: {e}")
            if self.docker_image != self.ALTERNATIVE_IMAGE:
                print(f"Trying alternative image: {self.ALTERNATIVE_IMAGE}")
                self.docker_image = self.ALTERNATIVE_IMAGE
                try:
                    print(f"Pulling alternative Docker image: {self.docker_image}")
                    result = run_subprocess_safe(
                        ["docker", "pull", self.docker_image],
                        capture_output=True,
                        text=True,
                        check=True,
                    )
                    print(f"Successfully pulled {self.docker_image}")
                except subprocess.CalledProcessError as e2:
                    print(f"Failed to pull alternative image: {e2}")
                    raise e2

    def prepare_firmware(self, firmware_path: Path) -> Path:
        """Prepare firmware files for mounting into Docker container.

        Args:
            firmware_path: Path to firmware.bin or build directory

        Returns:
            Path to the prepared firmware directory
        """
        # Create temporary directory for firmware files
        temp_dir = Path(tempfile.mkdtemp(prefix="qemu_firmware_"))

        try:
            if firmware_path.is_file() and firmware_path.suffix == ".bin":
                # Copy single firmware file
                shutil.copy2(firmware_path, temp_dir / "firmware.bin")
                # Also create flash.bin for compatibility
                shutil.copy2(firmware_path, temp_dir / "flash.bin")
            else:
                # Copy entire build directory
                if firmware_path.is_dir():
                    # Copy relevant files from build directory
                    patterns = ["*.bin", "*.elf", "partitions.csv", "flash_args"]
                    for pattern in patterns:
                        for file in firmware_path.glob(pattern):
                            shutil.copy2(file, temp_dir)

                    # Also check for PlatformIO build structure
                    pio_build = firmware_path / ".pio" / "build" / "esp32dev"
                    if pio_build.exists():
                        for pattern in patterns:
                            for file in pio_build.glob(pattern):
                                shutil.copy2(file, temp_dir)

                    # Create flash.bin file that the QEMU script expects
                    firmware_bin = temp_dir / "firmware.bin"
                    if firmware_bin.exists():
                        # Copy firmware.bin to flash.bin for compatibility
                        shutil.copy2(firmware_bin, temp_dir / "flash.bin")
                    else:
                        # Create a minimal flash.bin for testing
                        flash_bin = temp_dir / "flash.bin"
                        flash_bin.write_bytes(
                            b"\xff" * (4 * 1024 * 1024)
                        )  # 4MB of 0xFF
                else:
                    raise ValueError(f"Invalid firmware path: {firmware_path}")

            return temp_dir
        except Exception as e:
            # Clean up on error
            shutil.rmtree(temp_dir, ignore_errors=True)
            raise e

    def build_qemu_command(
        self,
        firmware_name: str = "firmware.bin",
        flash_size: int = 4,
        machine: str = "esp32",
    ) -> List[str]:
        """Build QEMU command to run inside Docker container.

        Args:
            firmware_name: Name of firmware file in mounted directory
            flash_size: Flash size in MB
            machine: QEMU machine type (esp32, esp32c3, etc.)

        Returns:
            List of command arguments for QEMU
        """
        # For Linux containers, use a workaround to run QEMU through Docker Desktop's Windows integration
        firmware_path = f"{self.firmware_mount_path}/flash.bin"
        rom_path = "/opt/qemu/esp32-v3-rom.bin"

        # Use the container's built-in QEMU from ESP-IDF
        wrapper_script = f'''#!/bin/bash
set -e
echo "Starting ESP32 QEMU emulation..."
echo "Firmware: {firmware_path}"

# Check if firmware file exists
if [ ! -f "{firmware_path}" ]; then
    echo "ERROR: Firmware file not found: {firmware_path}"
    exit 1
fi

# Use ESP-IDF's QEMU which should be in the container
qemu-system-xtensa \\
    -nographic \\
    -machine esp32 \\
    -drive file="{firmware_path}",if=mtd,format=raw \\
    -global driver=timer.esp32.timg,property=wdt_disable,value=true \\
    -monitor none

echo "QEMU execution completed"
exit 0
'''

        cmd = ["bash", "-c", wrapper_script]

        return cmd


    def run(
        self,
        firmware_path: Path,
        timeout: int = 30,
        flash_size: int = 4,
        interrupt_regex: Optional[str] = None,
        interactive: bool = False,
    ) -> int:
        """Run ESP32 firmware in QEMU using Docker.

        Args:
            firmware_path: Path to firmware.bin or build directory
            timeout: Timeout in seconds
            flash_size: Flash size in MB
            interrupt_regex: Regex pattern to interrupt execution
            interactive: Run in interactive mode (attach to container)

        Returns:
            Exit code (0 for success)
        """
        if not self.check_docker_available():
            print("ERROR: Docker is not available or not running", file=sys.stderr)
            print("Please install Docker and ensure it's running", file=sys.stderr)
            return 1

        # Pull image if needed
        self.pull_image()

        # Prepare firmware files
        print(f"Preparing firmware from: {firmware_path}")
        temp_firmware_dir = None

        try:
            temp_firmware_dir = self.prepare_firmware(firmware_path)

            # Generate unique container name
            self.container_name = f"qemu_esp32_{int(time.time())}"

            # Convert Windows paths to Docker-compatible paths
            def windows_to_docker_path(path_str: Union[str, Path]) -> str:
                """Convert Windows path to Docker volume mount format."""
                import os

                # Check if we're in Git Bash/MSYS2 environment
                is_git_bash = (
                    "MSYSTEM" in os.environ
                    or os.environ.get("TERM") == "xterm"
                    or "bash.exe" in os.environ.get("SHELL", "")
                )

                if os.name == "nt" and is_git_bash:
                    # Convert C:\path\to\dir to /c/path/to/dir for Git Bash
                    path_str = str(path_str).replace("\\", "/")
                    if len(path_str) > 2 and path_str[1:3] == ":/":  # Drive letter
                        path_str = "/" + path_str[0].lower() + path_str[2:]
                else:
                    # For cmd.exe on Windows, keep native Windows paths
                    path_str = str(path_str)

                return path_str

            docker_firmware_path = windows_to_docker_path(temp_firmware_dir)

            # Only mount the firmware directory - QEMU is built into the container
            volumes = {
                docker_firmware_path: {
                    "bind": self.firmware_mount_path,
                    "mode": "ro",  # Read-only mount
                },
            }

            # Build QEMU command
            qemu_cmd = self.build_qemu_command(
                firmware_name="firmware.bin", flash_size=flash_size
            )

            print(f"Running QEMU in Docker container: {self.container_name}")
            print(f"QEMU command: {' '.join(qemu_cmd)}")

            if interactive:
                # Run interactively with streaming
                return self.docker_manager.run_container_streaming(
                    image_name=self.docker_image,
                    command=qemu_cmd,
                    volumes=volumes,
                    name=self.container_name,
                    interrupt_pattern=interrupt_regex,
                )
            else:
                # Run with streaming output and pattern detection
                return self.docker_manager.run_container_streaming(
                    image_name=self.docker_image,
                    command=qemu_cmd,
                    volumes=volumes,
                    name=self.container_name,
                    interrupt_pattern=interrupt_regex,
                )

        except Exception as e:
            print(f"ERROR: {e}", file=sys.stderr)
            return 1
        finally:
            # Cleanup
            if temp_firmware_dir and temp_firmware_dir.exists():
                shutil.rmtree(temp_firmware_dir, ignore_errors=True)

            # Ensure container is stopped and removed
            if self.container_name:
                try:
                    # Stop and remove container
                    run_subprocess_safe(
                        ["docker", "stop", self.container_name],
                        capture_output=True,
                        check=False,
                    )
                    run_subprocess_safe(
                        ["docker", "rm", self.container_name],
                        capture_output=True,
                        check=False,
                    )
                except:
                    pass


def main():
    parser = argparse.ArgumentParser(
        description="Run ESP32 firmware in QEMU using Docker"
    )
    parser.add_argument(
        "firmware_path", type=Path, help="Path to firmware.bin file or build directory"
    )
    parser.add_argument(
        "--docker-image",
        type=str,
        help=f"Docker image to use (default: {DockerQEMURunner.DEFAULT_IMAGE})",
    )
    parser.add_argument(
        "--flash-size", type=int, default=4, help="Flash size in MB (default: 4)"
    )
    parser.add_argument(
        "--timeout", type=int, default=30, help="Timeout in seconds (default: 30)"
    )
    parser.add_argument(
        "--interrupt-regex", type=str, help="Regex pattern to interrupt execution"
    )
    parser.add_argument(
        "--interactive", action="store_true", help="Run in interactive mode"
    )

    args = parser.parse_args()

    if not args.firmware_path.exists():
        print(
            f"ERROR: Firmware path does not exist: {args.firmware_path}",
            file=sys.stderr,
        )
        return 1

    runner = DockerQEMURunner(docker_image=args.docker_image)
    return runner.run(
        firmware_path=args.firmware_path,
        timeout=args.timeout,
        flash_size=args.flash_size,
        interrupt_regex=args.interrupt_regex,
        interactive=args.interactive,
    )


if __name__ == "__main__":
    sys.exit(main())
