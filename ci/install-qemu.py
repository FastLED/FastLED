#!/usr/bin/env python3
"""
Standalone QEMU Installation Script

This is a convenience wrapper around install_qemu_esp32.py
that can be called directly to install QEMU for ESP32 emulation.

Usage:
  uv run ci/install-qemu.py
"""

import os
import sys

from install_qemu_esp32 import main as install_qemu_main


def main():
    """Run the QEMU installation script."""
    print("=== FastLED QEMU Installation ===")
    print("Starting QEMU installation process...")
    print(f"Python executable: {sys.executable}")
    print(f"Working directory: {os.getcwd()}")
    print()

    try:
        # Call the main function directly
        print("Calling install_qemu_esp32.main()...")
        install_qemu_main()
        print("install_qemu_esp32.main() completed successfully")

    except KeyboardInterrupt:
        print("\nInstallation cancelled by user")
        sys.exit(130)
    except SystemExit as e:
        print(f"SystemExit caught with code: {e.code}")
        # Re-raise SystemExit to preserve exit codes
        raise
    except Exception as e:
        print(f"ERROR: Installation failed: {e}")
        import traceback

        print("Full traceback:")
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
