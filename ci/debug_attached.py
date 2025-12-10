#!/usr/bin/env python3
"""Three-phase device workflow: Package Install → Compile+Upload+Monitor.

This script orchestrates a complete device development workflow in three distinct phases:

Phase 0: Package Installation (GLOBAL SINGLETON LOCK via daemon)
    - Ensures PlatformIO packages are installed using `pio pkg install -e <environment>`
    - Uses singleton daemon to serialize package installations globally
    - Lock scope: System-wide (~/.platformio/packages/ shared by all projects)
    - Daemon survives agent termination and completes installation atomically
    - Fast path: ~3.8s validation when packages already installed
    - Prevents corruption from concurrent package downloads

Phase 1: Compile (NO LOCK - parallelizable)
    - Builds the PlatformIO project for the target environment
    - Uses `pio run -e <environment>` (normal compilation)
    - No lock needed: Each project has isolated build directory
    - Packages already installed, so no downloads triggered
    - Multiple agents can compile different projects simultaneously

Phase 2: Upload (with self-healing)
    - Kills lingering esptool/pio upload/monitor processes that may lock files or ports
    - Uploads firmware to the attached device using `pio run -t upload`
    - PlatformIO's incremental build system only rebuilds if source files changed
    - Handles port and file lock conflicts automatically

Phase 3: Monitor
    - Attaches to serial monitor and displays real-time output
    - --expect: Monitors until timeout, exits 0 if ALL keywords found, exits 1 if any missing
    - --fail-on: Terminates immediately on match, exits 1
    - Provides output summary (first/last 100 lines)

Concurrency Control:
    - Uses a file-based lock (~/.fastled/locks/device_debug.lock) for upload and monitor phases
    - Prevents multiple agents from interfering with device operations
    - Lock scope: System-wide (device is a global resource, not project-specific)
    - Lock timeout: 600 seconds (10 minutes)
    - Prevents port conflicts between multiple agents

Locking Architecture:
    - Phase 0: Daemon provides implicit global lock (singleton + sequential processing)
    - Phase 1: NO LOCK (compilation is project-local, fully parallelizable)
    - Phase 2-3: Device lock (~/.fastled/locks/device_debug.lock, system-wide)

Usage:
    uv run ci/debug_attached.py                          # Auto-detect env & sketch (default: 20s timeout, fails on "ERROR")
    uv run ci/debug_attached.py RX                       # Compile RX sketch (examples/RX), auto-detect env
    uv run ci/debug_attached.py RX --env esp32dev        # Compile RX sketch for specific environment
    uv run ci/debug_attached.py examples/RX/RX.ino       # Full path to sketch
    uv run ci/debug_attached.py --verbose                # Verbose mode
    uv run ci/debug_attached.py --upload-port /dev/ttyUSB0  # Specific port
    uv run ci/debug_attached.py --timeout 120            # Monitor for 120 seconds
    uv run ci/debug_attached.py --timeout 2m             # Monitor for 2 minutes
    uv run ci/debug_attached.py --timeout 5000ms         # Monitor for 5 seconds
    uv run ci/debug_attached.py --fail-on PANIC          # Exit 1 immediately if "PANIC" found (replaces default "ERROR")
    uv run ci/debug_attached.py --fail-on ERROR --fail-on CRASH  # Exit 1 on any keyword
    uv run ci/debug_attached.py --no-fail-on             # Disable all failure keywords
    uv run ci/debug_attached.py --expect "SUCCESS"       # Exit 0 only if "SUCCESS" found by timeout
    uv run ci/debug_attached.py --expect "PASS" --expect "OK"  # Exit 0 only if ALL keywords found
    uv run ci/debug_attached.py --stream                 # Stream mode (runs until Ctrl+C)
    uv run ci/debug_attached.py RX --env esp32dev --verbose --upload-port COM3
"""

import argparse
import os
import platform
import re
import subprocess
import sys
import time
from pathlib import Path

import psutil
import serial
import serial.tools.list_ports
from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream

from ci.compiler.build_utils import get_utf8_env
from ci.util.build_lock import BuildLock
from ci.util.global_interrupt_handler import notify_main_thread
from ci.util.output_formatter import TimestampFormatter


def resolve_sketch_path(sketch_arg: str, project_dir: Path) -> str:
    """Resolve sketch argument to examples directory path.

    Handles various input formats:
        - 'RX' → 'examples/RX'
        - 'examples/RX' → 'examples/RX'
        - 'examples/RX/RX.ino' → 'examples/RX'
        - '/full/path/to/examples/RX' → 'examples/RX'
        - 'examples/deep/nested/Sketch' → 'examples/deep/nested/Sketch'

    Args:
        sketch_arg: Sketch name, relative path, or full path
        project_dir: Project root directory

    Returns:
        Resolved path relative to project root (e.g., 'examples/RX')

    Raises:
        SystemExit: If sketch cannot be found or is ambiguous
    """
    # Handle full paths
    sketch_path = Path(sketch_arg)
    if sketch_path.is_absolute():
        # Convert absolute path to relative from project root
        try:
            relative_path = sketch_path.relative_to(project_dir)
            # If it's a .ino file, get the parent directory
            if relative_path.suffix == ".ino":
                relative_path = relative_path.parent
            return str(relative_path).replace("\\", "/")
        except ValueError:
            print(f"❌ Error: Sketch path is outside project directory: {sketch_arg}")
            print(f"   Project directory: {project_dir}")
            sys.exit(1)

    # Handle relative paths or sketch names
    sketch_str = str(sketch_path).replace("\\", "/")

    # Strip .ino extension if present
    if sketch_str.endswith(".ino"):
        sketch_str = str(Path(sketch_str).parent).replace("\\", "/")

    # If already starts with 'examples/', use as-is
    if sketch_str.startswith("examples/"):
        candidate = project_dir / sketch_str
        if candidate.is_dir():
            return sketch_str
        print(f"❌ Error: Sketch directory not found: {sketch_str}")
        print(f"   Expected directory: {candidate}")
        sys.exit(1)

    # Search for sketch in examples directory
    examples_dir = project_dir / "examples"
    if not examples_dir.exists():
        print(f"❌ Error: examples directory not found: {examples_dir}")
        sys.exit(1)

    # Find all matching directories
    sketch_name = sketch_str.split("/")[-1]  # Get the sketch name
    matches = list(examples_dir.rglob(f"*/{sketch_name}")) + list(
        examples_dir.glob(sketch_name)
    )

    # Filter to directories only
    matches = [m for m in matches if m.is_dir()]

    if len(matches) == 0:
        print(f"❌ Error: Sketch not found: {sketch_arg}")
        print(f"   Searched in: {examples_dir}")
        sys.exit(1)
    elif len(matches) > 1:
        print(
            f"❌ Error: Ambiguous sketch name '{sketch_arg}'. Multiple matches found:"
        )
        for match in matches:
            rel_path = match.relative_to(project_dir)
            print(f"   - {rel_path}")
        print("\n   Please specify the full path to resolve ambiguity.")
        sys.exit(1)

    # Single match found
    resolved = matches[0].relative_to(project_dir)
    return str(resolved).replace("\\", "/")


def parse_timeout(timeout_str: str) -> int:
    """Parse timeout string with optional suffix into seconds.

    Supported formats:
        - Plain number: "120" → 120 seconds
        - Milliseconds: "5000ms" → 5 seconds
        - Seconds: "120s" → 120 seconds
        - Minutes: "2m" → 120 seconds

    Args:
        timeout_str: Timeout string (e.g., "120", "2m", "5000ms")

    Returns:
        Timeout in seconds (integer)

    Raises:
        ValueError: If format is invalid or value is not positive
    """
    timeout_str = timeout_str.strip()

    # Match number with optional suffix
    match = re.match(r"^(\d+(?:\.\d+)?)\s*(ms|s|m)?$", timeout_str, re.IGNORECASE)
    if not match:
        raise ValueError(
            f"Invalid timeout format: '{timeout_str}'. "
            f"Expected formats: '120', '120s', '2m', '5000ms'"
        )

    value_str, suffix = match.groups()
    value = float(value_str)

    if value <= 0:
        raise ValueError(f"Timeout must be positive, got: {value}")

    # Convert to seconds
    if suffix is None or suffix.lower() == "s":
        # Default is seconds
        seconds = value
    elif suffix.lower() == "ms":
        # Milliseconds to seconds
        seconds = value / 1000
    elif suffix.lower() == "m":
        # Minutes to seconds
        seconds = value * 60
    else:
        # Should never reach here due to regex
        raise ValueError(f"Unknown suffix: {suffix}")

    # Return as integer (round up to ensure at least 1 second for small values)
    return max(1, int(seconds))


def auto_detect_upload_port() -> str | None:
    """Auto-detect the upload port from available serial ports.

    Returns the first available ESP32/ESP8266/Arduino serial port found,
    or None if no suitable port is detected.

    Returns:
        Port name (e.g., 'COM3') or None if not found.
    """
    ports = serial.tools.list_ports.comports()

    # Look for common ESP32/Arduino USB-to-serial chips
    for port in ports:
        description = port.description.lower()
        # Common ESP32 USB chips: CP2102, CH340, FTDI
        if any(
            chip in description
            for chip in ["cp210", "ch340", "ch341", "ftdi", "usb-serial", "uart"]
        ):
            return port.device

    # If no ESP-specific port found, return first non-Bluetooth port
    for port in ports:
        if "bluetooth" not in port.description.lower():
            return port.device

    return None


def kill_process_using_port(port_name: str) -> bool:
    """Kill the process holding a specific serial port.

    This function:
    1. Tries to open the port to check if it's locked
    2. If locked, finds the process using common serial communication patterns
    3. Surgically kills only that specific process

    Args:
        port_name: Serial port name (e.g., 'COM3', '/dev/ttyUSB0')

    Returns:
        True if a process was found and killed, False otherwise.
    """
    # First, check if the port is actually locked
    try:
        ser = serial.Serial(port_name, baudrate=115200, timeout=1)
        ser.close()
        # Port is available - no need to kill anything
        return False
    except serial.SerialException as e:
        error_msg = str(e)
        # Check if this is actually a port lock (not just port doesn't exist)
        if not (
            "PermissionError" in error_msg
            or "Access is denied" in error_msg
            or "in use" in error_msg.lower()
        ):
            # Port doesn't exist or other error - not a lock issue
            return False

    print(f"Port {port_name} is locked, searching for process using it...")

    current_pid = os.getpid()

    # Get current process tree to protect our own execution chain
    protected_pids = {current_pid}
    try:
        current_proc = psutil.Process(current_pid)
        parent = current_proc.parent()
        while parent:
            protected_pids.add(parent.pid)
            parent = parent.parent()
    except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass

    # List of executable names that commonly use serial ports
    serial_exes = [
        "python.exe",
        "python3.exe",
        "python",
        "pio.exe",
        "pio",
        "esptool.exe",
        "esptool.py",
        "esptool",
        "platformio.exe",
        "platformio",
        "miniterm.exe",
        "miniterm.py",
        "miniterm",
        "putty.exe",
        "putty",
        "teraterm.exe",
        "teraterm",
    ]

    # Patterns in command line that indicate serial monitor usage
    cmdline_patterns = [
        "pio monitor",
        "pio device monitor",
        "device monitor",
        "miniterm",
        "esptool",
        port_name.lower(),
    ]

    # Find processes that match serial communication patterns
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            if proc.pid in protected_pids:
                continue

            proc_name = proc.info.get("name", "").lower()
            cmdline = proc.info.get("cmdline", [])

            # Check if executable name matches
            if not any(exe in proc_name for exe in serial_exes):
                continue

            # Check if command line indicates serial port usage
            if cmdline:
                cmdline_str = " ".join(cmdline).lower()
                if any(pattern in cmdline_str for pattern in cmdline_patterns):
                    # Found a candidate - kill it
                    print(f"Found process using {port_name}:")
                    print(f"  PID {proc.pid}: {proc.info.get('name', 'unknown')}")
                    if cmdline:
                        print(f"  Command: {' '.join(cmdline[:10])}")
                    print(f"Killing process...")
                    proc.kill()
                    print(f"Killed process using port {port_name}")
                    print("Waiting 2 seconds for port to release...")
                    time.sleep(2)
                    return True

        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue

    print(f"Could not identify process using {port_name}")
    return False


def kill_lingering_processes() -> int:
    """Kill lingering processes that may lock build files or serial ports.

    This self-healing step kills processes in the following order:
    1. All pio.exe processes (PlatformIO native wrapper - safe to kill all)
    2. esptool processes (firmware upload tool that locks .bin files)
    3. Python processes running PlatformIO commands (pio run/upload/monitor)
       - ONLY kills fastled8 venv processes
       - PROTECTS clud processes and current process tree

    SAFETY: We surgically target only PlatformIO-related processes while
    protecting the current agent instance (clud) and its process tree.

    Returns:
        Number of processes killed.
    """
    killed_count = 0
    current_pid = os.getpid()

    # Get current process tree to protect our own execution chain
    try:
        current_proc = psutil.Process(current_pid)
        protected_pids = {current_pid}
        # Add all parent PIDs to protection list
        parent = current_proc.parent()
        while parent:
            protected_pids.add(parent.pid)
            parent = parent.parent()
    except (psutil.NoSuchProcess, psutil.AccessDenied):
        protected_pids = {current_pid}

    # Phase 1: Kill ALL pio.exe processes (PlatformIO native wrapper)
    # These are always safe to kill - they're just wrappers around python
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            if proc.pid in protected_pids:
                continue

            proc_name = proc.info.get("name", "").lower()

            if "pio.exe" == proc_name or "pio" == proc_name:
                cmdline = proc.info.get("cmdline", [])
                print(
                    f"Killing pio.exe process (PID {proc.pid}): {' '.join(cmdline[:5]) if cmdline else 'pio.exe'}..."
                )
                proc.kill()
                killed_count += 1
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    # Phase 2: Kill esptool processes (locks .bin files and COM ports)
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            if proc.pid in protected_pids:
                continue

            cmdline = proc.info.get("cmdline", [])
            if not cmdline:
                continue

            cmdline_str = " ".join(cmdline).lower()
            proc_name = proc.info.get("name", "").lower()

            if "esptool" in cmdline_str or "esptool.exe" in proc_name:
                print(
                    f"Killing esptool process (PID {proc.pid}): {' '.join(cmdline[:5])}..."
                )
                proc.kill()
                killed_count += 1
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    # Phase 3: Kill Python processes running PlatformIO commands
    # SURGICAL: Only kill fastled8 venv processes, protect clud and current chain
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            if proc.pid in protected_pids:
                continue

            proc_name = proc.info.get("name", "").lower()
            if "python" not in proc_name:
                continue

            cmdline = proc.info.get("cmdline", [])
            if not cmdline:
                continue

            cmdline_str = " ".join(cmdline)
            cmdline_lower = cmdline_str.lower()

            # PROTECTION: Skip clud processes (contains 'clud' in path or command)
            if "clud" in cmdline_lower:
                continue

            # PROTECTION: Skip non-PlatformIO processes
            if "pio" not in cmdline_lower:
                continue

            # TARGET: PlatformIO commands from fastled8 venv
            # - pio run (compilation - can lock build files)
            # - pio upload (locks COM ports and .bin files)
            # - pio monitor (locks COM ports)
            # - tool-scons (build system - can lock files)
            is_pio_command = any(
                keyword in cmdline_lower
                for keyword in ["pio.exe", "tool-scons", "platformio"]
            )

            if is_pio_command:
                # Extra safety: Verify it's from fastled8 venv or PlatformIO packages
                is_fastled8 = (
                    "fastled8" in cmdline_lower or ".platformio" in cmdline_lower
                )

                if is_fastled8:
                    print(
                        f"Killing PlatformIO Python process (PID {proc.pid}): {cmdline_str[:80]}..."
                    )
                    proc.kill()
                    killed_count += 1

        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    if killed_count > 0:
        print(f"Killed {killed_count} lingering process(es)")
        print("Waiting 3 seconds for OS to release file locks and ports...")
        time.sleep(
            3
        )  # Give OS time to release file locks and ports (increased from 2s)
        print()

    return killed_count


def run_compile(
    build_dir: Path,
    environment: str | None = None,
    verbose: bool = False,
) -> bool:
    """Compile the PlatformIO project.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: PlatformIO environment to build (None = default)
        verbose: Enable verbose output

    Returns:
        True if compilation succeeded, False otherwise.
    """
    # Self-healing: Kill lingering processes before compilation to prevent file locks
    kill_lingering_processes()

    cmd = ["pio", "run", "--project-dir", str(build_dir)]
    if environment:
        cmd.extend(["--environment", environment])
    if verbose:
        cmd.append("--verbose")

    print("=" * 60)
    print("COMPILING")
    print("=" * 60)

    formatter = TimestampFormatter()
    proc = RunningProcess(
        cmd,
        cwd=build_dir,
        auto_run=True,
        output_formatter=formatter,
        env=get_utf8_env(),
    )

    try:
        # 15 minute timeout per CLAUDE.md standards
        while line := proc.get_next_line(timeout=900):
            if isinstance(line, EndOfStream):
                break
            print(line)
    except KeyboardInterrupt:
        print("\nKeyboardInterrupt: Stopping compilation")
        proc.terminate()
        notify_main_thread()
        raise

    proc.wait()
    success = proc.returncode == 0

    if success:
        print("\n✅ Compilation succeeded\n")
    else:
        print(f"\n❌ Compilation failed (exit code {proc.returncode})\n")

    return success


def check_port_available(port_name: str) -> bool:
    """Check if a serial port is available (not locked).

    Args:
        port_name: Serial port name (e.g., 'COM3', '/dev/ttyUSB0')

    Returns:
        True if port is available, False if locked or doesn't exist.
    """
    try:
        ser = serial.Serial(port_name, baudrate=115200, timeout=1)
        ser.close()
        return True
    except serial.SerialException:
        return False


def force_port_release(port_name: str) -> bool:
    """Attempt to force-release a locked serial port.

    This function tries to open and immediately close the port multiple times
    with delays to help the Windows driver release the port lock.

    Args:
        port_name: Serial port name (e.g., 'COM3')

    Returns:
        True if port becomes available, False otherwise.
    """
    print(f"Attempting to force-release port {port_name}...")

    for attempt in range(3):
        try:
            # Try to open and immediately close the port
            ser = serial.Serial(port_name, baudrate=115200, timeout=1)
            ser.close()
            print(f"✅ Port {port_name} is now available")
            return True
        except serial.SerialException as e:
            if attempt < 2:
                print(
                    f"   Attempt {attempt + 1}/3 failed: {str(e)[:60]}... retrying in 2s"
                )
                time.sleep(2)
            else:
                print(f"   All attempts failed: {str(e)[:60]}")

    return False


def usb_device_reset(port_name: str) -> bool:
    """Reset USB device associated with a serial port.

    This function attempts to reset the USB device at the hardware/driver level
    on Windows using PowerShell's Device Manager APIs. This is more reliable
    than process killing for stubborn driver-level port locks.

    On non-Windows platforms, this is a no-op.

    Args:
        port_name: Serial port name (e.g., 'COM3')

    Returns:
        True if USB device was reset successfully, False otherwise.
    """
    # Only supported on Windows
    if platform.system() != "Windows":
        return False

    print(f"Attempting USB device reset for port {port_name}...")

    # Find USB device info for this port
    ports = serial.tools.list_ports.comports()
    target_port = None
    for port in ports:
        if port.device == port_name:
            target_port = port
            break

    if not target_port:
        print(f"   ❌ Port {port_name} not found in system port list")
        return False

    # Check if we have hardware ID info
    if not target_port.hwid:
        print(f"   ❌ No hardware ID available for port {port_name}")
        return False

    print(f"   Port info: {target_port.description}")
    print(f"   Hardware ID: {target_port.hwid}")

    # Use PowerShell to disable/enable the device via Device Manager
    # This is more reliable than devcon and doesn't require external tools
    try:
        # PowerShell command to disable and re-enable COM port
        ps_script = f"""
$port = Get-PnpDevice | Where-Object {{ $_.FriendlyName -like "*{port_name}*" -or $_.FriendlyName -like "*{target_port.description}*" }}
if ($port) {{
    Write-Host "Disabling device: $($port.FriendlyName)"
    Disable-PnpDevice -InstanceId $port.InstanceId -Confirm:$false -ErrorAction Stop
    Start-Sleep -Milliseconds 1000
    Write-Host "Enabling device: $($port.FriendlyName)"
    Enable-PnpDevice -InstanceId $port.InstanceId -Confirm:$false -ErrorAction Stop
    Start-Sleep -Milliseconds 2000
    Write-Host "Device reset complete"
    exit 0
}} else {{
    Write-Host "Device not found"
    exit 1
}}
"""

        # Run PowerShell command
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command", ps_script],
            capture_output=True,
            text=True,
            timeout=15,
        )

        if result.returncode == 0:
            print(f"   ✅ USB device reset successfully")
            print(f"   Waiting 3 seconds for device re-enumeration...")
            time.sleep(3)
            return True
        else:
            print(f"   ⚠️  PowerShell reset failed: {result.stderr.strip()}")
            return False

    except subprocess.TimeoutExpired:
        print(f"   ⚠️  PowerShell reset timed out")
        return False
    except Exception as e:
        print(f"   ⚠️  USB reset failed: {str(e)[:80]}")
        return False


def run_upload(
    build_dir: Path,
    environment: str | None = None,
    upload_port: str | None = None,
    verbose: bool = False,
) -> bool:
    """Upload firmware to device.

    This function uses `pio run -t upload` which will upload the firmware.
    PlatformIO's incremental build system only rebuilds if source files changed
    since the last compilation, making this fast when nothing changed.

    Includes self-healing logic to kill lingering processes that may lock
    serial ports before attempting upload.

    On Windows, serial ports may remain locked at the driver level even after
    all processes are killed. This function retries up to 3 times with
    increasing delays and port release attempts to handle driver port release latency.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: PlatformIO environment to upload (None = default)
        upload_port: Serial port to use (None = auto-detect)
        verbose: Enable verbose output

    Returns:
        True if upload succeeded, False otherwise.
    """
    # Self-healing: First try surgical kill of process using specific port
    port_to_check = upload_port or auto_detect_upload_port()

    if port_to_check:
        killed_port_process = kill_process_using_port(port_to_check)
        if not killed_port_process:
            # No port-specific process found, fall back to general cleanup
            kill_lingering_processes()
    else:
        # No specific port detected, do general cleanup
        kill_lingering_processes()

    # Windows driver port release retry parameters
    max_retries = 3  # Increased from 2 to 3
    retry_delay = 5  # Increased from 3 to 5 seconds

    cmd = [
        "pio",
        "run",
        "--project-dir",
        str(build_dir),
        "-t",
        "upload",
    ]
    if environment:
        cmd.extend(["--environment", environment])
    if upload_port:
        cmd.extend(["--upload-port", upload_port])
    if verbose:
        cmd.append("--verbose")

    print("=" * 60)
    print("UPLOADING")
    print("=" * 60)

    # Retry loop for Windows driver port release delays
    for attempt in range(max_retries + 1):
        if attempt > 0:
            print(
                f"\n⚠️  Upload attempt {attempt + 1}/{max_retries + 1} (retrying after port release delay...)"
            )
            # Kill lingering processes before retry to release port locks
            if port_to_check:
                kill_process_using_port(port_to_check)
            kill_lingering_processes()

            # Check if port is available before waiting
            if port_to_check and not check_port_available(port_to_check):
                print(f"Port {port_to_check} still locked, attempting force-release...")
                if not force_port_release(port_to_check):
                    print(
                        f"⚠️  Port {port_to_check} could not be released via serial port operations"
                    )
                    # Try USB device reset as a deeper intervention
                    print(f"Attempting USB device reset (hardware-level)...")
                    if usb_device_reset(port_to_check):
                        # Check if port is now available after USB reset
                        if check_port_available(port_to_check):
                            print(
                                f"✅ Port {port_to_check} successfully released via USB reset"
                            )
                        else:
                            print(
                                f"⚠️  USB reset completed but port still appears locked"
                            )
                            print(
                                f"Waiting {retry_delay} seconds for Windows driver to stabilize..."
                            )
                            time.sleep(retry_delay)
                            retry_delay += (
                                3  # Increase delay for next retry (5s, 8s, 11s, ...)
                            )
                    else:
                        print(
                            f"⚠️  USB reset unavailable or failed, waiting for driver release"
                        )
                        print(
                            f"Waiting {retry_delay} seconds for Windows driver to release port..."
                        )
                        time.sleep(retry_delay)
                        retry_delay += (
                            3  # Increase delay for next retry (5s, 8s, 11s, ...)
                        )
                else:
                    print(
                        f"✅ Port {port_to_check} successfully released, proceeding with upload"
                    )
            else:
                # Port is available or no specific port to check
                if port_to_check:
                    print(f"✅ Port {port_to_check} is available")
                print(f"Waiting {retry_delay} seconds before retry...")
                time.sleep(retry_delay)
                retry_delay += 3  # Increase delay for next retry

        formatter = TimestampFormatter()
        proc = RunningProcess(
            cmd,
            cwd=build_dir,
            auto_run=True,
            output_formatter=formatter,
            env=get_utf8_env(),
        )

        # Track the actual port PlatformIO detects (may differ from our auto-detect)
        actual_detected_port = None

        try:
            # 15 minute timeout per CLAUDE.md standards
            while line := proc.get_next_line(timeout=900):
                if isinstance(line, EndOfStream):
                    break
                print(line)

                # Parse PlatformIO's auto-detected port from output
                # Example: "26.46 Auto-detected: COM4"
                if "Auto-detected:" in line and actual_detected_port is None:
                    # Extract port name (e.g., "COM4", "/dev/ttyUSB0")
                    match = re.search(r"Auto-detected:\s+(\S+)", line)
                    if match:
                        actual_detected_port = match.group(1)
                        print(
                            f"   [Detected PlatformIO auto-selected port: {actual_detected_port}]"
                        )

        except KeyboardInterrupt:
            print("\nKeyboardInterrupt: Stopping upload")
            proc.terminate()
            notify_main_thread()
            raise

        proc.wait()
        success = proc.returncode == 0

        # Update port_to_check with PlatformIO's actual detected port for retry logic
        if actual_detected_port and not upload_port:
            # Only override if user didn't specify a port explicitly
            if actual_detected_port != port_to_check:
                print(f"\n⚠️  Port mismatch detected:")
                print(f"   Python auto-detect: {port_to_check}")
                print(f"   PlatformIO detected: {actual_detected_port}")
                print(
                    f"   Using PlatformIO's port ({actual_detected_port}) for retry logic\n"
                )
                port_to_check = actual_detected_port

        if success:
            print("\n✅ Upload succeeded\n")
            return True

        # Check if this was a port lock error (retry-able)
        # If it's a different error, don't retry
        if attempt < max_retries:
            print(f"\n⚠️  Upload failed (exit code {proc.returncode})")
            print(
                "Retrying upload (Windows driver may need more time to release port)..."
            )
        else:
            print(
                f"\n❌ Upload failed after {max_retries + 1} attempts (exit code {proc.returncode})"
            )
            if port_to_check:
                print(
                    f"\n⚠️  Port {port_to_check} may still be locked at the Windows driver level"
                )
                print("\nDiagnostic tool (identifies process holding port):")
                print(f"  uv run python ci/debug/port_diagnostic.py {port_to_check}")
                print("\nManual intervention options:")
                print(f"  1. Unplug and replug the USB cable")
                print(f"  2. Reset the device manually")
                print(f"  3. Restart the Windows COM port driver:")
                print(
                    f"     - Open Device Manager → Ports (COM & LPT) → Right-click {port_to_check} → Disable → Enable"
                )
                print(
                    f"  4. Run: taskkill /F /IM python.exe (WARNING: kills ALL Python processes)"
                )
            print()

    return False


def run_monitor(
    build_dir: Path,
    environment: str | None = None,
    monitor_port: str | None = None,
    verbose: bool = False,
    timeout: int = 80,
    fail_keywords: list[str] | None = None,
    expect_keywords: list[str] | None = None,
    stream: bool = False,
) -> tuple[bool, list[str]]:
    """Attach to serial monitor and capture output.

    Keyword Behavior:
        --expect: Monitors until timeout, then checks if ALL keywords were found.
                  Exit 0 if all found, exit 1 if any missing.
        --fail-on: Terminates immediately when ANY keyword is found, exits 1.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: PlatformIO environment to monitor (None = default)
        monitor_port: Serial port to monitor (None = auto-detect)
        verbose: Enable verbose output
        timeout: Maximum time to monitor in seconds (default: 20)
        fail_keywords: List of keywords that trigger immediate termination + exit 1 (default: ["ERROR"])
        expect_keywords: List of keywords that must ALL be found by timeout for exit 0
        stream: If True, monitor runs indefinitely until Ctrl+C (ignores timeout)

    Returns:
        Tuple of (success, output_lines)
    """
    if fail_keywords is None:
        fail_keywords = []
    if expect_keywords is None:
        expect_keywords = []
    cmd = [
        "pio",
        "device",
        "monitor",
        "--project-dir",
        str(build_dir),
    ]
    if environment:
        cmd.extend(["--environment", environment])
    if monitor_port:
        cmd.extend(["--port", monitor_port])
    if verbose:
        cmd.append("--verbose")

    print("=" * 60)
    print("MONITORING SERIAL OUTPUT")
    if stream:
        print("Mode: STREAMING (runs until Ctrl+C)")
    else:
        print(f"Timeout: {timeout} seconds")
    if fail_keywords:
        print(f"Fail keywords: {', '.join(fail_keywords)}")
    if expect_keywords:
        print(f"Expect keywords: {', '.join(expect_keywords)}")
    print("=" * 60)

    formatter = TimestampFormatter()
    proc = RunningProcess(
        cmd,
        cwd=build_dir,
        auto_run=True,
        output_formatter=formatter,
        env=get_utf8_env(),
    )

    output_lines = []
    failing_lines = []  # Track all lines containing fail keywords
    expect_lines = []  # Track all lines containing expect keywords
    found_expect_keywords = set()  # Track which expect keywords have been found
    start_time = time.time()
    fail_keyword_found = False
    matched_fail_keyword = None
    matched_fail_line = None
    timeout_reached = False

    try:
        while True:
            # Check timeout (unless in streaming mode)
            if not stream:
                elapsed = time.time() - start_time
                if elapsed >= timeout:
                    print(f"\n⏱️  Timeout reached ({timeout}s), stopping monitor...")
                    timeout_reached = True
                    proc.terminate()
                    break

            # Read next line with 30-second timeout
            try:
                line = proc.get_next_line(timeout=30)
                if isinstance(line, EndOfStream):
                    break
                if line:
                    output_lines.append(line)
                    print(line)  # Real-time output

                    # Check for expect keywords (track but don't terminate)
                    if expect_keywords:
                        for keyword in expect_keywords:
                            if keyword in line:
                                # Track this expected line
                                expect_lines.append((keyword, line))

                                # Mark this keyword as found
                                if keyword not in found_expect_keywords:
                                    found_expect_keywords.add(keyword)
                                    print(f"\n✅ EXPECT KEYWORD DETECTED: '{keyword}'")
                                    print(f"   Matched line: {line}")
                                    print(
                                        f"   (Continuing to monitor - need all {len(expect_keywords)} keywords)\n"
                                    )

                    # Check for fail keywords - TERMINATE IMMEDIATELY
                    if fail_keywords and not fail_keyword_found:
                        for keyword in fail_keywords:
                            if keyword in line:
                                # Track this failing line
                                failing_lines.append((keyword, line))

                                if not fail_keyword_found:
                                    # First failure - terminate immediately
                                    fail_keyword_found = True
                                    matched_fail_keyword = keyword
                                    matched_fail_line = line
                                    print(f"\n🚨 FAIL KEYWORD DETECTED: '{keyword}'")
                                    print(f"   Matched line: {line}")
                                    print(f"   Terminating monitor immediately...\n")
                                    proc.terminate()
                                break

            except TimeoutError:
                # No output within 30 seconds - continue waiting (check overall timeout on next loop)
                continue

    except KeyboardInterrupt:
        print("\nKeyboardInterrupt: Stopping monitor")
        proc.terminate()
        notify_main_thread()
        raise

    proc.wait()

    # Determine success based on exit conditions
    if fail_keyword_found:
        # Fail keyword match always means failure
        success = False
    elif expect_keywords:
        # If expect keywords were specified, ALL must be found for success
        missing_keywords = set(expect_keywords) - found_expect_keywords
        if missing_keywords:
            # Not all expect keywords found - failure
            success = False
        else:
            # All expect keywords found - success
            success = True
    elif timeout_reached:
        # Normal timeout is considered success (exit 0)
        success = True
    else:
        # Process exited on its own - use actual return code
        success = proc.returncode == 0

    # Display first 50 and last 100 lines summary
    print("\n" + "=" * 60)
    print("OUTPUT SUMMARY")
    print("=" * 60)

    if len(output_lines) > 0:
        first_50 = output_lines[:50]
        last_100 = output_lines[-100:]

        print(f"\n--- FIRST {len(first_50)} LINES ---")
        for line in first_50:
            print(line)

        if len(output_lines) > 50:
            print(f"\n--- LAST {len(last_100)} LINES ---")
            for line in last_100:
                print(line)

        # Display all expect lines if any were detected
        if expect_lines:
            print(f"\n--- EXPECT LINES ({len(expect_lines)} total) ---")
            for keyword, line in expect_lines:
                print(f"[{keyword}] {line}")

        # Display all failing lines if any were detected
        if failing_lines:
            print(f"\n--- FAILING LINES ({len(failing_lines)} total) ---")
            for keyword, line in failing_lines:
                print(f"[{keyword}] {line}")

        print(f"\nTotal output lines: {len(output_lines)}")
    else:
        print("\nNo output captured")

    print("\n" + "=" * 60)

    if fail_keyword_found:
        print(f"❌ Monitor failed - fail keyword '{matched_fail_keyword}' detected")
        print(f"   Matched line: {matched_fail_line}")
    elif expect_keywords:
        # Check if all expect keywords were found
        missing_keywords = set(expect_keywords) - found_expect_keywords
        if missing_keywords:
            print(f"❌ Monitor failed - not all expect keywords found")
            print(
                f"   Expected {len(expect_keywords)} keywords, found {len(found_expect_keywords)}"
            )
            # Display missing keywords clearly
            if len(missing_keywords) == 1:
                print(f'   Missing keyword: "{list(missing_keywords)[0]}"')
            else:
                print(f"   Missing keywords:")
                for keyword in sorted(missing_keywords):
                    print(f'     - "{keyword}"')
            # Display found keywords if any
            if found_expect_keywords:
                if len(found_expect_keywords) == 1:
                    print(f'   Found keyword: "{list(found_expect_keywords)[0]}"')
                else:
                    print(f"   Found keywords:")
                    for keyword in sorted(found_expect_keywords):
                        print(f'     - "{keyword}"')
        else:
            print(
                f"✅ Monitor succeeded - all {len(expect_keywords)} expect keywords found"
            )
            if len(expect_keywords) == 1:
                print(f'   Keyword: "{expect_keywords[0]}"')
            else:
                print(f"   Keywords:")
                for keyword in sorted(expect_keywords):
                    print(f'     - "{keyword}"')
    elif timeout_reached:
        print(f"✅ Monitor completed successfully (timeout reached after {timeout}s)")
    elif stream and success:
        print("✅ Monitor completed successfully (streaming mode ended)")
    elif success:
        print("✅ Monitor completed successfully")
    else:
        print(f"❌ Monitor failed (exit code {proc.returncode})")

    return success, output_lines


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Upload and monitor attached PlatformIO device",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Auto-detect env & sketch (default: 20s timeout, fails on "ERROR")
  %(prog)s RX                       # Compile RX sketch (examples/RX), auto-detect environment
  %(prog)s RX --env esp32dev        # Compile RX sketch for specific environment
  %(prog)s examples/RX              # Same as above (explicit path)
  %(prog)s examples/deep/nested/Sketch  # Deep nested sketch
  %(prog)s --verbose                # Verbose mode
  %(prog)s --upload-port /dev/ttyUSB0  # Specific port
  %(prog)s --timeout 120            # Monitor for 120 seconds (default: 20s)
  %(prog)s --timeout 2m             # Monitor for 2 minutes
  %(prog)s --timeout 5000ms         # Monitor for 5 seconds
  %(prog)s --fail-on PANIC          # Exit 1 immediately if "PANIC" found (replaces default "ERROR")
  %(prog)s --fail-on ERROR --fail-on CRASH  # Exit 1 on any keyword
  %(prog)s --no-fail-on             # Disable all failure keywords
  %(prog)s --expect "SUCCESS"       # Exit 0 only if "SUCCESS" found by timeout
  %(prog)s --expect "PASS" --expect "OK"  # Exit 0 only if ALL keywords found
  %(prog)s --stream                 # Stream mode (runs until Ctrl+C)
  %(prog)s RX --env esp32dev --verbose --upload-port COM3
        """,
    )

    parser.add_argument(
        "sketch",
        nargs="?",
        help="Sketch to compile (e.g., 'RX', 'examples/RX', 'examples/RX/RX.ino'). "
        "Sets FASTLED_SRC_DIR environment variable. Supports full paths and deep sketches.",
    )
    parser.add_argument(
        "--env",
        "-e",
        dest="environment",
        help="PlatformIO environment to build (optional, auto-detect if not provided)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose output",
    )
    parser.add_argument(
        "--upload-port",
        "-p",
        help="Serial port to use for upload and monitoring (e.g., /dev/ttyUSB0, COM3)",
    )
    parser.add_argument(
        "--timeout",
        "-t",
        type=str,
        default="20",
        help="Timeout for monitor phase. Supports: plain number (seconds), '120s', '2m', '5000ms' (default: 20)",
    )
    parser.add_argument(
        "--project-dir",
        "-d",
        type=Path,
        default=Path.cwd(),
        help="PlatformIO project directory (default: current directory)",
    )
    parser.add_argument(
        "--fail-on",
        "-f",
        action="append",
        dest="fail_keywords",
        help="Keyword that triggers immediate termination + exit 1 if found (can be specified multiple times). Default: 'ERROR'",
    )
    parser.add_argument(
        "--no-fail-on",
        action="store_true",
        help="Disable all --fail-on keywords (including default 'ERROR')",
    )
    parser.add_argument(
        "--expect",
        "-x",
        action="append",
        dest="expect_keywords",
        help="Keyword that must be found by timeout for exit 0. ALL keywords must be found (can be specified multiple times). Monitor runs until timeout to find all.",
    )
    parser.add_argument(
        "--stream",
        "-s",
        action="store_true",
        help="Stream mode: monitor runs indefinitely until Ctrl+C (ignores timeout)",
    )

    return parser.parse_args()


def main() -> int:
    """Main entry point."""
    args = parse_args()

    build_dir = args.project_dir.resolve()

    # Verify platformio.ini exists
    if not (build_dir / "platformio.ini").exists():
        print(f"❌ Error: platformio.ini not found in {build_dir}")
        print("   Make sure you're running this from a PlatformIO project directory")
        return 1

    # Handle sketch argument and set PLATFORMIO_SRC_DIR environment variable
    if args.sketch:
        try:
            resolved_sketch = resolve_sketch_path(args.sketch, build_dir)
            os.environ["PLATFORMIO_SRC_DIR"] = resolved_sketch
            print(f"📁 Sketch: {resolved_sketch}")
        except SystemExit:
            # resolve_sketch_path already printed error message
            return 1

    # Validate --fail-on and --no-fail-on conflict
    if args.no_fail_on and args.fail_keywords:
        print("❌ Error: Cannot specify both --fail-on and --no-fail-on")
        return 1

    # Set default fail keywords (unless --no-fail-on is specified)
    if args.no_fail_on:
        fail_keywords = []
    elif args.fail_keywords is None:
        fail_keywords = ["ERROR"]
    else:
        fail_keywords = args.fail_keywords

    # Parse timeout string with suffix support
    try:
        timeout_seconds = parse_timeout(args.timeout)
    except ValueError as e:
        print(f"❌ Error: {e}")
        return 1

    print("FastLED Debug Attached Device")
    print("=" * 60)
    print(f"Project: {build_dir}")
    if args.environment:
        print(f"Environment: {args.environment}")
    if args.upload_port:
        print(f"Upload port: {args.upload_port}")
    print("=" * 60)
    print()

    try:
        # ============================================================
        # PHASE 0: Package Installation (GLOBAL LOCK via daemon)
        # ============================================================
        # The daemon acts as a global singleton lock for package installations.
        # No explicit lock needed here - daemon serializes all pio pkg install.
        print("=" * 60)
        print("PHASE 0: ENSURING PACKAGES INSTALLED")
        print("=" * 60)

        from ci.util.pio_package_client import ensure_packages_installed

        if not ensure_packages_installed(build_dir, args.environment, timeout=1800):
            print("\n❌ Package installation failed or timed out")
            return 1

        print()

        # ============================================================
        # PHASE 1: Compile (NO LOCK - parallelizable)
        # ============================================================
        # No lock needed: each project has isolated build directory.
        # Multiple agents can compile different projects simultaneously.
        if not run_compile(build_dir, args.environment, args.verbose):
            return 1

        # ============================================================
        # PHASES 2-3: Upload + Monitor (DEVICE LOCK)
        # ============================================================
        # Acquire device lock for upload and monitor phases.
        # Lock ensures only one agent accesses the device at a time.
        lock = BuildLock(lock_name="device_debug", use_global=True)
        if not lock.acquire(timeout=600.0):  # 10 minute timeout
            print(
                "\n❌ Another agent is currently using the device for debug operations."
            )
            print("   Please retry later once the debug operation has completed.")
            print(f"   Lock file: {lock.lock_file}")
            return 1

        try:
            # Phase 2: Upload firmware (with incremental rebuild if needed)
            if not run_upload(
                build_dir, args.environment, args.upload_port, args.verbose
            ):
                return 1

            # Phase 3: Monitor serial output
            success, output = run_monitor(
                build_dir,
                args.environment,
                args.upload_port,  # Use same port for monitoring
                args.verbose,
                timeout_seconds,
                fail_keywords,
                args.expect_keywords or [],
                args.stream,
            )

            if not success:
                return 1

            print("\n✅ All three phases completed successfully")
            return 0

        finally:
            # Always release the device lock
            lock.release()

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
