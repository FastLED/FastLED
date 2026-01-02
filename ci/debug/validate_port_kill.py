#!/usr/bin/env python3
"""Validation script: Demonstrates port-specific process killing.

This script shows the new functionality added to ci/debug_attached.py:
1. Auto-detection of ESP32/Arduino ports
2. Detection of locked ports
3. Surgical killing of processes holding specific ports

Usage:
    uv run python ci/debug/validate_port_kill.py                # Check all ports
    uv run python ci/debug/validate_port_kill.py COM3           # Check specific port
    uv run python ci/debug/validate_port_kill.py --kill COM3    # Kill process using COM3
"""

import argparse
import sys

import serial.tools.list_ports

from ci.util.port_utils import ComportResult, auto_detect_upload_port, kill_port_users


def list_all_ports() -> list[str]:
    """List all available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found")
        return []

    print("Available serial ports:")
    for port in ports:
        print(f"  {port.device}")
        print(f"    Description: {port.description}")
        print(f"    Hardware ID: {port.hwid}")
        print()

    return [port.device for port in ports]


def check_port_status(port_name: str) -> bool:
    """Check if a port is locked."""
    import serial

    try:
        ser = serial.Serial(port_name, baudrate=115200, timeout=1)
        ser.close()
        print(f"✅ {port_name}: Available (not locked)")
        return False
    except serial.SerialException as e:
        error_msg = str(e)
        if (
            "PermissionError" in error_msg
            or "Access is denied" in error_msg
            or "in use" in error_msg.lower()
        ):
            print(f"🔒 {port_name}: LOCKED")
            print(f"   Error: {error_msg}")
            return True
        else:
            print(f"❌ {port_name}: Error - {error_msg}")
            return False


def main():
    """Main validation function."""
    parser = argparse.ArgumentParser(description="Validate port killing functionality")
    parser.add_argument("port", nargs="?", help="Specific port to check (e.g., COM3)")
    parser.add_argument(
        "--kill", action="store_true", help="Kill process using the port"
    )
    parser.add_argument("--auto", action="store_true", help="Auto-detect port")
    args = parser.parse_args()

    print("=" * 60)
    print("PORT KILL VALIDATION")
    print("=" * 60)
    print()

    detected: ComportResult | None = None
    if args.auto or not args.port:
        print("Auto-detecting upload port...")
        detected = auto_detect_upload_port()
        if detected and detected.ok:
            print(f"✅ Auto-detected: {detected}")
            print()
        else:
            print("❌ No suitable port detected")
            print()

    if not args.port and not args.auto:
        # Just list all ports
        list_all_ports()
        print("Tip: Run with --auto to auto-detect ESP32/Arduino port")
        print("Tip: Run with a port name (e.g., COM3) to check that specific port")
        return

    port_to_check = args.port or (detected.selected_port if detected else None)
    if not port_to_check:
        print("No port specified or detected")
        return

    print(f"Checking port: {port_to_check}")
    print("-" * 60)

    is_locked = check_port_status(port_to_check)
    print()

    if is_locked and args.kill:
        print("Attempting to kill process using port...")
        print("-" * 60)
        kill_port_users(port_to_check)
        print()

        print(f"✅ Port cleanup completed for {port_to_check}")
        print()
        print("Re-checking port status...")
        print("-" * 60)
        check_port_status(port_to_check)
    elif is_locked:
        print("Port is locked. Run with --kill to attempt killing the process.")
        print(f"Example: uv run python {sys.argv[0]} {port_to_check} --kill")

    print()
    print("=" * 60)
    print("VALIDATION COMPLETE")
    print("=" * 60)


if __name__ == "__main__":
    main()
