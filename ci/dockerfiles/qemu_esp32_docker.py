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
from ci.util.running_process import RunningProcess


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


def run_docker_command_streaming(cmd: List[str]) -> int:
    """Run docker command with streaming output using RunningProcess."""
    print(f"Executing: {' '.join(cmd)}")
    proc = RunningProcess(cmd, env=get_docker_env(), auto_run=True)

    # Stream all output to stdout
    with proc.line_iter(timeout=None) as it:
        for line in it:
            print(line)

    returncode = proc.wait()
    if returncode != 0:
        raise subprocess.CalledProcessError(returncode, cmd)
    return returncode


def run_docker_command_no_fail(cmd: List[str]) -> int:
    """Run docker command with streaming output, but don't raise exceptions on failure."""
    print(f"Executing: {' '.join(cmd)}")
    proc = RunningProcess(cmd, env=get_docker_env(), auto_run=True)

    # Stream all output to stdout
    with proc.line_iter(timeout=None) as it:
        for line in it:
            print(line)

    return proc.wait()


class DockerQEMURunner:
    """Runner for ESP32 QEMU emulation using Docker containers."""

    DEFAULT_IMAGE = "mluis/qemu-esp32:latest"
    ALTERNATIVE_IMAGE = "espressif/idf:latest"
    FALLBACK_IMAGE = "espressif/idf:release-v5.2"

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
            proc = RunningProcess(
                ["docker", "version"], env=get_docker_env(), auto_run=True
            )
            returncode = proc.wait(timeout=5)
            return returncode == 0
        except (Exception, FileNotFoundError):
            return False

    def pull_image(self):
        """Pull the Docker image if not already available."""
        print(f"Ensuring Docker image {self.docker_image} is available...")
        try:
            # Check if image exists locally
            proc = RunningProcess(
                ["docker", "images", "-q", self.docker_image],
                env=get_docker_env(),
                auto_run=True,
            )
            stdout_lines: List[str] = []
            with proc.line_iter(timeout=None) as it:
                for line in it:
                    stdout_lines.append(line)
            result_stdout = "\n".join(stdout_lines)
            if not result_stdout.strip():
                # Image doesn't exist, pull it directly using docker command
                print(f"Pulling Docker image: {self.docker_image}")
                run_docker_command_streaming(["docker", "pull", self.docker_image])
                print(f"Successfully pulled {self.docker_image}")
            else:
                print(f"Image {self.docker_image} already available locally")
        except subprocess.CalledProcessError as e:
            print(f"Warning: Failed to pull image {self.docker_image}: {e}")
            if self.docker_image == self.DEFAULT_IMAGE:
                print(f"Trying alternative image: {self.ALTERNATIVE_IMAGE}")
                self.docker_image = self.ALTERNATIVE_IMAGE
                try:
                    print(f"Pulling alternative Docker image: {self.docker_image}")
                    run_docker_command_streaming(["docker", "pull", self.docker_image])
                    print(f"Successfully pulled {self.docker_image}")
                except subprocess.CalledProcessError as e2:
                    print(f"Failed to pull alternative image: {e2}")
                    print(f"Trying fallback image: {self.FALLBACK_IMAGE}")
                    self.docker_image = self.FALLBACK_IMAGE
                    try:
                        print(f"Pulling fallback Docker image: {self.docker_image}")
                        run_docker_command_streaming(
                            ["docker", "pull", self.docker_image]
                        )
                        print(f"Successfully pulled {self.docker_image}")
                    except subprocess.CalledProcessError as e3:
                        print(f"Failed to pull fallback image: {e3}")
                        raise e3
            else:
                raise e

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
                # Create proper 4MB flash image for QEMU
                self._create_flash_image(firmware_path, temp_dir / "flash.bin")
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

                    # Create proper flash.bin file from firmware.bin
                    firmware_bin = temp_dir / "firmware.bin"
                    if firmware_bin.exists():
                        # Create proper 4MB flash image for QEMU
                        self._create_flash_image(firmware_bin, temp_dir / "flash.bin")
                    else:
                        # Create a minimal 4MB flash.bin for testing
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

    def _create_flash_image(
        self, firmware_path: Path, output_path: Path, flash_size_mb: int = 4
    ):
        """Create a proper flash image for QEMU ESP32.

        Args:
            firmware_path: Path to the firmware.bin file
            output_path: Path where to write the flash.bin
            flash_size_mb: Flash size in MB (must be 2, 4, 8, or 16)
        """
        if flash_size_mb not in [2, 4, 8, 16]:
            raise ValueError(
                f"Flash size must be 2, 4, 8, or 16 MB, got {flash_size_mb}"
            )

        flash_size = flash_size_mb * 1024 * 1024

        # Read firmware content
        firmware_data = firmware_path.read_bytes()

        # Create flash image: firmware at beginning, rest filled with 0xFF
        flash_data = firmware_data + b"\xff" * (flash_size - len(firmware_data))

        # Ensure we have exactly the right size
        if len(flash_data) > flash_size:
            raise ValueError(
                f"Firmware size ({len(firmware_data)} bytes) exceeds flash size ({flash_size} bytes)"
            )

        flash_data = flash_data[:flash_size]  # Truncate to exact size

        output_path.write_bytes(flash_data)

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
            machine: QEMU machine type (esp32, esp32c3, esp32s3, etc.)

        Returns:
            List of command arguments for QEMU
        """
        # For Linux containers, use a workaround to run QEMU through Docker Desktop's Windows integration
        firmware_path = f"{self.firmware_mount_path}/flash.bin"

        # Determine QEMU system and machine based on target
        if machine == "esp32c3":
            qemu_system = "/usr/local/bin/qemu-system-riscv32"
            qemu_machine = "esp32c3"
            echo_target = "ESP32C3"
        elif machine == "esp32s3":
            qemu_system = "/usr/local/bin/qemu-system-xtensa"
            qemu_machine = "esp32s3"
            echo_target = "ESP32S3"
        else:
            # Default to ESP32 (Xtensa)
            qemu_system = "/usr/local/bin/qemu-system-xtensa"
            qemu_machine = "esp32"
            echo_target = "ESP32"

        # Use container's QEMU with proper configuration
        wrapper_script = f'''#!/bin/bash
set -e
echo "Starting {echo_target} QEMU emulation..."
echo "Firmware: {firmware_path}"
echo "Machine: {qemu_machine}"
echo "QEMU system: {qemu_system}"
echo "Container: $(cat /etc/os-release | head -1)"

# Check if firmware file exists
if [ ! -f "{firmware_path}" ]; then
    echo "ERROR: Firmware file not found: {firmware_path}"
    exit 1
fi

# Check firmware size
FIRMWARE_SIZE=$(stat -c%s "{firmware_path}")
echo "Firmware size: $FIRMWARE_SIZE bytes"

# Copy firmware to writable location since QEMU needs write access
cp "{firmware_path}" /tmp/flash.bin
echo "Copied firmware to writable location: /tmp/flash.bin"

# Try different QEMU configurations depending on machine type
if [ "{qemu_machine}" = "esp32c3" ]; then
    # ESP32C3 uses RISC-V architecture
    echo "Running {qemu_system} for {qemu_machine}"
    {qemu_system} \\
        -nographic \\
        -machine {qemu_machine} \\
        -drive file="/tmp/flash.bin",if=mtd,format=raw \\
        -monitor none \\
        -serial mon:stdio
else
    # ESP32 uses Xtensa architecture
    echo "Running {qemu_system} for {qemu_machine}"
    {qemu_system} \\
        -nographic \\
        -machine {qemu_machine} \\
        -drive file="/tmp/flash.bin",if=mtd,format=raw \\
        -global driver=timer.esp32.timg,property=wdt_disable,value=true \\
        -monitor none \\
        -serial mon:stdio
fi

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
        output_file: Optional[str] = None,
        machine: str = "esp32",
    ) -> int:
        """Run ESP32/ESP32C3/ESP32S3 firmware in QEMU using Docker.

        Args:
            firmware_path: Path to firmware.bin or build directory
            timeout: Timeout in seconds (timeout is treated as success)
            flash_size: Flash size in MB
            interrupt_regex: Regex pattern to detect in output (informational only)
            interactive: Run in interactive mode (attach to container)
            output_file: Optional file path to write QEMU output to
            machine: QEMU machine type (esp32, esp32c3, esp32s3, etc.)

        Returns:
            Exit code: actual QEMU/container exit code, except timeout returns 0
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
                firmware_name="firmware.bin", flash_size=flash_size, machine=machine
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
                    timeout=timeout,
                    interrupt_pattern=interrupt_regex,
                    output_file=output_file,
                )
            else:
                # Run with streaming output and return actual exit code
                return self.docker_manager.run_container_streaming(
                    image_name=self.docker_image,
                    command=qemu_cmd,
                    volumes=volumes,
                    name=self.container_name,
                    timeout=timeout,
                    interrupt_pattern=interrupt_regex,
                    output_file=output_file,
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
                    run_docker_command_no_fail(["docker", "stop", self.container_name])
                    run_docker_command_no_fail(["docker", "rm", self.container_name])
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
    parser.add_argument("--output-file", type=str, help="File to write QEMU output to")

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
        output_file=args.output_file,
    )


if __name__ == "__main__":
    sys.exit(main())
