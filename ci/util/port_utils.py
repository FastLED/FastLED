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

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# Mapping from detected ESP chip type to PlatformIO environment
CHIP_TO_ENVIRONMENT: dict[str, str] = {
    "ESP32-S3": "esp32s3",
    "ESP32-C6": "esp32c6",
    "ESP32-C3": "esp32c3",
    "ESP32-C2": "esp32c2",
    "ESP32-H2": "esp32h2",
    "ESP32-P4": "esp32p4",
    "ESP32": "esp32dev",  # Generic ESP32 (original)
    "ESP8266": "esp8266",  # ESP8266
}

# ESP32 variants that lack WiFi hardware
NO_WIFI_ENVIRONMENTS: set[str] = {"esp32h2", "esp32p4"}

# PlatformIO environments → expected USB VID:PID(s) for the data-bearing
# VCOM port. This bypasses the description-string heuristic for boards
# whose USB descriptors are too generic to disambiguate (e.g. "USB Serial
# Device" on Windows for both the LPC845-BRK VCOM and any other Microsoft
# CDC device).
#
# Each entry is a **tuple of accepted VID:PID pairs** — the first port
# whose (vid, pid) matches any pair is selected. Some boards ship one of
# several debug-probe firmwares that expose different VID:PIDs; listing
# them all here lets the same PlatformIO env work regardless of which
# firmware is on the on-board probe (see the LPC845-BRK note below).
#
# Add new entries here when porting to a new board. See FastLED #3300 for
# the LPC845-BRK case that motivated this map. ci/util/serial_probe.py
# has a richer fingerprint table; this map only carries the entries
# autoresearch needs for default-port selection.
ENVIRONMENT_TO_VCOM_VID_PIDS: dict[str, tuple[tuple[int, int], ...]] = {
    # LPC8xx family: the on-board debug probe can be either
    #   * LPC11U35 running the LPCXpresso VCOM firmware — 16C0:0483
    #     (the community "V-USB" VID:PID; shared with PJRC Teensy).
    #   * LPC-Link2 CMSIS-DAP firmware — 1FC9:0132
    #     (NXP's own VID:PID; the debug probe presents a single COM
    #     port that carries the LPC845's USART0 as a virtual serial
    #     bridge alongside the CMSIS-DAP HID interface).
    # Newer LPC845-BRK / LPCXpresso boards ship with the LPC-Link2
    # CMSIS-DAP firmware pre-flashed; either variant is acceptable for
    # AutoResearch as long as the VCOM stream reaches the host.
    "lpc845brk": ((0x16C0, 0x0483), (0x1FC9, 0x0132)),
    "lpc845": ((0x16C0, 0x0483), (0x1FC9, 0x0132)),
    "lpc804": ((0x16C0, 0x0483), (0x1FC9, 0x0132)),
    "lpcxpresso845max": ((0x16C0, 0x0483), (0x1FC9, 0x0132)),
    "lpcxpresso804": ((0x16C0, 0x0483), (0x1FC9, 0x0132)),
}

# Backwards-compatibility shim — the old single-VID:PID map is derived
# from the first entry of the new plural form so any external caller
# that still reads `ENVIRONMENT_TO_VCOM_VID_PID` gets the primary
# (LPCXpresso VCOM) fingerprint. Deprecated; update callers to consume
# the plural form and drop this once no in-repo callers remain.
ENVIRONMENT_TO_VCOM_VID_PID: dict[str, tuple[int, int]] = {
    env: pids[0] for env, pids in ENVIRONMENT_TO_VCOM_VID_PIDS.items()
}


def environment_has_wifi(environment: str) -> bool:
    """Check if a PlatformIO environment has WiFi hardware."""
    return environment.lower() not in NO_WIFI_ENVIRONMENTS


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


def auto_detect_upload_port(expected_environment: str | None) -> ComportResult:
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
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
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

    expected_env = expected_environment.lower() if expected_environment else None

    # VID:PID-based detection takes precedence over chip-probe and description
    # heuristics. For boards whose VCOM bridges have well-known IDs (LPC845-BRK
    # via LPC11U35 or LPC-Link2 CMSIS-DAP, see FastLED #3300), this is the only
    # reliable signal on Windows where multiple boards report a generic "USB
    # Serial Device" name. An environment may map to more than one accepted
    # VID:PID (e.g. LPC845-BRK boards ship with either LPCXpresso VCOM
    # firmware 16C0:0483 or LPC-Link2 CMSIS-DAP firmware 1FC9:0132 on the
    # debug probe) — the first port matching ANY of the entry's pairs wins.
    expected_vid_pids = (
        ENVIRONMENT_TO_VCOM_VID_PIDS.get(expected_env) if expected_env else None
    )
    if expected_vid_pids:
        accepted = set(expected_vid_pids)
        for port in usb_ports:
            if (port.vid, port.pid) in accepted:
                return ComportResult(
                    ok=True,
                    selected_port=port.device,
                    error_message=None,
                    all_ports=all_ports,
                )
        # Fingerprint expected but no port matched. Don't fall through to the
        # ESP probe — the wrong port will fail later with PermissionError or
        # silent timeout. Report explicitly so the user can plug the board in.
        formatted_expected = " or ".join(
            f"{vid:04X}:{pid:04X}" for vid, pid in expected_vid_pids
        )
        return ComportResult(
            ok=False,
            selected_port=None,
            error_message=(
                f"No USB serial port matched expected VCOM fingerprint for "
                f"'{expected_environment}' (VID:PID {formatted_expected}). "
                f"Detected USB ports: "
                + ", ".join(
                    f"{p.device}={p.vid:04X}:{p.pid:04X}"
                    if p.vid and p.pid
                    else f"{p.device}=----:----"
                    for p in usb_ports
                )
            ),
            all_ports=all_ports,
        )

    esp_environments = {env.lower() for env in CHIP_TO_ENVIRONMENT.values()}
    if expected_env in esp_environments:
        probe_notes: list[str] = []
        saw_positive_detection = False
        for port in usb_ports:
            # FastLED #3446: drop the explicit 3.0 s override so the call
            # picks up `detect_attached_chip`'s richer 15 s default and the
            # reset-strategy fallback chain (default → usb → no-reset).
            chip_result = detect_attached_chip(port.device)
            if chip_result.ok:
                saw_positive_detection = True
                detected_env = chip_result.environment or "unknown"
                probe_notes.append(
                    f"{port.device}: {chip_result.chip_type} ({detected_env})"
                )
                if detected_env.lower() == expected_env:
                    return ComportResult(
                        ok=True,
                        selected_port=port.device,
                        error_message=None,
                        all_ports=all_ports,
                    )
            else:
                probe_notes.append(
                    f"{port.device}: detection failed ({chip_result.error_message})"
                )

        if saw_positive_detection:
            expected_chip = environment_to_chip(expected_env) or expected_environment
            return ComportResult(
                ok=False,
                selected_port=None,
                error_message=(
                    f"No USB serial port matched expected environment "
                    f"'{expected_environment}' ({expected_chip}). Probed: "
                    + "; ".join(probe_notes)
                ),
                all_ports=all_ports,
            )
        # Probe was inconclusive; fall through to the USB descriptor heuristic.

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
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
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
    # Kill dedicated serial tools AND orphaned Python serial processes
    # (but protect agent backend: clud, claude, node.exe, etc.)
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

    # Python scripts that are safe to kill if orphaned (known serial port users)
    safe_python_scripts = [
        "autoresearch.py",
        "debug_attached.py",
        "autoresearch_loop.py",
        "monitor.py",
    ]

    # Agent processes to NEVER kill (even if orphaned)
    protected_agent_patterns = [
        "clud",
        "claude",
        "node.exe",
        "node",
        ".claude",
        "anthropic",
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
            proc_info: dict[str, Any] = proc.as_dict(attrs=["pid", "name", "cmdline"])  # type: ignore[misc]
            proc_pid: int = proc_info["pid"]
            proc_name: str = proc_info["name"]
            cmdline: list[str] | None = proc_info["cmdline"]
            proc_name_lower = proc_name.lower()

            # NEVER kill ourselves or parent processes
            if proc_pid in protected_pids:
                continue

            # Check if this is a Python process
            is_python = "python" in proc_name_lower

            if is_python and cmdline:
                # Python process - check if it's a protected agent process
                cmdline_str = " ".join(cmdline).lower()

                # NEVER kill agent processes (clud, claude, node.exe running claude code)
                if any(pattern in cmdline_str for pattern in protected_agent_patterns):
                    continue

                # Check if it's a safe-to-kill Python script (validate.py, debug_attached.py, etc.)
                is_safe_python_script = any(
                    script in cmdline_str for script in safe_python_scripts
                )

                if is_safe_python_script:
                    # Safe Python script - only kill if it mentions the port
                    if port_lower in cmdline_str:
                        processes_to_kill.append((proc, proc_name, cmdline))
                # Otherwise skip this Python process (might be something else)

            # Check dedicated serial tools (safe to kill)
            elif any(exe in proc_name_lower for exe in safe_serial_exes):
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


def _probe_chip_with_reset_mode(
    port: str, reset_mode: str, timeout: float
) -> tuple[str | None, str | None]:
    """Run esptool chip-id with a specific --before reset strategy.

    Returns ``(chip_type, error)``. Exactly one of the two is non-None on
    a clean call. The caller is responsible for combining results across
    multiple strategies and surfacing the final error if every strategy
    fails.
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
                "--before",
                reset_mode,
                "chip-id",
            ],
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        return (
            None,
            f"esptool --before {reset_mode} timed out after {timeout:.1f}s",
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        return None, f"esptool --before {reset_mode} crashed: {e}"

    # Parse output for "Detecting chip type... ESP32-S3" line
    chip_type: str | None = None
    for line in result.stdout.split("\n"):
        if "Detecting chip type..." in line:
            chip_type = line.split("...")[-1].strip()
            break

    if not chip_type:
        # Also check for "Chip is ESP32-S3" pattern in case output format varies
        for line in result.stdout.split("\n"):
            if line.strip().startswith("Chip is "):
                chip_type = line.strip().replace("Chip is ", "").strip()
                break

    if chip_type:
        return chip_type, None

    stderr_tail = (result.stderr or "").strip().splitlines()
    tail_line = stderr_tail[-1] if stderr_tail else ""
    return (
        None,
        f"esptool --before {reset_mode} produced no chip-id; last stderr: {tail_line!r}",
    )


def detect_attached_chip(port: str, timeout: float = 7.0) -> ChipDetectionResult:
    """Detect ESP chip type using esptool.

    Uses esptool with auto chip detection to identify the connected ESP device.
    This allows automatic selection of the correct PlatformIO environment.

    FastLED #3446: previously hardcoded at 3.0 s with one reset strategy.
    That budget undershoots the CP210x worst-case auto-reset path (slow
    USB driver init + ~4 s of esptool SYNC retries + bootloader
    handshake), so devkits with even a slightly sluggish reset circuit
    failed to be recognised on the first try.

    Detection budget per port:

    1. **Primary**: ``--before default-reset`` (the classic DTR/RTS
       auto-reset). Bounded by ``timeout``. Catches all healthy boards
       on the first pass.
    2. **Fallback chain**, ONLY triggered when the primary attempt
       actually *timed out* (i.e. esptool was still in the
       reset/sync loop, not "no device on this port" — those fail fast):
       - ``--before usb-reset`` — native USB-CDC chips
         (S2/S3/C2/C3/H2/C6) whose reset path is over USB, not DTR/RTS.
       - ``--before no-reset`` — boards where the user (or a
         previous flash) already left the device in the bootloader.

       Each fallback gets the same ``timeout``. Skipped on "no chip-id
       produced" so empty ports don't drag the total probe out — keeps
       a 5-port-all-empty sweep under ~10 s.

    Args:
        port: Serial port name (e.g., "COM13", "/dev/ttyUSB0")
        timeout: Per-strategy wall-clock budget. Default 7 s — covers
            the CP210x worst case while keeping a multi-port sweep
            with no devices well under 30 s total.

    Returns:
        ChipDetectionResult with chip type, suggested environment, or
        an aggregated error describing what each reset strategy saw.
    """
    primary_mode = "default-reset"
    chip_type, primary_err = _probe_chip_with_reset_mode(port, primary_mode, timeout)
    if chip_type:
        return ChipDetectionResult(
            ok=True,
            chip_type=chip_type,
            environment=chip_to_environment(chip_type),
            error_message=None,
        )

    # Only escalate to the fallback chain if the primary attempt was
    # cut off by `timeout` — that's the signature of a struggling reset
    # circuit. A primary that returned "no chip-id produced" means the
    # port is dead or the device isn't an ESP, so additional strategies
    # are a waste.
    if primary_err and "timed out" not in primary_err:
        return ChipDetectionResult(
            ok=False,
            chip_type=None,
            environment=None,
            error_message=primary_err,
        )

    attempts: list[str] = [primary_err or f"esptool --before {primary_mode}: timed out"]
    for mode in ("usb-reset", "no-reset"):
        chip_type, err = _probe_chip_with_reset_mode(port, mode, timeout)
        if chip_type:
            return ChipDetectionResult(
                ok=True,
                chip_type=chip_type,
                environment=chip_to_environment(chip_type),
                error_message=None,
            )
        attempts.append(err or f"esptool --before {mode}: unknown failure")

    return ChipDetectionResult(
        ok=False,
        chip_type=None,
        environment=None,
        error_message=(
            "esptool reset-strategy fallback chain exhausted: " + "; ".join(attempts)
        ),
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


def environment_to_chip(environment: str) -> str | None:
    """Map a PlatformIO environment name to its base ESP chip type."""
    env_lower = environment.lower()
    for chip_type, env in CHIP_TO_ENVIRONMENT.items():
        if env.lower() == env_lower:
            return chip_type
    return None
