#!/usr/bin/env python3
"""
Docker-based QEMU ESP32 Runner

Runs ESP32 firmware in QEMU using Docker containers for better isolation
and consistency across different environments.
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Optional


# Add parent directory to path to import DockerManager
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from ci.dockerfiles.DockerManager import DockerManager


class DockerQEMURunner:
    """Runner for ESP32 QEMU emulation using Docker containers."""

    DEFAULT_IMAGE = "espressif/qemu:esp-develop-8.2.0-20240122"
    ALTERNATIVE_IMAGE = "mluis/qemu-esp32:latest"

    def __init__(self, docker_image: Optional[str] = None):
        """Initialize Docker QEMU runner.

        Args:
            docker_image: Docker image to use, defaults to espressif/qemu
        """
        self.docker_manager = DockerManager()
        self.docker_image = docker_image or self.DEFAULT_IMAGE
        self.container_name = None
        self.firmware_mount_path = "/workspace/firmware"

    def check_docker_available(self) -> bool:
        """Check if Docker is available and running."""
        try:
            result = subprocess.run(
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
            result = subprocess.run(
                ["docker", "images", "-q", self.docker_image],
                capture_output=True,
                text=True,
            )
            if not result.stdout.strip():
                # Image doesn't exist, pull it
                self.docker_manager.pull_image(self.docker_image)
            else:
                print(f"Image {self.docker_image} already available locally")
        except subprocess.CalledProcessError as e:
            print(f"Warning: Failed to pull image {self.docker_image}: {e}")
            if self.docker_image != self.ALTERNATIVE_IMAGE:
                print(f"Trying alternative image: {self.ALTERNATIVE_IMAGE}")
                self.docker_image = self.ALTERNATIVE_IMAGE
                self.docker_manager.pull_image(self.docker_image)

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
        # Path inside container
        firmware_path = f"{self.firmware_mount_path}/{firmware_name}"

        # Build QEMU command
        cmd = [
            "qemu-system-xtensa",
            "-nographic",
            "-machine",
            machine,
            "-drive",
            f"file={firmware_path},if=mtd,format=raw",
            "-global",
            "driver=timer.esp32.timg,property=wdt_disable,value=true",
        ]

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

            # Prepare volumes
            volumes = {
                str(temp_firmware_dir): {
                    "bind": self.firmware_mount_path,
                    "mode": "ro",  # Read-only mount
                }
            }

            # Build QEMU command
            qemu_cmd = self.build_qemu_command(
                firmware_name="firmware.bin", flash_size=flash_size
            )

            print(f"Running QEMU in Docker container: {self.container_name}")
            print(f"QEMU command: {' '.join(qemu_cmd)}")

            if interactive:
                # Run interactively
                docker_cmd = [
                    "docker",
                    "run",
                    "-it",
                    "--rm",
                    "--name",
                    self.container_name,
                    "-v",
                    f"{temp_firmware_dir}:{self.firmware_mount_path}:ro",
                    self.docker_image,
                ] + qemu_cmd

                result = subprocess.run(docker_cmd)
                return result.returncode
            else:
                # Run in background and capture output
                container_id = self.docker_manager.run_container(
                    image_name=self.docker_image,
                    command=qemu_cmd,
                    volumes=volumes,
                    detach=True,
                    name=self.container_name,
                )

                # Monitor output
                start_time = time.time()
                output_lines: List[str] = []
                interrupt_met = False

                while True:
                    # Check if timeout exceeded
                    if time.time() - start_time > timeout:
                        print(
                            f"ERROR: Timeout after {timeout} seconds", file=sys.stderr
                        )
                        self.docker_manager.stop_container(self.container_name)
                        return 1

                    # Get logs
                    logs = self.docker_manager.get_container_logs(self.container_name)
                    new_lines = logs.split("\n")[len(output_lines) :]

                    for line in new_lines:
                        if line:
                            print(line)
                            output_lines.append(line)

                            # Check for interrupt pattern
                            if interrupt_regex and interrupt_regex in line:
                                print(f"SUCCESS: Interrupt condition met: {line}")
                                interrupt_met = True
                                break

                    if interrupt_met:
                        break

                    # Check if container is still running
                    result = subprocess.run(
                        ["docker", "ps", "-q", "-f", f"name={self.container_name}"],
                        capture_output=True,
                        text=True,
                    )
                    if not result.stdout.strip():
                        # Container has stopped
                        break

                    time.sleep(0.5)

                # Stop container if still running
                try:
                    self.docker_manager.stop_container(self.container_name)
                except subprocess.CalledProcessError:
                    pass  # Container may have already stopped

                # Save output to file
                log_file = Path("qemu_docker_output.log")
                log_file.write_text("\n".join(output_lines))
                print(f"Output saved to: {log_file}")

                return 0 if interrupt_met else 1

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
                    subprocess.run(
                        ["docker", "rm", "-f", self.container_name],
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
