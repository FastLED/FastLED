#!/usr/bin/env python3
"""
Standalone script to install QEMU for ESP32 emulation.

This script pulls the Docker QEMU image required for ESP32 testing.
"""

import sys
from pathlib import Path


# Add parent directory to path to import ci modules
sys.path.insert(0, str(Path(__file__).parent.parent))

from ci.docker_utils.qemu_test_integration import QEMUTestIntegration


def main() -> int:
    """Install QEMU Docker image.

    Returns:
        Exit code (0 for success, 1 for failure)
    """
    print("FastLED QEMU Installation")
    print("=" * 50)
    print()

    integration = QEMUTestIntegration()

    if not integration.docker_available:
        print("ERROR: Docker is not available!", file=sys.stderr)
        print("Please install Docker first:", file=sys.stderr)
        print("  - Windows/Mac: https://www.docker.com/products/docker-desktop")
        print("  - Linux: https://docs.docker.com/engine/install/")
        return 1

    print("Docker is available, pulling QEMU image...")
    success = integration.install_qemu()

    if success:
        print()
        print("✓ QEMU installation successful!")
        print()
        print("You can now run QEMU tests with:")
        print("  uv run test.py --qemu esp32s3")
        return 0
    else:
        print()
        print("✗ QEMU installation failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
