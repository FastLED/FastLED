#!/usr/bin/env python3
"""
Integration module for Docker-based QEMU ESP32 testing.

This module provides a bridge between the existing test infrastructure
and the Docker-based QEMU runner.
"""

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional, Union

from ci.dockerfiles.qemu_esp32_docker import DockerQEMURunner


class QEMUTestIntegration:
    """Integration class for QEMU testing with Docker fallback."""

    def __init__(self, prefer_docker: bool = False):
        """Initialize QEMU test integration.

        Args:
            prefer_docker: If True, prefer Docker even if native QEMU is available
        """
        self.prefer_docker = prefer_docker
        self.docker_available = self._check_docker()
        self.native_qemu_available = self._check_native_qemu()

    def _check_docker(self) -> bool:
        """Check if Docker is available."""
        try:
            result = subprocess.run(
                ["docker", "version"], capture_output=True, timeout=5
            )
            return result.returncode == 0
        except (subprocess.SubprocessError, FileNotFoundError):
            return False

    def _check_native_qemu(self) -> bool:
        """Check if native QEMU ESP32 is available."""
        try:
            # Import the native QEMU module to check
            from ci.qemu_esp32 import find_qemu_binary  # type: ignore[import-untyped]

            return find_qemu_binary() is not None
        except ImportError:
            return False

    def select_runner(self) -> str:
        """Select the best available runner.

        Returns:
            'docker' or 'native' based on availability and preference
        """
        if self.prefer_docker and self.docker_available:
            return "docker"
        elif self.native_qemu_available:
            return "native"
        elif self.docker_available:
            return "docker"
        else:
            return "none"

    def run_qemu_test(
        self,
        firmware_path: Union[str, Path],
        timeout: int = 30,
        interrupt_regex: Optional[str] = None,
        flash_size: int = 4,
        force_runner: Optional[str] = None,
    ) -> int:
        """Run QEMU test with automatic runner selection.

        Args:
            firmware_path: Path to firmware or build directory
            timeout: Test timeout in seconds
            interrupt_regex: Pattern to interrupt on success
            flash_size: Flash size in MB
            force_runner: Force specific runner ('docker' or 'native')

        Returns:
            Exit code (0 for success)
        """
        firmware_path = Path(firmware_path)

        # Determine which runner to use
        if force_runner:
            runner_type = force_runner
        else:
            runner_type = self.select_runner()

        print(f"Selected QEMU runner: {runner_type}")

        if runner_type == "docker":
            return self._run_docker_qemu(
                firmware_path, timeout, interrupt_regex, flash_size
            )
        elif runner_type == "native":
            return self._run_native_qemu(
                firmware_path, timeout, interrupt_regex, flash_size
            )
        else:
            print("ERROR: No QEMU runner available!", file=sys.stderr)
            print("Install Docker or native QEMU ESP32", file=sys.stderr)
            return 1

    def _run_docker_qemu(
        self,
        firmware_path: Path,
        timeout: int,
        interrupt_regex: Optional[str],
        flash_size: int,
    ) -> int:
        """Run QEMU test using Docker."""
        print("Running QEMU test in Docker container...")

        runner = DockerQEMURunner()
        return runner.run(
            firmware_path=firmware_path,
            timeout=timeout,
            interrupt_regex=interrupt_regex,
            flash_size=flash_size,
        )

    def _run_native_qemu(
        self,
        firmware_path: Path,
        timeout: int,
        interrupt_regex: Optional[str],
        flash_size: int,
    ) -> int:
        """Run QEMU test using native installation."""
        print("Running QEMU test with native installation...")

        # Import and use the native runner
        from ci.qemu_esp32 import QEMURunner  # type: ignore[import-untyped]

        runner = QEMURunner()  # type: ignore[no-untyped-call]
        return runner.run(  # type: ignore[no-untyped-call]
            firmware_path=firmware_path,
            timeout=timeout,
            interrupt_regex=interrupt_regex,
            flash_size=flash_size,
        )

    def install_qemu(self, use_docker: bool = False) -> bool:
        """Install QEMU (native or pull Docker image).

        Args:
            use_docker: If True, pull Docker image instead of native install

        Returns:
            True if installation successful
        """
        if use_docker:
            print("Pulling Docker QEMU image...")
            try:
                runner = DockerQEMURunner()
                runner.pull_image()
                return True
            except Exception as e:
                print(f"Failed to pull Docker image: {e}", file=sys.stderr)
                return False
        else:
            print("Installing native QEMU...")
            try:
                # Run the native install script
                result = subprocess.run(
                    [sys.executable, "ci/install-qemu.py"],
                    cwd=Path(__file__).parent.parent.parent,
                )
                return result.returncode == 0
            except Exception as e:
                print(f"Failed to install native QEMU: {e}", file=sys.stderr)
                return False


def integrate_with_test_framework():
    """Integrate Docker QEMU with the existing test framework.

    This function modifies the test.py to add Docker support.
    """
    test_file = Path(__file__).parent.parent.parent / "test.py"

    if not test_file.exists():
        print(f"ERROR: test.py not found at {test_file}", file=sys.stderr)
        return False

    print(f"Integrating Docker QEMU support into {test_file}")

    # Read the test.py file
    content = test_file.read_text()

    # Check if already integrated
    if "docker.qemu_test_integration" in content:
        print("Docker QEMU support already integrated")
        return True

    # Find the QEMU test section and add Docker support
    integration_code = """
# Docker QEMU integration
try:
    from ci.dockerfiles.qemu_test_integration import QEMUTestIntegration
    DOCKER_QEMU_AVAILABLE = True
except ImportError:
    DOCKER_QEMU_AVAILABLE = False
"""

    # Add the import at the top of the file after other imports
    import_marker = "from ci.qemu_esp32 import"
    if import_marker in content:
        # Add after the existing QEMU import
        content = content.replace(
            import_marker, integration_code + "\n" + import_marker
        )

        # Save the modified file
        test_file.write_text(content)
        print("Successfully integrated Docker QEMU support")
        return True
    else:
        print("WARNING: Could not find appropriate location to add integration")
        return False


def main():
    """Main function for testing the integration."""
    import argparse

    parser = argparse.ArgumentParser(
        description="QEMU Test Integration with Docker support"
    )
    parser.add_argument(
        "command",
        choices=["test", "install", "integrate", "check"],
        help="Command to execute",
    )
    parser.add_argument("--firmware", type=Path, help="Firmware path for test command")
    parser.add_argument(
        "--docker", action="store_true", help="Prefer Docker over native QEMU"
    )
    parser.add_argument(
        "--timeout", type=int, default=30, help="Test timeout in seconds"
    )

    args = parser.parse_args()

    integration = QEMUTestIntegration(prefer_docker=args.docker)

    if args.command == "check":
        print(f"Docker available: {integration.docker_available}")
        print(f"Native QEMU available: {integration.native_qemu_available}")
        print(f"Selected runner: {integration.select_runner()}")
        return 0

    elif args.command == "install":
        success = integration.install_qemu(use_docker=args.docker)
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
            force_runner="docker" if args.docker else None,
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
