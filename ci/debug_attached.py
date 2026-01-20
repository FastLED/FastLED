#!/usr/bin/env python3
"""Four-phase device workflow: Package Install → Lint → Compile → Upload+Monitor.

⚠️ NOTE FOR AI AGENTS: For live device testing, prefer 'bash validate' which provides
a complete hardware validation framework. This script is for advanced/custom workflows.

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
    - --stop: Early exit on regex match, exits 0 if all expects found (saves time on long tests)
    - --exit-on-error: Terminates immediately on regex match, exits 1 (can specify multiple patterns)
    - Provides output summary (first/last 100 lines)

Concurrency Control:
    - Device locking is handled by fbuild (daemon-based)
    - No legacy file-based locking required

Usage:
    ⚠️ AI agents should use 'bash validate' for device testing (see CLAUDE.md)

    uv run ci/debug_attached.py                          # Auto-detect env & sketch (default: 60s timeout, waits until timeout)
    uv run ci/debug_attached.py RX                       # Compile RX sketch (examples/RX), auto-detect env
    uv run ci/debug_attached.py RX --env esp32dev        # Compile RX sketch for specific environment
    uv run ci/debug_attached.py examples/RX/RX.ino       # Full path to sketch
    uv run ci/debug_attached.py --verbose                # Verbose mode
    uv run ci/debug_attached.py --skip-lint              # Skip C++ linting phase (faster iteration)
    uv run ci/debug_attached.py --upload-port /dev/ttyUSB0  # Specific port
    uv run ci/debug_attached.py --timeout 120            # Monitor for 120 seconds
    uv run ci/debug_attached.py --timeout 2m             # Monitor for 2 minutes
    uv run ci/debug_attached.py --timeout 5000ms         # Monitor for 5 seconds
    uv run ci/debug_attached.py --exit-on-error "ERROR"  # Exit 1 immediately if ERROR pattern found
    uv run ci/debug_attached.py --exit-on-error "ClearCommError" --exit-on-error "register dump"  # Multiple exit patterns
    uv run ci/debug_attached.py --fail-on "PANIC"        # Exit 1 immediately if "PANIC" pattern found
    uv run ci/debug_attached.py --fail-on "ERROR" --fail-on "CRASH"  # Exit 1 on any pattern match
    uv run ci/debug_attached.py --no-fail-on             # Explicitly disable all failure patterns
    uv run ci/debug_attached.py --expect "SUCCESS"       # Exit 0 only if "SUCCESS" pattern found by timeout
    uv run ci/debug_attached.py --expect "PASS" --expect "OK"  # Exit 0 only if ALL patterns found
    uv run ci/debug_attached.py --stop "TEST COMPLETE"   # Early exit when pattern found (success if all expects found)
    uv run ci/debug_attached.py --expect "READY" --stop "FINISHED"  # Exit early when FINISHED found (after READY matched)
    uv run ci/debug_attached.py --stream                 # Stream mode (runs until Ctrl+C)
    uv run ci/debug_attached.py --kill-daemon            # Restart daemon before running (useful if stuck)
    uv run ci/debug_attached.py RX --env esp32dev --verbose --upload-port COM3
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import serial
from colorama import Fore, Style, init
from running_process.process_output_reader import EndOfStream

from ci.compiler.build_utils import get_utf8_env
from ci.util.global_interrupt_handler import (
    handle_keyboard_interrupt_properly,
    is_interrupted,
)
from ci.util.json_rpc_handler import JsonRpcHandler
from ci.util.output_formatter import TimestampFormatter
from ci.util.pio_runner import create_pio_process
from ci.util.port_utils import auto_detect_upload_port, kill_port_users
from ci.util.sketch_resolver import parse_timeout, resolve_sketch_path


# Initialize colorama for cross-platform colored output
init(autoreset=True)


# ============================================================
# PlatformIO Workflow Functions
# ============================================================


def run_cpp_lint() -> bool:
    """Run C++ linting using unified runner (matches 'bash lint' behavior).

    This function mirrors the C++ linting approach from the 'lint' script to ensure
    consistent behavior between 'bash validate' and 'bash lint'.

    Returns:
        True if linting succeeded, False otherwise
    """
    print("=" * 60)
    print("LINTING C++ CODE")
    print("=" * 60)
    print("Running C++ linters (unified runner - matches 'bash lint')")
    print()

    project_root = Path(__file__).parent.parent

    try:
        # Phase 1: Run unified C++ checker (all content-based linters in one pass)
        # This is the same as 'bash lint' line 200
        print("🔍 Running unified C++ checker (all content-based linters in one pass)")
        result = subprocess.run(
            ["uv", "run", "python", "ci/lint_cpp/run_all_checkers.py"],
            cwd=project_root,
            env=get_utf8_env(),
        )
        if result.returncode != 0:
            print("❌ Unified C++ linter failed")
            return False

        print()

        # Phase 3: Run relative includes check for src/ only
        # This is the same as 'bash lint' line 219
        print("🔗 Running relative includes check (cpp_lint.py for src/ only)")
        result = subprocess.run(
            ["uv", "run", "python", "cpp_lint.py", "src/"],
            cwd=project_root,
            env=get_utf8_env(),
        )
        if result.returncode != 0:
            print("❌ cpp_lint.py failed: Found relative includes in src/")
            return False

        print()
        print("✅ All C++ lint checks passed\n")
        return True

    except KeyboardInterrupt:
        print("\nKeyboardInterrupt: Stopping linting")
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"❌ Error running C++ linting: {e}")
        return False


def run_compile(
    build_dir: Path,
    environment: str | None = None,
    verbose: bool = False,
) -> bool:
    """Compile the PlatformIO project.

    On Windows Git Bash: Runs via cmd.exe with clean environment (no Git Bash indicators)
    On other platforms: Runs directly with UTF-8 environment

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
    # Use create_pio_process() for Git Bash compatibility
    proc = create_pio_process(
        cmd,
        cwd=build_dir,
        output_formatter=formatter,
        auto_run=True,
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
        handle_keyboard_interrupt_properly()
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

    On Windows Git Bash: Runs via cmd.exe with clean environment (no Git Bash indicators)
    On other platforms: Runs directly with UTF-8 environment

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
    # Use create_pio_process() for Git Bash compatibility
    proc = create_pio_process(
        cmd,
        cwd=build_dir,
        output_formatter=formatter,
        auto_run=True,
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
        handle_keyboard_interrupt_properly()
        raise

    proc.wait()
    success = proc.returncode == 0

    if success:
        print("\n✅ Upload succeeded\n")
    else:
        print(f"\n❌ Upload failed (exit code {proc.returncode})\n")

    return success


def format_test_summary(result_events: list[dict[str, Any]]) -> str:
    """Format RESULT JSON events into compact test summary.

    Extracts meaningful test results from the RESULT JSON lines emitted by
    the Validation firmware and formats them into a concise summary.

    Args:
        result_events: List of parsed RESULT JSON events

    Returns:
        Formatted summary string, or empty string if no results
    """
    if not result_events:
        return ""

    # Find case_result events (one per test case)
    case_results = [e for e in result_events if e.get("type") == "case_result"]

    if not case_results:
        return ""

    # Extract info from first case (they should all be same driver)
    first = case_results[0]
    driver = first.get("driver", "Unknown")
    lanes = first.get("laneCount", 1)
    strip_size = first.get("stripSize", 0)

    total_tests = sum(c.get("totalTests", 0) for c in case_results)
    passed_tests = sum(c.get("passedTests", 0) for c in case_results)
    all_passed = all(c.get("passed", False) for c in case_results)

    # Build compact summary
    lane_str = "lane" if lanes == 1 else "lanes"
    lines = [
        f"{driver} Driver Test Results",
        f"  Config    {lanes} {lane_str} × {strip_size} LEDs",
        f"  Patterns  {total_tests} bit patterns tested",
    ]

    if all_passed:
        lines.append(f"  Tests     {passed_tests}/{total_tests} PASSED")
    else:
        failed = total_tests - passed_tests
        mismatch_str = "mismatch" if failed == 1 else "mismatches"
        lines.append(
            f"  Tests     {passed_tests}/{total_tests} FAILED ({failed} {mismatch_str})"
        )

    return "\n".join(lines)


def run_monitor(
    build_dir: Path,
    environment: str | None = None,
    monitor_port: str | None = None,
    verbose: bool = False,
    timeout: int = 80,
    fail_keywords: list[str] | None = None,
    expect_keywords: list[str] | None = None,
    stream: bool = False,
    input_on_trigger: str | None = None,
    device_error_keywords: list[str] | None = None,
    stop_keyword: str | None = None,
    json_rpc_commands: list[dict[str, Any]] | None = None,
) -> tuple[bool, list[str], JsonRpcHandler]:
    """Attach to serial monitor and capture output.

    Keyword Behavior:
        --expect: Monitors until timeout, then checks if ALL keywords were found.
                  Exit 0 if all found, exit 1 if any missing.
        --fail-on: Terminates immediately when ANY keyword is found, exits 1.
        --stop: Terminates immediately when keyword is found, exits 0 if all expect patterns found.
        --input-on-trigger: Wait for trigger pattern, then send text to serial.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: PlatformIO environment to monitor (None = default)
        monitor_port: Serial port to monitor (None = auto-detect)
        verbose: Enable verbose output
        timeout: Maximum time to monitor in seconds (default: 60)
        fail_keywords: List of regex patterns that trigger immediate termination + exit 1 (default: [r"\bERROR\b"])
        expect_keywords: List of regex patterns that must ALL be found by timeout for exit 0
        stream: If True, monitor runs indefinitely until Ctrl+C (ignores timeout)
        input_on_trigger: Format "PATTERN:TEXT" - sends TEXT when PATTERN is detected
        device_error_keywords: List of error keywords in serial exceptions that indicate device stuck
                               (default: ["ClearCommError", "PermissionError"])
        stop_keyword: Regex pattern that triggers early successful exit if all expect patterns found
        json_rpc_commands: List of JSON-RPC commands to send to device at startup (before trigger)

    Returns:
        Tuple of (success, output_lines, json_rpc_handler)
    """
    if fail_keywords is None:
        fail_keywords = []
    if expect_keywords is None:
        expect_keywords = []
    if device_error_keywords is None:
        device_error_keywords = ["ClearCommError", "PermissionError"]
    if json_rpc_commands is None:
        json_rpc_commands = []

    # Initialize JSON-RPC handler
    rpc_handler = JsonRpcHandler()
    rpc_commands_sent = False  # Track if RPC commands have been sent
    rpc_responses_needed = len(json_rpc_commands)  # Number of RPC responses to wait for
    rpc_responses_received = 0

    # Parse input-on-trigger argument
    trigger_pattern = None
    trigger_text = None
    trigger_timeout_seconds = None
    if input_on_trigger:
        if ":" not in input_on_trigger:
            print(
                f"⚠️  Warning: Invalid --input-on-trigger format: '{input_on_trigger}'"
            )
            print("   Expected format: 'PATTERN:TEXT' or 'PATTERN:TEXT:TIMEOUT'")
            print("   Example: 'VALIDATION_READY:START' or 'VALIDATION_READY:START:10'")
            print("   Ignoring input-on-trigger")
        else:
            parts = input_on_trigger.split(":", 2)
            trigger_pattern_str = parts[0]
            trigger_text = parts[1] if len(parts) > 1 else ""

            # Parse optional timeout for trigger (3rd field)
            if len(parts) >= 3 and parts[2]:
                try:
                    trigger_timeout_seconds = float(parts[2])
                except ValueError:
                    print(
                        f"⚠️  Warning: Invalid timeout value '{parts[2]}' in input-on-trigger"
                    )
                    print("   Expected numeric value (e.g., '10' or '10.5')")
                    print("   Ignoring trigger timeout")
                    trigger_timeout_seconds = None

            try:
                trigger_pattern = re.compile(trigger_pattern_str)
                if trigger_timeout_seconds is not None:
                    print(
                        f"📥 Input-on-trigger enabled: Will send '{trigger_text}' when pattern /{trigger_pattern_str}/ is detected"
                    )
                    print(
                        f"   Trigger timeout: {trigger_timeout_seconds}s (will fail if pattern not found)"
                    )
                else:
                    print(
                        f"📥 Input-on-trigger enabled: Will send '{trigger_text}' when pattern /{trigger_pattern_str}/ is detected"
                    )
            except re.error as e:
                print(
                    f"⚠️  Warning: Invalid regex pattern in input-on-trigger '{trigger_pattern_str}': {e}"
                )
                print("   Using literal string match instead")
                trigger_pattern = re.compile(re.escape(trigger_pattern_str))

    # Compile regex patterns for fail and expect keywords
    fail_patterns: list[tuple[str, re.Pattern[str]]] = []
    for pattern_str in fail_keywords:
        try:
            fail_patterns.append((pattern_str, re.compile(pattern_str)))
        except re.error as e:
            print(f"⚠️  Warning: Invalid regex pattern '{pattern_str}': {e}")
            print("   Using literal string match instead")
            # Fallback to escaped literal pattern
            fail_patterns.append((pattern_str, re.compile(re.escape(pattern_str))))

    expect_patterns: list[tuple[str, re.Pattern[str]]] = []
    for pattern_str in expect_keywords:
        try:
            expect_patterns.append((pattern_str, re.compile(pattern_str)))
        except re.error as e:
            print(f"⚠️  Warning: Invalid regex pattern '{pattern_str}': {e}")
            print("   Using literal string match instead")
            # Fallback to escaped literal pattern
            expect_patterns.append((pattern_str, re.compile(re.escape(pattern_str))))

    # Compile stop pattern if provided
    stop_pattern = None
    if stop_keyword:
        try:
            stop_pattern = re.compile(stop_keyword)
        except re.error as e:
            print(f"⚠️  Warning: Invalid regex pattern in --stop '{stop_keyword}': {e}")
            print("   Using literal string match instead")
            stop_pattern = re.compile(re.escape(stop_keyword))
    # Only show detailed config in verbose mode; otherwise show compact summary
    if verbose:
        print("─" * 60)
        print("MONITORING CONFIGURATION")
        print("─" * 60)
        print(f"  Port: {monitor_port if monitor_port else 'AUTO-DETECT'}")
        print(
            f"  Mode: {'STREAMING (runs until Ctrl+C)' if stream else f'TIMEOUT ({timeout}s)'}"
        )

        if fail_patterns:
            print(f"  Fail patterns: {len(fail_patterns)}")
        if expect_patterns:
            print(f"  Expect patterns: {len(expect_patterns)}")
        if stop_pattern:
            print(f"  Stop pattern: /{stop_pattern.pattern}/")
        if json_rpc_commands:
            print(f"  JSON-RPC commands: {len(json_rpc_commands)}")
        if trigger_pattern:
            print(f"  Trigger: send '{trigger_text}' on /{trigger_pattern.pattern}/")
        print("─" * 60)
    else:
        # Compact one-line summary for non-verbose mode
        parts = [f"Port: {monitor_port}"]
        if not stream:
            parts.append(f"Timeout: {timeout}s")
        if expect_patterns:
            parts.append(f"Expect: {len(expect_patterns)}")
        if fail_patterns:
            parts.append(f"Fail: {len(fail_patterns)}")
        if trigger_pattern:
            parts.append(f"Trigger: /{trigger_pattern.pattern}/")
        print(f"  {' | '.join(parts)}")

    # Open serial port directly for full read/write control
    if not monitor_port:
        # Auto-detect serial port
        from serial.tools import list_ports

        ports = list(list_ports.comports())
        if not ports:
            print("❌ No serial ports found")
            return False, [], JsonRpcHandler()
        monitor_port = ports[0].device
        print(f"📡 Auto-detected serial port: {monitor_port}")

    try:
        ser = serial.Serial(
            port=monitor_port,
            baudrate=115200,
            timeout=0.1,  # Non-blocking read with 100ms timeout
            write_timeout=1.0,
        )
        print(f"\n✓ Serial port opened: {monitor_port} (115200 baud)")
    except serial.SerialException as e:
        print(f"❌ Failed to open serial port {monitor_port}: {e}")
        return False, [], JsonRpcHandler()

    # Print compact monitor start message
    print("\n" + "─" * 25 + " SERIAL MONITOR " + "─" * 19)

    formatter = TimestampFormatter()
    formatter.begin()  # Start the timestamp timer

    output_lines: list[str] = []
    result_events: list[
        dict[str, Any]
    ] = []  # Track RESULT JSON events for test summary
    failing_lines: list[
        tuple[str, str]
    ] = []  # Track all lines containing fail keywords
    expect_lines: list[
        tuple[str, str]
    ] = []  # Track all lines containing expect keywords
    found_expect_keywords: set[str] = (
        set()
    )  # Track which expect keywords have been found
    start_time = time.time()
    fail_keyword_found = False
    matched_fail_keyword = None
    matched_fail_line = None
    timeout_reached = False
    device_stuck = False
    trigger_sent = False  # Track if input-on-trigger text has been sent
    trigger_deadline = None  # Deadline for trigger timeout
    trigger_timeout_triggered = False  # Track if trigger timeout occurred
    stop_keyword_found = False  # Track if stop keyword was found
    line_buffer = ""  # Buffer for incomplete lines

    # Set trigger deadline if trigger timeout is configured
    if trigger_pattern and trigger_timeout_seconds:
        trigger_deadline = start_time + trigger_timeout_seconds

    try:
        while True:
            # Check for interrupt flag (Windows Ctrl+C workaround)
            if is_interrupted():
                raise KeyboardInterrupt()

            # Send JSON-RPC commands immediately if not yet sent
            # This happens before waiting for trigger pattern
            if not rpc_commands_sent and json_rpc_commands:
                rpc_commands_sent = True
                print(
                    f"\n🔧 Sending {len(json_rpc_commands)} JSON-RPC command(s) to device..."
                )
                for i, cmd in enumerate(json_rpc_commands, 1):
                    from ci.util.json_rpc_handler import format_json_rpc_command

                    cmd_str = format_json_rpc_command(cmd)
                    try:
                        ser.write(f"{cmd_str}\n".encode("utf-8"))
                        ser.flush()
                        print(
                            f"   ✓ Sent command {i}/{len(json_rpc_commands)}: {cmd['function']}()"
                        )
                    except KeyboardInterrupt:
                        handle_keyboard_interrupt_properly()
                        raise
                    except Exception as e:
                        print(f"   ⚠️  Warning: Failed to send RPC command {i}: {e}")
                print(f"   ⏳ Waiting for {rpc_responses_needed} response(s)...\n")

            # Check trigger timeout (higher priority than general timeout)
            if trigger_deadline and not trigger_sent and time.time() > trigger_deadline:
                trigger_timeout_triggered = True
                # Print red error banner
                banner_width = 62
                border = "═" * (banner_width - 2)
                print(f"\n{Fore.RED}╔{border}╗")
                print("║ DEVICE APPEARS STUCK!!!                                    ║")
                print("║ DID NOT RESPOND TO A COMMAND TO START!!!                   ║")
                print("║ PLEASE ASK THE USER TO POWER CYCLE THE DEVICE!!!           ║")
                print(f"╚{border}╝{Style.RESET_ALL}\n")
                break

            # Check timeout (unless in streaming mode)
            if not stream:
                elapsed = time.time() - start_time
                if elapsed >= timeout:
                    print(f"\n⏱️  Timeout reached ({timeout}s), stopping monitor...")
                    timeout_reached = True
                    break

            # Read from serial port
            try:
                if ser.in_waiting > 0:
                    # Read available data
                    data = ser.read(ser.in_waiting)
                    try:
                        text = data.decode("utf-8", errors="replace")
                    except KeyboardInterrupt:
                        handle_keyboard_interrupt_properly()
                        raise
                    except Exception:
                        text = data.decode("latin-1", errors="replace")

                    # Add to line buffer
                    line_buffer += text

                    # Process complete lines
                    while "\n" in line_buffer:
                        line, line_buffer = line_buffer.split("\n", 1)
                        line = line.rstrip("\r")  # Remove carriage return

                        # Format with timestamp using the formatter's transform method
                        formatted_line = formatter.transform(line)

                        if formatted_line:
                            output_lines.append(
                                formatted_line
                            )  # Store timestamped line
                            print(formatted_line)  # Display with timestamp

                            # Parse RESULT JSON lines for test summary
                            if "RESULT: " in formatted_line:
                                try:
                                    json_str = formatted_line.split("RESULT: ", 1)[1]
                                    event = json.loads(json_str)
                                    result_events.append(event)
                                except (json.JSONDecodeError, IndexError):
                                    pass  # Not valid JSON, skip

                            # Process JSON-RPC responses (REMOTE: prefix)
                            rpc_response = rpc_handler.process_line(formatted_line)
                            if rpc_response:
                                rpc_responses_received += 1
                                func_name = rpc_response.get("function", "unknown")
                                result = rpc_response.get("result")
                                print(f"   📡 RPC Response: {func_name}() → {result}")
                                if (
                                    rpc_responses_received >= rpc_responses_needed
                                    and rpc_responses_needed > 0
                                ):
                                    print(
                                        f"   ✅ All {rpc_responses_needed} RPC response(s) received\n"
                                    )

                            # Check for expect patterns (track but don't terminate)
                            if expect_patterns:
                                for pattern_str, pattern_re in expect_patterns:
                                    if pattern_re.search(formatted_line):
                                        # Track this expected line
                                        expect_lines.append(
                                            (pattern_str, formatted_line)
                                        )

                                        # Mark this pattern as found
                                        if pattern_str not in found_expect_keywords:
                                            found_expect_keywords.add(pattern_str)
                                            # Compact pattern progress indicator
                                            found = len(found_expect_keywords)
                                            total = len(expect_patterns)
                                            print(
                                                f"  ✓ [{found}/{total}] /{pattern_str}/"
                                            )

                            # Check for input-on-trigger pattern
                            if trigger_pattern and not trigger_sent:
                                if trigger_pattern.search(formatted_line):
                                    trigger_sent = True
                                    # Send text to serial with newline
                                    try:
                                        ser.write(f"{trigger_text}\n".encode("utf-8"))
                                        ser.flush()
                                        print(
                                            f"  → Trigger matched, sent '{trigger_text}'"
                                        )
                                    except KeyboardInterrupt:
                                        handle_keyboard_interrupt_properly()
                                        raise
                                    except Exception as e:
                                        print(f"  ⚠ Failed to send trigger: {e}")

                            # Check for stop pattern - EARLY EXIT (success if all expects found)
                            if stop_pattern and not stop_keyword_found:
                                if stop_pattern.search(formatted_line):
                                    stop_keyword_found = True
                                    if expect_patterns:
                                        missing = (
                                            set(p for p, _ in expect_patterns)
                                            - found_expect_keywords
                                        )
                                        if missing:
                                            print(
                                                f"  ⚠ Stop pattern found, missing {len(missing)} expect pattern(s)"
                                            )
                                        else:
                                            print(
                                                f"\n  ✓ Complete - all {len(expect_patterns)} patterns matched"
                                            )
                                            break
                                    else:
                                        print(f"\n  ✓ Stop pattern matched")
                                        break

                            # Check for fail patterns - TERMINATE IMMEDIATELY
                            if fail_patterns and not fail_keyword_found:
                                for pattern_str, pattern_re in fail_patterns:
                                    if pattern_re.search(formatted_line):
                                        # Track this failing line
                                        failing_lines.append(
                                            (pattern_str, formatted_line)
                                        )

                                        if not fail_keyword_found:
                                            # First failure - terminate immediately
                                            fail_keyword_found = True
                                            matched_fail_keyword = pattern_str
                                            matched_fail_line = formatted_line
                                            print(
                                                f"\n🚨 FAIL PATTERN DETECTED: /{pattern_str}/"
                                            )
                                            print(f"   Matched line: {formatted_line}")
                                            print(
                                                "   Terminating monitor immediately...\n"
                                            )
                                            break

                                # Break outer loop if fail found
                                if fail_keyword_found:
                                    break

                            # Break inner loop if stop keyword found and should exit
                            if stop_keyword_found:
                                if not expect_patterns or (
                                    set(p for p, _ in expect_patterns)
                                    <= found_expect_keywords
                                ):
                                    break

                        # Break outer loop if stop keyword found and all expects matched
                        if stop_keyword_found:
                            if not expect_patterns or (
                                set(p for p, _ in expect_patterns)
                                <= found_expect_keywords
                            ):
                                break
                else:
                    # No data available, small sleep to prevent busy loop
                    time.sleep(0.01)

                # Break outer loop if stop keyword found and all expects matched
                # This check MUST be after the if/else for ser.in_waiting
                if stop_keyword_found:
                    if not expect_patterns or (
                        set(p for p, _ in expect_patterns) <= found_expect_keywords
                    ):
                        break

            except serial.SerialException as e:
                # Check for specific serial port errors indicating device is stuck
                error_str = str(e)
                if any(keyword in error_str for keyword in device_error_keywords):
                    device_stuck = True
                    matched_keywords = [
                        kw for kw in device_error_keywords if kw in error_str
                    ]
                    print(
                        "\n🚨 CRITICAL ERROR: Device appears stuck and no longer responding"
                    )
                    print(
                        f"   Serial port failure: {', '.join(matched_keywords)} (device not recognizing commands)"
                    )
                    print("   Possible causes:")
                    print("     - ISR (Interrupt Service Routine) hogging CPU time")
                    print("     - Device crashed or entered non-responsive state")
                    print("     - Hardware watchdog triggered")
                    print(f"   Technical details: {error_str}")
                    break
                # Re-raise other exceptions
                raise

    except KeyboardInterrupt:
        print("\nKeyboardInterrupt: Stopping monitor")
        handle_keyboard_interrupt_properly()
        raise
    finally:
        # Close serial port
        if ser and ser.is_open:
            ser.close()
            print(f"\n✓ Closed serial port: {monitor_port}")

    # Determine success based on exit conditions
    if trigger_timeout_triggered:
        # Trigger timeout is always a failure
        success = False
    elif device_stuck:
        # Device stuck is always a failure
        success = False
    elif fail_keyword_found:
        # Fail pattern match always means failure
        success = False
    elif stop_pattern and not stop_keyword_found:
        # Stop pattern was specified but never found - failure
        success = False
    elif stop_keyword_found:
        # Stop pattern found - success if all expect patterns found (or no expect patterns)
        if expect_patterns:
            missing_patterns = (
                set(p for p, _ in expect_patterns) - found_expect_keywords
            )
            if missing_patterns:
                # Stop found but not all expects - this shouldn't happen (we check in loop)
                success = False
            else:
                # Stop found and all expects matched - success
                success = True
        else:
            # Stop found with no expect patterns - success
            success = True
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
        # No specific failure conditions - success
        success = True

    # Show output summary only on failure or in verbose mode
    # On success, the user already saw the output and patterns during monitoring
    show_summary = not success or verbose

    if show_summary:
        print("\n" + "─" * 60)
        print("OUTPUT SUMMARY")
        print("─" * 60)

        if len(output_lines) > 0:
            # On failure, show more context to help debug
            if not success:
                last_lines = output_lines[-30:]
                print(f"\n--- LAST {len(last_lines)} LINES ---")
                for line in last_lines:
                    print(line)

                # Display all failing lines if any were detected
                if failing_lines:
                    print(f"\n--- FAILING LINES ({len(failing_lines)} total) ---")
                    for keyword, line in failing_lines:
                        print(f"[{keyword}] {line}")

            print(f"\nTotal output lines: {len(output_lines)}")
        else:
            print("\nNo output captured")

        print("─" * 60)
    else:
        # Success - show test summary if available, otherwise minimal summary
        test_summary = format_test_summary(result_events)
        if test_summary:
            print("\n" + test_summary)
        else:
            print(f"\n(Captured {len(output_lines)} output lines)")

    print()

    if device_stuck:
        print("❌ Monitor failed - device appears stuck and no longer responding")
        print("   WARNING: Serial port communication failed (ClearCommError)")
        print(
            "   This typically indicates an ISR is hogging CPU time or device crashed"
        )
    elif fail_keyword_found:
        print(f"❌ Monitor failed - fail pattern /{matched_fail_keyword}/ detected")
        print(f"   Matched line: {matched_fail_line}")
    elif stop_pattern and not stop_keyword_found:
        print(
            f"❌ Monitor failed - stop pattern /{stop_pattern.pattern}/ was specified but not found"
        )
        print(
            "   The test did not complete successfully or the stop word was not emitted"
        )
    elif expect_patterns:
        # Check if all expect patterns were found
        missing_patterns = set(p for p, _ in expect_patterns) - found_expect_keywords
        if missing_patterns:
            print("❌ Monitor failed - not all expect patterns found")
            print(
                f"   Expected {len(expect_patterns)} patterns, found {len(found_expect_keywords)}"
            )
            # Display missing patterns clearly
            if len(missing_patterns) == 1:
                print(f"   Missing pattern: /{list(missing_patterns)[0]}/")
            else:
                print("   Missing patterns:")
                for pattern in sorted(missing_patterns):
                    print(f"     - /{pattern}/")
            # Display found patterns if any
            if found_expect_keywords:
                if len(found_expect_keywords) == 1:
                    print(f"   Found pattern: /{list(found_expect_keywords)[0]}/")
                else:
                    print("   Found patterns:")
                    for pattern in sorted(found_expect_keywords):
                        print(f"     - /{pattern}/")
        else:
            # Test summary (if available) was already shown above
            print(
                f"✅ Monitor succeeded - all {len(expect_patterns)} expect patterns found"
            )
    elif timeout_reached:
        print(f"✅ Monitor completed successfully (timeout reached after {timeout}s)")
    elif stream and success:
        print("✅ Monitor completed successfully (streaming mode ended)")
    elif success:
        print("✅ Monitor completed successfully")
    else:
        print("❌ Monitor failed")

    return success, output_lines, rpc_handler


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Upload and monitor attached PlatformIO device",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Auto-detect env & sketch (default: 60s timeout, waits until timeout)
  %(prog)s RX                       # Compile RX sketch (examples/RX), auto-detect environment
  %(prog)s RX --env esp32dev        # Compile RX sketch for specific environment
  %(prog)s examples/RX              # Same as above (explicit path)
  %(prog)s examples/deep/nested/Sketch  # Deep nested sketch
  %(prog)s --verbose                # Verbose mode
  %(prog)s --skip-lint              # Skip C++ linting (faster, but may miss ISR errors)
  %(prog)s --upload-port /dev/ttyUSB0  # Specific port
  %(prog)s --timeout 120            # Monitor for 120 seconds (default: 60s)
  %(prog)s --timeout 2m             # Monitor for 2 minutes
  %(prog)s --timeout 5000ms         # Monitor for 5 seconds
  %(prog)s --exit-on-error "ERROR"  # Exit 1 immediately if ERROR pattern found
  %(prog)s --exit-on-error "ClearCommError" --exit-on-error "register dump"  # Multiple exit patterns
  %(prog)s --fail-on "PANIC"        # Exit 1 immediately if "PANIC" pattern found
  %(prog)s --fail-on "ERROR" --fail-on "CRASH"  # Exit 1 on any pattern match
  %(prog)s --no-fail-on             # Explicitly disable all failure patterns
  %(prog)s --expect "SUCCESS"       # Exit 0 only if "SUCCESS" pattern found by timeout
  %(prog)s --expect "PASS" --expect "OK"  # Exit 0 only if ALL patterns found
  %(prog)s --stop "TEST COMPLETE"   # Early exit when pattern found (success if all expects found)
  %(prog)s --expect "READY" --stop "FINISHED"  # Exit early when FINISHED found (after READY matched)
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
        default="60",
        help="Timeout for monitor phase. Supports: plain number (seconds), '120s', '2m', '5000ms' (default: 60)",
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
        action="append",
        dest="exit_on_error_patterns",
        metavar="PATTERN",
        help=r"Exit 1 immediately if pattern found (can be specified multiple times). If specified without value, uses default pattern: \bERROR\b (word boundary). Accepts custom regex patterns.",
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
        "--stop",
        type=str,
        dest="stop_keyword",
        help="Regex pattern that triggers early successful exit if all expect patterns found (or no expect patterns). Allows tests to exit early when complete.",
    )
    parser.add_argument(
        "--stream",
        "-s",
        action="store_true",
        help="Stream mode: monitor runs indefinitely until Ctrl+C (ignores timeout)",
    )
    parser.add_argument(
        "--input-on-trigger",
        type=str,
        metavar="TRIGGER:TEXT",
        help='Wait for trigger pattern, then send text. Format: "PATTERN:TEXT" (e.g., "VALIDATION_READY:START")',
    )
    parser.add_argument(
        "--device-error-keyword",
        action="append",
        dest="device_error_keywords",
        help='Serial exception keywords indicating device stuck. Can be specified multiple times to check for multiple error conditions. If omitted, uses defaults: "ClearCommError", "PermissionError". If specified, replaces all defaults.',
    )
    parser.add_argument(
        "--kill-daemon",
        action="store_true",
        help="Stop and restart the package daemon before running (useful if daemon is stuck)",
    )
    parser.add_argument(
        "--check-usage",
        action="store_true",
        help="Enable sketch usage checks (e.g., block 'bash debug Validation' without wrapper)",
    )
    parser.add_argument(
        "--json-rpc",
        type=str,
        metavar="COMMANDS",
        help='JSON-RPC commands to send to device at startup. Accepts JSON string: \'{"function":"ping"}\' or \'[{"function":"setDrivers","args":["PARLIO"]}]\', or file path: @commands.json',
    )
    parser.add_argument(
        "--use-fbuild",
        action="store_true",
        help="Use fbuild for compile and upload instead of PlatformIO (default for esp32s3/esp32c6)",
    )
    parser.add_argument(
        "--no-fbuild",
        action="store_true",
        help="Force PlatformIO even for esp32s3/esp32c6 (disables fbuild default)",
    )

    return parser.parse_args()


def _should_use_fbuild(
    environment: str | None, use_fbuild_flag: bool, no_fbuild_flag: bool
) -> bool:
    """Determine if fbuild should be used for compilation and upload.

    fbuild is the default for esp32s3 and esp32c6 (RISC-V) environments.

    Args:
        environment: PlatformIO environment name (e.g., "esp32s3", "esp32c6")
        use_fbuild_flag: True if --use-fbuild was explicitly specified
        no_fbuild_flag: True if --no-fbuild was explicitly specified

    Returns:
        True if fbuild should be used, False otherwise
    """
    # Explicit --no-fbuild takes priority
    if no_fbuild_flag:
        return False

    # Explicit --use-fbuild
    if use_fbuild_flag:
        return True

    # Default: use fbuild for esp32s3 and esp32c6
    if environment:
        env_lower = environment.lower()
        if "esp32s3" in env_lower or "esp32c6" in env_lower:
            return True

    return False


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

            # Detect if user is trying to run Validation sketch directly (only with --check-usage)
            sketch_name = Path(resolved_sketch).name
            if args.check_usage and sketch_name.lower() == "validation":
                # Display red banner error
                print()
                print(Fore.RED + "=" * 60)
                print(Fore.RED + "⚠️  ERROR: DO NOT USE 'bash debug Validation'")
                print(Fore.RED + "=" * 60)
                print(
                    Fore.RED
                    + "The Validation sketch requires specific --expect and --fail-on"
                )
                print(
                    Fore.RED
                    + "patterns to work correctly. Use the dedicated wrapper instead:"
                )
                print()
                print(Fore.YELLOW + "  bash validate" + Style.RESET_ALL)
                print()
                print(
                    Fore.RED
                    + "This ensures proper pattern matching and prevents false positives."
                )
                print(Fore.RED + "=" * 60 + Style.RESET_ALL)
                print()
                return 1

            os.environ["PLATFORMIO_SRC_DIR"] = resolved_sketch
            print(f"📁 Sketch: {resolved_sketch}")
        except SystemExit:
            # resolve_sketch_path already printed error message
            return 1

    # Validate --fail-on and --no-fail-on conflict
    if args.no_fail_on and (args.fail_keywords or args.exit_on_error_patterns):
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

        # Add regex patterns from --exit-on-error (can be specified multiple times)
        if args.exit_on_error_patterns:
            fail_keywords.extend(args.exit_on_error_patterns)

        # Add custom patterns from --fail-on
        if args.fail_keywords:
            fail_keywords.extend(args.fail_keywords)

    # Parse timeout string with suffix support
    try:
        timeout_seconds = parse_timeout(args.timeout)
    except ValueError as e:
        print(f"❌ Error: {e}")
        return 1

    # Parse JSON-RPC commands if provided
    from ci.util.json_rpc_handler import parse_json_rpc_commands

    try:
        json_rpc_commands = parse_json_rpc_commands(args.json_rpc)
    except ValueError as e:
        print(f"❌ Error parsing JSON-RPC commands: {e}")
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

        # Determine if fbuild should be used (default for esp32s3/esp32c6)
        use_fbuild = _should_use_fbuild(
            args.environment, args.use_fbuild, args.no_fbuild
        )
        if use_fbuild:
            print("📦 Using fbuild (default for esp32s3/esp32c6)")
        else:
            print("📦 Using PlatformIO")

        if use_fbuild:
            from ci.util.fbuild_runner import run_fbuild_compile

            if not run_fbuild_compile(build_dir, args.environment, args.verbose):
                return 1
        else:
            if not run_compile(build_dir, args.environment, args.verbose):
                return 1

        # ============================================================
        # PHASES 3-4: Upload + Monitor
        # ============================================================
        # Note: Device locking is handled by fbuild (daemon-based)

        # Clean up orphaned processes holding the serial port
        if upload_port:
            kill_port_users(upload_port)

        # Phase 3: Upload firmware (with incremental rebuild if needed)
        if use_fbuild:
            from ci.util.fbuild_runner import run_fbuild_upload

            if not run_fbuild_upload(
                build_dir, args.environment, upload_port, args.verbose
            ):
                return 1
        else:
            if not run_upload(build_dir, args.environment, upload_port, args.verbose):
                return 1

        # Phase 4: Monitor serial output
        success, output, rpc_handler = run_monitor(
            build_dir,
            args.environment,
            upload_port,  # Use same port for monitoring
            args.verbose,
            timeout_seconds,
            fail_keywords,
            args.expect_keywords or [],
            args.stream,
            args.input_on_trigger,
            args.device_error_keywords,
            args.stop_keyword,
            json_rpc_commands,
        )

        if not success:
            return 1

        phases_completed = "three" if args.skip_lint else "four"
        print(f"\n✅ All {phases_completed} phases completed successfully")
        return 0

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\n\n⚠️  Interrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
