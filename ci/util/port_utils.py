#!/usr/bin/env python3
"""Serial port detection and management utilities.

This module provides utilities for detecting serial ports, identifying USB devices,
and cleaning up orphaned processes that may be holding serial port locks.

Key features:
    - Auto-detection of USB serial ports (filters out Bluetooth, ACPI, etc.)
    - Preference for common ESP32/Arduino USB-to-serial chips
    - Safe process cleanup (never kills current process or parents)
    - Detailed logging of port cleanup operations
"""

import datetime
import os
from dataclasses import dataclass, field
from pathlib import Path

import psutil
import serial.tools.list_ports
from serial.tools.list_ports_common import ListPortInfo


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
    all_ports: list[ListPortInfo] = field(default_factory=list)


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
        non_usb_types = set()
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
    processes_to_kill = []

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
            proc_info = proc.as_dict(attrs=["pid", "name", "cmdline"])
            proc_pid = proc_info["pid"]
            proc_name = proc_info["name"]
            cmdline = proc_info["cmdline"]
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
                        processes_to_kill.append((proc, proc_name, cmdline))

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
        print(f"✅ Port cleanup completed")
        log_port_cleanup(f"Port cleanup completed for {port}")
    else:
        print(f"✅ No orphaned processes found using {port}")
        log_port_cleanup(f"Port cleanup for {port}: No orphaned processes found")
