#!/usr/bin/env python3
"""Four-phase device workflow: Package Install → Lint → Compile → Upload+Monitor.

This script orchestrates a complete device development workflow in four distinct phases:

Phase 0: Package Installation (GLOBAL SINGLETON LOCK via daemon)
    - Ensures PlatformIO packages are installed using `pio pkg install -e <environment>`
    - Uses singleton daemon to serialize package installations globally
    - Lock scope: System-wide (~/.platformio/packages/ shared by all projects)
    - Daemon survives agent termination and completes installation atomically
    - Fast path: ~3.8s validation when packages already installed
    - Prevents corruption from concurrent package downloads

Phase 1: Linting (C++ linting only - catches ISR errors)
    - Runs C++ linting to catch ISR errors before compilation
    - Detects logging macros in FL_IRAM functions (ISR safety)
    - Enforces namespace, include, and code style rules
    - Can be skipped with --skip-lint flag for faster iteration

Phase 2: Compile (NO LOCK - parallelizable)
    - Builds the PlatformIO project for the target environment
    - Uses `pio run -e <environment>` (normal compilation)
    - No lock needed: Each project has isolated build directory
    - Packages already installed, so no downloads triggered
    - Multiple agents can compile different projects simultaneously

Phase 3: Upload
    - Uploads firmware to the attached device using `pio run -t upload`
    - PlatformIO's incremental build system only rebuilds if source files changed
    - Blocks if daemon indicates resource is busy

Phase 4: Monitor
    - Attaches to serial monitor and displays real-time output
    - --expect: Monitors until timeout, exits 0 if ALL regex patterns match, exits 1 if any missing
    - --fail-on: Terminates immediately on regex match, exits 1
    - --exit-on-error: Terminates immediately on regex match (default: \bERROR\b), accepts custom pattern
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
    uv run ci/debug_attached.py                          # Auto-detect env & sketch (default: 20s timeout, waits until timeout)
    uv run ci/debug_attached.py RX                       # Compile RX sketch (examples/RX), auto-detect env
    uv run ci/debug_attached.py RX --env esp32dev        # Compile RX sketch for specific environment
    uv run ci/debug_attached.py examples/RX/RX.ino       # Full path to sketch
    uv run ci/debug_attached.py --verbose                # Verbose mode
    uv run ci/debug_attached.py --skip-lint              # Skip C++ linting phase (faster iteration)
    uv run ci/debug_attached.py --upload-port /dev/ttyUSB0  # Specific port
    uv run ci/debug_attached.py --timeout 120            # Monitor for 120 seconds
    uv run ci/debug_attached.py --timeout 2m             # Monitor for 2 minutes
    uv run ci/debug_attached.py --timeout 5000ms         # Monitor for 5 seconds
    uv run ci/debug_attached.py --exit-on-error          # Exit 1 immediately if \bERROR\b pattern matches (default)
    uv run ci/debug_attached.py --exit-on-error "ClearCommError"  # Exit 1 immediately if custom pattern found
    uv run ci/debug_attached.py --fail-on "PANIC"        # Exit 1 immediately if "PANIC" pattern found
    uv run ci/debug_attached.py --fail-on "ERROR" --fail-on "CRASH"  # Exit 1 on any pattern match
    uv run ci/debug_attached.py --no-fail-on             # Explicitly disable all failure patterns
    uv run ci/debug_attached.py --expect "SUCCESS"       # Exit 0 only if "SUCCESS" pattern found by timeout
    uv run ci/debug_attached.py --expect "PASS" --expect "OK"  # Exit 0 only if ALL patterns found
    uv run ci/debug_attached.py --stream                 # Stream mode (runs until Ctrl+C)
    uv run ci/debug_attached.py --kill-daemon            # Restart daemon before running (useful if stuck)
    uv run ci/debug_attached.py RX --env esp32dev --verbose --upload-port COM3
"""

import argparse
import datetime
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

import psutil
import serial
import serial.tools.list_ports
from colorama import Fore, Style, init
from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream
from serial.tools.list_ports_common import ListPortInfo

from ci.compiler.build_utils import get_utf8_env
from ci.util.build_lock import BuildLock
from ci.util.global_interrupt_handler import notify_main_thread
from ci.util.output_formatter import TimestampFormatter


# Initialize colorama for cross-platform colored output
init(autoreset=True)


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


def auto_detect_upload_port() -> ComportResult:
    """Auto-detect the upload port from available serial ports.

    Only considers USB devices - filters out Bluetooth and other non-USB ports.
    Returns a structured result with success status, selected port, error details,
    and all scanned ports for diagnostics.

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


def run_cpp_lint() -> bool:
    """Run C++ linting to catch ISR errors and other issues.

    Returns:
        True if linting succeeded, False otherwise
    """
    print("=" * 60)
    print("LINTING C++ CODE")
    print("=" * 60)
    print("Running C++ linters (ISR safety, namespace checks, etc.)")
    print()

    # Run C++ linting scripts from ci/lint_cpp/
    project_root = Path(__file__).parent.parent
    lint_cpp_dir = project_root / "ci" / "lint_cpp"

    if not lint_cpp_dir.exists():
        print(f"⚠️  Warning: lint_cpp directory not found: {lint_cpp_dir}")
        return True  # Don't fail if linting isn't available

    lint_scripts = sorted(lint_cpp_dir.glob("*.py"))
    if not lint_scripts:
        print(f"⚠️  Warning: No lint scripts found in {lint_cpp_dir}")
        return True

    all_passed = True
    for lint_script in lint_scripts:
        script_name = lint_script.name
        print(f"Running {script_name}...")

        # Check if it's a unittest-based script
        try:
            with open(lint_script, "r", encoding="utf-8") as f:
                content = f.read()
                is_unittest = "unittest.main()" in content
        except Exception as e:
            print(f"⚠️  Warning: Could not read {script_name}: {e}")
            continue

        try:
            if is_unittest:
                # Run as unittest via pytest
                result = subprocess.run(
                    ["uv", "run", "python", "-m", "pytest", str(lint_script), "-v"],
                    cwd=project_root,
                    capture_output=True,
                    text=True,
                    env=get_utf8_env(),
                )
            else:
                # Run as standalone script
                result = subprocess.run(
                    ["uv", "run", "python", str(lint_script)],
                    cwd=project_root,
                    capture_output=True,
                    text=True,
                    env=get_utf8_env(),
                )

            # Display output (both stdout and stderr)
            if result.stdout:
                print(result.stdout)
            if result.stderr:
                print(result.stderr, file=sys.stderr)

            if result.returncode != 0:
                print(f"❌ Lint check failed: {script_name}")
                all_passed = False
            else:
                print(f"✅ {script_name} passed")

        except KeyboardInterrupt:
            print("\nKeyboardInterrupt: Stopping linting")
            notify_main_thread()
            raise
        except Exception as e:
            print(f"❌ Error running {script_name}: {e}")
            all_passed = False

        print()  # Blank line between scripts

    if all_passed:
        print("✅ All C++ lint checks passed\n")
    else:
        print("❌ Some C++ lint checks failed\n")

    return all_passed


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
        True if compilation succeeded, False otherwise
    """
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

    Args:
        build_dir: Project directory containing platformio.ini
        environment: PlatformIO environment to upload (None = default)
        upload_port: Serial port to use (None = auto-detect)
        verbose: Enable verbose output

    Returns:
        True if upload succeeded, False otherwise.
    """
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
        print("\nKeyboardInterrupt: Stopping upload")
        proc.terminate()
        notify_main_thread()
        raise

    proc.wait()
    success = proc.returncode == 0

    if success:
        print("\n✅ Upload succeeded\n")
    else:
        print(f"\n❌ Upload failed (exit code {proc.returncode})\n")

    return success


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
        fail_keywords: List of regex patterns that trigger immediate termination + exit 1 (default: [r"\bERROR\b"])
        expect_keywords: List of regex patterns that must ALL be found by timeout for exit 0
        stream: If True, monitor runs indefinitely until Ctrl+C (ignores timeout)

    Returns:
        Tuple of (success, output_lines)
    """
    if fail_keywords is None:
        fail_keywords = []
    if expect_keywords is None:
        expect_keywords = []

    # Compile regex patterns for fail and expect keywords
    fail_patterns = []
    for pattern_str in fail_keywords:
        try:
            fail_patterns.append((pattern_str, re.compile(pattern_str)))
        except re.error as e:
            print(f"⚠️  Warning: Invalid regex pattern '{pattern_str}': {e}")
            print(f"   Using literal string match instead")
            # Fallback to escaped literal pattern
            fail_patterns.append((pattern_str, re.compile(re.escape(pattern_str))))

    expect_patterns = []
    for pattern_str in expect_keywords:
        try:
            expect_patterns.append((pattern_str, re.compile(pattern_str)))
        except re.error as e:
            print(f"⚠️  Warning: Invalid regex pattern '{pattern_str}': {e}")
            print(f"   Using literal string match instead")
            # Fallback to escaped literal pattern
            expect_patterns.append((pattern_str, re.compile(re.escape(pattern_str))))
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
    if fail_patterns:
        print(f"Fail patterns: {', '.join(f'/{p}/' for p, _ in fail_patterns)}")
    if expect_patterns:
        print(f"Expect patterns: {', '.join(f'/{p}/' for p, _ in expect_patterns)}")
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
    device_stuck = False

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

            # Read next line with 30 second read timeout
            try:
                line = proc.get_next_line(timeout=30)
                if isinstance(line, EndOfStream):
                    break
                if line:
                    output_lines.append(line)
                    print(line)  # Real-time output

                    # Check for expect patterns (track but don't terminate)
                    if expect_patterns:
                        for pattern_str, pattern_re in expect_patterns:
                            if pattern_re.search(line):
                                # Track this expected line
                                expect_lines.append((pattern_str, line))

                                # Mark this pattern as found
                                if pattern_str not in found_expect_keywords:
                                    found_expect_keywords.add(pattern_str)
                                    print(
                                        f"\n✅ EXPECT PATTERN DETECTED: /{pattern_str}/"
                                    )
                                    print(f"   Matched line: {line}")
                                    print(
                                        f"   (Continuing to monitor - need all {len(expect_patterns)} patterns)\n"
                                    )

                    # Check for fail patterns - TERMINATE IMMEDIATELY
                    if fail_patterns and not fail_keyword_found:
                        for pattern_str, pattern_re in fail_patterns:
                            if pattern_re.search(line):
                                # Track this failing line
                                failing_lines.append((pattern_str, line))

                                if not fail_keyword_found:
                                    # First failure - terminate immediately
                                    fail_keyword_found = True
                                    matched_fail_keyword = pattern_str
                                    matched_fail_line = line
                                    print(
                                        f"\n🚨 FAIL PATTERN DETECTED: /{pattern_str}/"
                                    )
                                    print(f"   Matched line: {line}")
                                    print(f"   Terminating monitor immediately...\n")
                                    proc.terminate()
                                break

            except TimeoutError:
                # No output within read timeout - continue waiting (will check timeouts on next loop)
                continue
            except Exception as e:
                # Check for specific serial port errors indicating device is stuck
                error_str = str(e)
                if "ClearCommError" in error_str and "PermissionError" in error_str:
                    device_stuck = True
                    print(
                        "\n🚨 CRITICAL ERROR: Device appears stuck and no longer responding"
                    )
                    print(
                        "   Serial port failure: ClearCommError (device not recognizing commands)"
                    )
                    print("   Possible causes:")
                    print("     - ISR (Interrupt Service Routine) hogging CPU time")
                    print("     - Device crashed or entered non-responsive state")
                    print("     - Hardware watchdog triggered")
                    print(f"   Technical details: {error_str}")
                    proc.terminate()
                    break
                # Re-raise other exceptions
                raise

    except KeyboardInterrupt:
        print("\nKeyboardInterrupt: Stopping monitor")
        proc.terminate()
        notify_main_thread()
        raise

    proc.wait()

    # Determine success based on exit conditions
    if device_stuck:
        # Device stuck is always a failure
        success = False
    elif fail_keyword_found:
        # Fail pattern match always means failure
        success = False
    elif expect_patterns:
        # If expect patterns were specified, ALL must be found for success
        missing_patterns = set(p for p, _ in expect_patterns) - found_expect_keywords
        if missing_patterns:
            # Not all expect patterns found - failure
            success = False
        else:
            # All expect patterns found - success
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

    if device_stuck:
        print("❌ Monitor failed - device appears stuck and no longer responding")
        print("   WARNING: Serial port communication failed (ClearCommError)")
        print(
            "   This typically indicates an ISR is hogging CPU time or device crashed"
        )
    elif fail_keyword_found:
        print(f"❌ Monitor failed - fail pattern /{matched_fail_keyword}/ detected")
        print(f"   Matched line: {matched_fail_line}")
    elif expect_patterns:
        # Check if all expect patterns were found
        missing_patterns = set(p for p, _ in expect_patterns) - found_expect_keywords
        if missing_patterns:
            print(f"❌ Monitor failed - not all expect patterns found")
            print(
                f"   Expected {len(expect_patterns)} patterns, found {len(found_expect_keywords)}"
            )
            # Display missing patterns clearly
            if len(missing_patterns) == 1:
                print(f"   Missing pattern: /{list(missing_patterns)[0]}/")
            else:
                print(f"   Missing patterns:")
                for pattern in sorted(missing_patterns):
                    print(f"     - /{pattern}/")
            # Display found patterns if any
            if found_expect_keywords:
                if len(found_expect_keywords) == 1:
                    print(f"   Found pattern: /{list(found_expect_keywords)[0]}/")
                else:
                    print(f"   Found patterns:")
                    for pattern in sorted(found_expect_keywords):
                        print(f"     - /{pattern}/")
        else:
            print(
                f"✅ Monitor succeeded - all {len(expect_patterns)} expect patterns found"
            )
            if len(expect_patterns) == 1:
                print(f"   Pattern: /{expect_patterns[0][0]}/")
            else:
                print(f"   Patterns:")
                for pattern_str, _ in sorted(expect_patterns):
                    print(f"     - /{pattern_str}/")
    elif timeout_reached:
        print(f"✅ Monitor completed successfully (timeout reached after {timeout}s)")
    elif stream and success:
        print("✅ Monitor completed successfully (streaming mode ended)")
    elif success:
        print("✅ Monitor completed successfully")
    else:
        print(f"❌ Monitor failed (exit code {proc.returncode})")

    return success, output_lines


def log_port_cleanup(message: str) -> None:
    """Log port cleanup operations to .logs/debug_attached/port_cleanup.log.

    Args:
        message: Log message to write
    """
    try:
        # Get project root (where .logs directory is located)
        project_root = Path(__file__).parent.parent
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
        print(f"   Waiting 2 seconds for OS to release {port}...")
        time.sleep(2)
        print(f"✅ Port cleanup completed")
        log_port_cleanup(f"Port cleanup completed for {port}")
    else:
        print(f"✅ No orphaned processes found using {port}")
        log_port_cleanup(f"Port cleanup for {port}: No orphaned processes found")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Upload and monitor attached PlatformIO device",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Auto-detect env & sketch (default: 20s timeout, waits until timeout)
  %(prog)s RX                       # Compile RX sketch (examples/RX), auto-detect environment
  %(prog)s RX --env esp32dev        # Compile RX sketch for specific environment
  %(prog)s examples/RX              # Same as above (explicit path)
  %(prog)s examples/deep/nested/Sketch  # Deep nested sketch
  %(prog)s --verbose                # Verbose mode
  %(prog)s --skip-lint              # Skip C++ linting (faster, but may miss ISR errors)
  %(prog)s --upload-port /dev/ttyUSB0  # Specific port
  %(prog)s --timeout 120            # Monitor for 120 seconds (default: 20s)
  %(prog)s --timeout 2m             # Monitor for 2 minutes
  %(prog)s --timeout 5000ms         # Monitor for 5 seconds
  %(prog)s --exit-on-error          # Exit 1 immediately if \bERROR\b pattern matches (default)
  %(prog)s --exit-on-error "ClearCommError"  # Exit 1 immediately if custom pattern found
  %(prog)s --fail-on "PANIC"        # Exit 1 immediately if "PANIC" pattern found
  %(prog)s --fail-on "ERROR" --fail-on "CRASH"  # Exit 1 on any pattern match
  %(prog)s --no-fail-on             # Explicitly disable all failure patterns
  %(prog)s --expect "SUCCESS"       # Exit 0 only if "SUCCESS" pattern found by timeout
  %(prog)s --expect "PASS" --expect "OK"  # Exit 0 only if ALL patterns found
  %(prog)s --stream                 # Stream mode (runs until Ctrl+C)
  %(prog)s RX --env esp32dev --verbose --upload-port COM3
        """,
    )

    parser.add_argument(
        "sketch",
        nargs="?",
        help="Sketch to compile (e.g., 'RX', 'examples/RX', 'examples/RX/RX.ino'). "
        "Sets PLATFORMIO_SRC_DIR environment variable. Supports full paths and deep sketches.",
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
        "--skip-lint",
        action="store_true",
        help="Skip C++ linting phase (faster iteration, but may miss ISR errors)",
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
        "--exit-on-error",
        nargs="?",
        const=r"\bERROR\b",
        metavar="PATTERN",
        help=r"Exit 1 immediately if pattern found. Default pattern: \bERROR\b (word boundary). Accepts custom regex pattern.",
    )
    parser.add_argument(
        "--fail-on",
        "-f",
        action="append",
        dest="fail_keywords",
        help="Regex pattern that triggers immediate termination + exit 1 if matched (can be specified multiple times)",
    )
    parser.add_argument(
        "--no-fail-on",
        action="store_true",
        help="Explicitly disable all --fail-on patterns",
    )
    parser.add_argument(
        "--expect",
        "-x",
        action="append",
        dest="expect_keywords",
        help="Regex pattern that must be matched by timeout for exit 0. ALL patterns must be found (can be specified multiple times). Monitor runs until timeout to find all.",
    )
    parser.add_argument(
        "--stream",
        "-s",
        action="store_true",
        help="Stream mode: monitor runs indefinitely until Ctrl+C (ignores timeout)",
    )
    parser.add_argument(
        "--kill-daemon",
        action="store_true",
        help="Stop and restart the package daemon before running (useful if daemon is stuck)",
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
    if args.no_fail_on and (args.fail_keywords or args.exit_on_error):
        print(
            "❌ Error: Cannot specify both --no-fail-on and --fail-on/--exit-on-error"
        )
        return 1

    # Set default fail patterns (default: wait until timeout, no immediate fail)
    if args.no_fail_on:
        # Explicitly disable all fail patterns
        fail_keywords: list[str] = []
    else:
        # Start with empty list (new default: no immediate fail)
        fail_keywords: list[str] = []

        # Add regex pattern if --exit-on-error specified (with optional custom pattern)
        if args.exit_on_error:
            fail_keywords.append(args.exit_on_error)

        # Add custom patterns from --fail-on
        if args.fail_keywords:
            fail_keywords.extend(args.fail_keywords)

    # Parse timeout string with suffix support
    try:
        timeout_seconds = parse_timeout(args.timeout)
    except ValueError as e:
        print(f"❌ Error: {e}")
        return 1

    # Auto-detect upload port if not specified
    upload_port = args.upload_port
    if not upload_port:
        result = auto_detect_upload_port()
        if not result.ok:
            # Port detection failed - display detailed error and exit
            print(Fore.RED + "=" * 60)
            print(Fore.RED + "⚠️  FATAL ERROR: PORT DETECTION FAILED")
            print(Fore.RED + "=" * 60)
            print(f"\n{Fore.RED}{result.error_message}{Style.RESET_ALL}\n")

            # Display all scanned ports for diagnostics
            if result.all_ports:
                print("Available ports (all non-USB):")
                for port in result.all_ports:
                    print(f"  {port.device}: {port.description}")
                    print(f"    hwid: {port.hwid}")
            else:
                print("No serial ports detected on system.")

            print(
                f"\n{Fore.RED}Only USB devices are supported. Please connect a USB device and try again.{Style.RESET_ALL}"
            )
            print(
                f"{Fore.RED}Note: Bluetooth serial ports (BTHENUM) are not supported.{Style.RESET_ALL}\n"
            )
            return 1

        # Port detection succeeded
        upload_port = result.selected_port

    print("FastLED Debug Attached Device")
    print("=" * 60)
    print(f"Project: {build_dir}")
    if args.environment:
        print(f"Environment: {args.environment}")
    print(f"Upload port: {upload_port}")
    print("=" * 60)
    print()

    try:
        # Handle --kill-daemon flag (restart daemon before proceeding)
        if args.kill_daemon:
            from ci.util.pio_package_client import is_daemon_running, stop_daemon

            print("🔄 Restarting package daemon...")
            if is_daemon_running():
                print("   Stopping existing daemon...")
                stop_daemon()
                time.sleep(1)  # Brief delay after stop
                print("   ✅ Daemon stopped")
            else:
                print("   ℹ️  Daemon not running")
            print("   Daemon will auto-start during package installation")
            print()

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
        # PHASE 1: Lint C++ Code (catches ISR errors before compile)
        # ============================================================
        if not args.skip_lint:
            if not run_cpp_lint():
                print("\n❌ Linting failed. Fix issues or use --skip-lint to bypass.")
                return 1
        else:
            print("⚠️  Skipping C++ linting (--skip-lint flag set)")
            print()

        # ============================================================
        # PHASE 2: Compile (NO LOCK - parallelizable)
        # ============================================================
        # No lock needed: each project has isolated build directory.
        # Multiple agents can compile different projects simultaneously.

        if not run_compile(build_dir, args.environment, args.verbose):
            return 1

        # ============================================================
        # PHASES 3-4: Upload + Monitor (DEVICE LOCK)
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
            # Clean up orphaned processes holding the serial port
            # This MUST happen inside the lock to prevent race conditions
            if upload_port:
                kill_port_users(upload_port)

            # Phase 3: Upload firmware (with incremental rebuild if needed)
            if not run_upload(build_dir, args.environment, upload_port, args.verbose):
                return 1

            # Phase 4: Monitor serial output
            success, output = run_monitor(
                build_dir,
                args.environment,
                upload_port,  # Use same port for monitoring
                args.verbose,
                timeout_seconds,
                fail_keywords,
                args.expect_keywords or [],
                args.stream,
            )

            if not success:
                return 1

            phases_completed = "three" if args.skip_lint else "four"
            print(f"\n✅ All {phases_completed} phases completed successfully")
            return 0

        finally:
            # Always release the device lock
            lock.release()

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
