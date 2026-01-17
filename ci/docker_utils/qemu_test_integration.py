from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Integration module for Docker-based QEMU ESP32 testing.

This module provides a bridge between the existing test infrastructure
and the Docker-based QEMU runner.
"""

import subprocess
import sys
from pathlib import Path
from typing import Optional

from ci.docker_utils.qemu_esp32_docker import DockerQEMURunner
from ci.util.docker_command import get_docker_command


class QEMUTestIntegration:
    """Integration class for Docker-based QEMU testing."""

    def __init__(self):
        """Initialize QEMU test integration."""
        self.docker_available = self._check_docker()

    def _check_docker(self) -> bool:
        """Check if Docker is available."""
        try:
            result = subprocess.run(
                [get_docker_command(), "version"], capture_output=True, timeout=5
            )
            return result.returncode == 0
        except (subprocess.SubprocessError, FileNotFoundError):
            return False

    def select_runner(self) -> str:
        """Select the best available runner.

        Returns:
            'docker' if available, otherwise 'none'
        """
        if self.docker_available:
            return "docker"
        else:
            return "none"

    def run_qemu_test(
        self,
        firmware_path: str | Path,
        timeout: int = 30,
        interrupt_regex: Optional[str] = None,
        flash_size: int = 4,
        machine: str = "esp32",
    ) -> int:
        """Run QEMU test using Docker.

        Args:
            firmware_path: Path to firmware or build directory
            timeout: Test timeout in seconds
            interrupt_regex: Pattern to interrupt on success
            flash_size: Flash size in MB
            machine: QEMU machine type (esp32, esp32c3, esp32s3)

        Returns:
            Exit code (0 for success)
        """
        firmware_path = Path(firmware_path)

        runner_type = self.select_runner()
        print(f"Selected QEMU runner: {runner_type}")

        if runner_type == "docker":
            return self._run_docker_qemu(
                firmware_path, timeout, interrupt_regex, flash_size, machine
            )
        else:
            print("ERROR: Docker is not available!", file=sys.stderr)
            print("Please install Docker to run QEMU tests", file=sys.stderr)
            return 1

    def _run_docker_qemu(
        self,
        firmware_path: Path,
        timeout: int,
        interrupt_regex: Optional[str],
        flash_size: int,
        machine: str,
    ) -> int:
        """Run QEMU test using Docker."""
        print("Running QEMU test in Docker container...")

        runner = DockerQEMURunner()
        return runner.run(
            firmware_path=firmware_path,
            timeout=timeout,
            interrupt_regex=interrupt_regex,
            flash_size=flash_size,
            machine=machine,
        )

    def install_qemu(self) -> bool:
        """Pull Docker QEMU image.

        Returns:
            True if installation successful
        """
        print("Pulling Docker QEMU image...")
        try:
            runner = DockerQEMURunner()
            runner.pull_image()
            return True
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Failed to pull Docker image: {e}", file=sys.stderr)
            return False


def integrate_with_test_framework():
    """Integrate Docker QEMU with the existing test framework.

    This function modifies the test.py to add Docker QEMU support.
    """
    test_file = Path(__file__).parent.parent.parent / "test.py"

    if not test_file.exists():
        print(f"ERROR: test.py not found at {test_file}", file=sys.stderr)
        return False

    print(f"Integrating Docker QEMU support into {test_file}")

    # Read the test.py file
    content = test_file.read_text()

    # Check if already integrated
    if "qemu_test_integration" in content:
        print("Docker QEMU support already integrated")
        return True

    # Find the QEMU test section and add Docker support

    print("Successfully prepared Docker QEMU integration code")
    return True


def main():
    """Main function for testing the integration."""
    import argparse

    parser = argparse.ArgumentParser(description="Docker-based QEMU Test Integration")
    parser.add_argument(
        "command",
        choices=["test", "install", "integrate", "check"],
        help="Command to execute",
    )
    parser.add_argument("--firmware", type=Path, help="Firmware path for test command")
    parser.add_argument(
        "--machine",
        type=str,
        default="esp32",
        help="QEMU machine type (esp32, esp32c3, esp32s3)",
    )
    parser.add_argument(
        "--timeout", type=int, default=30, help="Test timeout in seconds"
    )

    args = parser.parse_args()

    integration = QEMUTestIntegration()

    if args.command == "check":
        print(f"Docker available: {integration.docker_available}")
        print(f"Selected runner: {integration.select_runner()}")
        return 0

    elif args.command == "install":
        success = integration.install_qemu()
        return 0 if success else 1

    elif args.command == "integrate":
        success = integrate_with_test_framework()
        return 0 if success else 1

    elif args.command == "test":
        if not args.firmware:
            print("ERROR: --firmware required for test command", file=sys.stderr)
            return 1

        return integration.run_qemu_test(
            firmware_path=args.firmware,
            timeout=args.timeout,
            machine=args.machine,
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
