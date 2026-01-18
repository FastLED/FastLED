from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""Serial port detection and management utilities.

This module provides utilities for detecting serial ports, identifying USB devices,
and cleaning up orphaned processes that may be holding serial port locks.

Key features:
    - Auto-detection of USB serial ports (filters out Bluetooth, ACPI, etc.)
    - Preference for common ESP32/Arduino USB-to-serial chips
    - ESP32 chip type detection using esptool
    - Chip-to-environment mapping for auto-detection
    - Safe process cleanup (never kills current process or parents)
    - Detailed logging of port cleanup operations
"""

import datetime
import os
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import psutil
import serial.tools.list_ports
from psutil import Process
from serial.tools.list_ports_common import ListPortInfo


# Mapping from detected ESP chip type to PlatformIO environment
CHIP_TO_ENVIRONMENT: dict[str, str] = {
    "ESP32-S3": "esp32s3",
    "ESP32-C6": "esp32c6",
    "ESP32-C3": "esp32c3",
    "ESP32-C2": "esp32c2",
    "ESP32-H2": "esp32h2",
    "ESP32": "esp32dev",  # Generic ESP32 (original)
    "ESP8266": "esp8266",  # ESP8266
}


@dataclass
class ComportResult:
    """Result of COM port detection with detailed diagnostics.

    Attributes:
        ok: True if a suitable USB port was found, False otherwise
        selected_port: The selected USB port device name (e.g., 'COM3'), or None if not found
        error_message: Optional error description if detection failed
        all_ports: List of all ports scanned (for diagnostics), empty list by default
    """

    ok: bool
    selected_port: str | None
    error_message: str | None = None
    all_ports: list[ListPortInfo] = field(default_factory=lambda: [])


def auto_detect_upload_port() -> ComportResult:
    """Auto-detect the upload port from available serial ports.

    Only considers USB devices - filters out Bluetooth and other non-USB ports.
    Returns a structured result with success status, selected port, error details,
    and all scanned ports for diagnostics.

    Prefers common ESP32/Arduino USB-to-serial chips:
        - CP2102/CP210x (Silicon Labs)
        - CH340/CH341 (WCH)
        - FTDI chips
        - Generic USB-Serial converters

    Returns:
        ComportResult with detection status and diagnostics
    """
    try:
        # Scan all available COM ports
        all_ports = list(serial.tools.list_ports.comports())
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        # Failed to scan ports - return error result
        return ComportResult(
            ok=False,
            selected_port=None,
            error_message=f"Failed to scan COM ports: {e}",
            all_ports=[],
        )

    if not all_ports:
        # No ports found at all
        return ComportResult(
            ok=False,
            selected_port=None,
            error_message="No serial ports detected on system",
            all_ports=[],
        )

    # Filter to USB devices only (exclude Bluetooth, ACPI, etc.)
    # USB devices have "USB" in their hwid (hardware ID) string
    usb_ports = [port for port in all_ports if "USB" in port.hwid.upper()]

    if not usb_ports:
        # No USB ports found - only non-USB ports available
        non_usb_types: set[str] = set()
        for port in all_ports:
            if "BTHENUM" in port.hwid.upper():
                non_usb_types.add("Bluetooth")
            elif "ACPI" in port.hwid.upper():
                non_usb_types.add("ACPI")
            else:
                non_usb_types.add("other")

        types_str = ", ".join(sorted(non_usb_types))
        return ComportResult(
            ok=False,
            selected_port=None,
            error_message=f"No USB serial ports found (only {types_str} ports detected)",
            all_ports=all_ports,
        )

    # Select best USB port (prefer ESP32/Arduino chips if available)
    selected_port = None
    for port in usb_ports:
        description = port.description.lower()
        # Common ESP32 USB chips: CP2102, CH340, FTDI
        if any(
            chip in description
            for chip in ["cp210", "ch340", "ch341", "ftdi", "usb-serial", "uart"]
        ):
            selected_port = port.device
            break

    # If no ESP-specific chip found, use first USB port
    if not selected_port:
        selected_port = usb_ports[0].device

    return ComportResult(
        ok=True, selected_port=selected_port, error_message=None, all_ports=all_ports
    )


def log_port_cleanup(message: str, log_dir: Path | None = None) -> None:
    """Log port cleanup operations to debug log file.

    Args:
        message: Log message to write
        log_dir: Optional custom log directory (default: project_root/.logs/debug_attached)
    """
    try:
        # Default to project root .logs directory if not specified
        if log_dir is None:
            # Assume this module is in ci/util/, so project root is two levels up
            project_root = Path(__file__).parent.parent.parent
            log_dir = project_root / ".logs" / "debug_attached"

        log_file = log_dir / "port_cleanup.log"

        # Create directory if it doesn't exist
        log_dir.mkdir(parents=True, exist_ok=True)

        # Append log entry with timestamp
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(f"[{timestamp}] {message}\n")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        # Silently ignore logging errors - don't fail the operation
        pass


def kill_port_users(port: str) -> None:
    """Kill processes holding the serial port.

    ⚠️ CRITICAL: MUST BE CALLED INSIDE DAEMON LOCK.
    Only kills orphaned processes from crashed sessions.
    Assumes caller has exclusive daemon lock (any port user is an orphan).

    This function uses process name and command-line pattern matching
    (not open file handles) because on Windows, COM ports don't typically
    show up as file handles via psutil.Process.open_files().

    Safety features:
        - Never kills current process or any parent process
        - Never kills Python processes (could be agent backend)
        - Only kills known serial tools (pio, esptool, miniterm, etc.)
        - Logs all actions for auditing

    Args:
        port: Serial port name (e.g., "COM4", "/dev/ttyUSB0")
    """
    port_lower = port.lower()
    processes_to_kill: list[tuple[Process, str, list[str]]] = []

    # Get current process and its entire process tree (ancestors)
    # We must NEVER kill ourselves or any parent process
    current_pid = os.getpid()
    protected_pids = {current_pid}

    try:
        current_proc = psutil.Process(current_pid)
        # Walk up the process tree and protect all ancestors
        parent = current_proc.parent()
        while parent:
            protected_pids.add(parent.pid)
            parent = parent.parent()
    except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass

    # Define patterns for serial port users
    # Only kill dedicated serial tools - NEVER kill Python processes
    # (Python could be the agent backend: clud, claude, etc.)
    safe_serial_exes = [
        "pio.exe",
        "pio",
        "esptool.exe",
        "esptool",
        "platformio.exe",
        "platformio",
        "miniterm.exe",
        "miniterm",
        "putty.exe",
        "putty",
        "teraterm.exe",
        "teraterm",
        "screen",  # Unix serial terminal
        "cu",  # Unix serial terminal
    ]

    cmdline_patterns = [
        "pio monitor",
        "pio device monitor",
        "device monitor",
        "miniterm",
        "esptool",
        port_lower,
    ]

    # Find processes using the port
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            proc_info: dict[str, Any] = proc.as_dict(attrs=["pid", "name", "cmdline"])  # type: ignore[assignment]
            proc_pid: int = proc_info["pid"]
            proc_name: str = proc_info["name"]
            cmdline: list[str] | None = proc_info["cmdline"]
            proc_name_lower = proc_name.lower()

            # NEVER kill ourselves or parent processes
            if proc_pid in protected_pids:
                continue

            # NEVER kill Python processes (could be agent backend)
            if "python" in proc_name_lower:
                continue

            # Check dedicated serial tools (safe to kill)
            if any(exe in proc_name_lower for exe in safe_serial_exes):
                if cmdline:
                    cmdline_str = " ".join(cmdline).lower()
                    # Check if command line contains port or serial patterns
                    if any(pattern in cmdline_str for pattern in cmdline_patterns):
                        processes_to_kill.append(
                            (proc, proc_name, cmdline)
                        )  # cmdline is guaranteed non-None here

        except (psutil.NoSuchProcess, psutil.AccessDenied):
            # Process may have terminated or access denied
            pass

    # Kill identified processes
    if processes_to_kill:
        log_port_cleanup(
            f"Port cleanup started for {port}: Found {len(processes_to_kill)} orphaned process(es)"
        )
        print(
            f"🔪 Cleaning up {len(processes_to_kill)} orphaned process(es) using {port}:"
        )
        for proc, proc_name, cmdline in processes_to_kill:
            try:
                cmdline_str = " ".join(cmdline) if cmdline else "<no cmdline>"
                # Truncate long command lines
                if len(cmdline_str) > 80:
                    cmdline_str = cmdline_str[:77] + "..."
                print(f"   → Killing {proc_name} (PID {proc.pid})")
                print(f"      Command: {cmdline_str}")
                proc.kill()
                log_port_cleanup(
                    f"Killed process: {proc_name} (PID {proc.pid}) - Command: {cmdline_str}"
                )
            except (psutil.NoSuchProcess, psutil.AccessDenied) as e:
                print(f"      ⚠️  Could not kill (may have already exited): {e}")
                log_port_cleanup(
                    f"Failed to kill process: {proc_name} (PID {proc.pid}) - Error: {e}"
                )

        # Wait for OS to release port
        import time

        print(f"   Waiting 2 seconds for OS to release {port}...")
        time.sleep(2)
        print("✅ Port cleanup completed")
        log_port_cleanup(f"Port cleanup completed for {port}")
    else:
        print(f"✅ No orphaned processes found using {port}")
        log_port_cleanup(f"Port cleanup for {port}: No orphaned processes found")


@dataclass
class ChipDetectionResult:
    """Result of ESP chip detection.

    Attributes:
        ok: True if chip was detected, False otherwise
        chip_type: Detected chip type (e.g., "ESP32-S3", "ESP32-C6")
        environment: Suggested PlatformIO environment (e.g., "esp32s3", "esp32c6")
        error_message: Optional error description if detection failed
    """

    ok: bool
    chip_type: str | None
    environment: str | None
    error_message: str | None = None


def detect_attached_chip(port: str, timeout: float = 10.0) -> ChipDetectionResult:
    """Detect ESP chip type using esptool.

    Uses esptool with auto chip detection to identify the connected ESP device.
    This allows automatic selection of the correct PlatformIO environment.

    Args:
        port: Serial port name (e.g., "COM13", "/dev/ttyUSB0")
        timeout: Maximum time to wait for esptool in seconds

    Returns:
        ChipDetectionResult with chip type, suggested environment, or error details
    """
    try:
        result = subprocess.run(
            [
                "uv",
                "run",
                "python",
                "-m",
                "esptool",
                "-c",
                "auto",
                "-p",
                port,
                "chip-id",
            ],
            capture_output=True,
            text=True,
            timeout=timeout,
        )

        # Parse output for "Detecting chip type... ESP32-S3" line
        chip_type = None
        for line in result.stdout.split("\n"):
            if "Detecting chip type..." in line:
                # Extract chip type after "..."
                chip_type = line.split("...")[-1].strip()
                break

        if not chip_type:
            # Also check for "Chip is ESP32-S3" pattern in case output format varies
            for line in result.stdout.split("\n"):
                if line.strip().startswith("Chip is "):
                    chip_type = line.strip().replace("Chip is ", "").strip()
                    break

        if not chip_type:
            return ChipDetectionResult(
                ok=False,
                chip_type=None,
                environment=None,
                error_message="Could not parse chip type from esptool output",
            )

        # Map chip type to environment
        environment = chip_to_environment(chip_type)

        return ChipDetectionResult(
            ok=True,
            chip_type=chip_type,
            environment=environment,
            error_message=None,
        )

    except subprocess.TimeoutExpired:
        return ChipDetectionResult(
            ok=False,
            chip_type=None,
            environment=None,
            error_message=f"esptool timed out after {timeout}s - device may not be in bootloader mode",
        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        return ChipDetectionResult(
            ok=False,
            chip_type=None,
            environment=None,
            error_message=f"Failed to detect chip: {e}",
        )


def chip_to_environment(chip_type: str) -> str | None:
    """Map an ESP chip type to a PlatformIO environment name.

    Handles chip variants like "ESP32-S3 (QFN56)" by matching the base chip type.

    Args:
        chip_type: Chip type string from esptool (e.g., "ESP32-S3", "ESP32-C6 (QFN40)")

    Returns:
        PlatformIO environment name (e.g., "esp32s3") or None if no mapping found
    """
    # Normalize chip type for comparison
    chip_upper = chip_type.upper()

    # Try exact match first
    for known_chip, env in CHIP_TO_ENVIRONMENT.items():
        if chip_upper == known_chip.upper():
            return env

    # Try prefix match for variants like "ESP32-S3 (QFN56)"
    for known_chip, env in CHIP_TO_ENVIRONMENT.items():
        if chip_upper.startswith(known_chip.upper()):
            return env

    return None
