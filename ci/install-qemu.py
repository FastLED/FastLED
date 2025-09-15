#!/usr/bin/env python3
"""
Standalone QEMU Installation Script

This is a convenience wrapper around install-qemu-esp32.py
that can be called directly to install QEMU for ESP32 emulation.

Usage:
  uv run ci/install-qemu.py
"""

import subprocess
import sys
from pathlib import Path


def main():
    """Run the QEMU installation script."""
    # Script is now in ci/ directory alongside install-qemu-esp32.py
    script_dir = Path(__file__).parent
    install_script = script_dir / "install-qemu-esp32.py"

    if not install_script.exists():
        print(f"Error: Installation script not found at {install_script}")
        sys.exit(1)

    print("=== FastLED QEMU Installation ===")
    print(f"Running installation script: {install_script}")
    print()

    try:
        # Run the actual installation script
        result = subprocess.run(
            ["python", str(install_script)],
            timeout=600,  # 10 minute timeout
        )

        sys.exit(result.returncode)

    except subprocess.TimeoutExpired:
        print("ERROR: QEMU installation timed out after 10 minutes")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nInstallation cancelled by user")
        sys.exit(130)
    except Exception as e:
        print(f"ERROR: Installation failed: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
