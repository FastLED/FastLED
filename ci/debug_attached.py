#!/usr/bin/env python3
"""Three-phase device workflow: Compile → Upload → Monitor.

This script orchestrates a complete device development workflow in three distinct phases:

Phase 1: Compile
    - Builds the PlatformIO project for the target environment
    - Validates code compiles without errors

Phase 2: Upload (with self-healing)
    - Kills lingering monitor processes that may lock the serial port
    - Uploads firmware to the attached device
    - Handles port conflicts automatically

Phase 3: Monitor
    - Attaches to serial monitor and displays real-time output
    - Captures output for analysis
    - Exits with code 1 if any fail keyword is detected (--fail-on)
    - Provides output summary (first/last 100 lines)

Usage:
    uv run ci/debug_attached.py                          # Auto-detect environment
    uv run ci/debug_attached.py esp32dev                 # Specific environment
    uv run ci/debug_attached.py --verbose                # Verbose mode
    uv run ci/debug_attached.py --upload-port /dev/ttyUSB0  # Specific port
    uv run ci/debug_attached.py --timeout 120            # Monitor for 120 seconds
    uv run ci/debug_attached.py --timeout 2m             # Monitor for 2 minutes
    uv run ci/debug_attached.py --timeout 5000ms         # Monitor for 5 seconds
    uv run ci/debug_attached.py --fail-on PANIC          # Exit 1 if "PANIC" found
    uv run ci/debug_attached.py --fail-on ERROR --fail-on CRASH  # Multiple keywords
    uv run ci/debug_attached.py --stream                 # Stream mode (runs until Ctrl+C)
    uv run ci/debug_attached.py esp32dev --verbose --upload-port COM3
"""

import argparse
import re
import sys
import time
from pathlib import Path

import psutil
from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream

from ci.compiler.build_utils import get_utf8_env
from ci.util.global_interrupt_handler import notify_main_thread
from ci.util.output_formatter import TimestampFormatter


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


def kill_lingering_monitors() -> int:
    """Kill any lingering 'pio device monitor' or 'pio run -t monitor' processes.

    This is the self-healing step to ensure the serial port is not locked
    by a previous monitor session.

    Returns:
        Number of processes killed.
    """
    killed_count = 0
    for proc in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            cmdline = proc.info.get("cmdline", [])
            if not cmdline:
                continue

            # Check if this is a pio monitor process
            cmdline_str = " ".join(cmdline)
            if "pio" in cmdline_str and "monitor" in cmdline_str:
                print(
                    f"Killing lingering monitor process (PID {proc.pid}): {' '.join(cmdline[:3])}..."
                )
                proc.kill()
                killed_count += 1
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            # Process already gone or we can't access it
            pass

    if killed_count > 0:
        time.sleep(1)  # Give OS time to release ports
        print(f"Killed {killed_count} lingering monitor process(es)\n")

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

    This function includes self-healing logic to kill lingering monitor
    processes before attempting upload.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: PlatformIO environment to upload (None = default)
        upload_port: Serial port to use (None = auto-detect)
        verbose: Enable verbose output

    Returns:
        True if upload succeeded, False otherwise.
    """
    # Self-healing: Kill lingering monitors before upload
    kill_lingering_monitors()

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
    stream: bool = False,
) -> tuple[bool, list[str]]:
    """Attach to serial monitor and capture output.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: PlatformIO environment to monitor (None = default)
        monitor_port: Serial port to monitor (None = auto-detect)
        verbose: Enable verbose output
        timeout: Maximum time to monitor in seconds (default: 80)
        fail_keywords: List of keywords that trigger exit code 1 if found
        stream: If True, monitor runs indefinitely until Ctrl+C (ignores timeout)

    Returns:
        Tuple of (success, output_lines)
    """
    if fail_keywords is None:
        fail_keywords = []
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
    start_time = time.time()
    keyword_found = False
    matched_keyword = None
    matched_line = None
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

                    # Check for fail keywords
                    if fail_keywords and not keyword_found:
                        for keyword in fail_keywords:
                            if keyword in line:
                                keyword_found = True
                                matched_keyword = keyword
                                matched_line = line
                                print(f"\n🚨 FAIL KEYWORD DETECTED: '{keyword}'")
                                print(f"   Matched line: {line}")
                                print("   Terminating monitor...\n")
                                proc.terminate()
                                break

                    if keyword_found:
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
    if keyword_found:
        # Keyword match always means failure
        success = False
    elif timeout_reached:
        # Normal timeout is considered success (exit 0)
        success = True
    else:
        # Process exited on its own - use actual return code
        success = proc.returncode == 0

    # Display first 100 and last 100 lines summary
    print("\n" + "=" * 60)
    print("OUTPUT SUMMARY")
    print("=" * 60)

    if len(output_lines) > 0:
        first_100 = output_lines[:100]
        last_100 = output_lines[-100:]

        print(f"\n--- FIRST {len(first_100)} LINES ---")
        for line in first_100:
            print(line)

        if len(output_lines) > 100:
            print(f"\n--- LAST {len(last_100)} LINES ---")
            for line in last_100:
                print(line)

        print(f"\nTotal output lines: {len(output_lines)}")
    else:
        print("\nNo output captured")

    print("\n" + "=" * 60)

    if keyword_found:
        print(f"❌ Monitor failed - keyword '{matched_keyword}' detected")
        print(f"   Matched line: {matched_line}")
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
  %(prog)s                          # Auto-detect environment
  %(prog)s esp32dev                 # Specific environment
  %(prog)s --verbose                # Verbose mode
  %(prog)s --upload-port /dev/ttyUSB0  # Specific port
  %(prog)s --timeout 120            # Monitor for 120 seconds
  %(prog)s --timeout 2m             # Monitor for 2 minutes
  %(prog)s --timeout 5000ms         # Monitor for 5 seconds
  %(prog)s --fail-on PANIC          # Exit 1 if "PANIC" found in output
  %(prog)s --fail-on ERROR --fail-on CRASH  # Multiple failure keywords
  %(prog)s --stream                 # Stream mode (runs until Ctrl+C)
  %(prog)s esp32dev --verbose --upload-port COM3
        """,
    )

    parser.add_argument(
        "environment",
        nargs="?",
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
        default="80",
        help="Timeout for monitor phase. Supports: plain number (seconds), '120s', '2m', '5000ms' (default: 80)",
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
        help="Keyword that triggers exit code 1 if found in monitor output (can be specified multiple times)",
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
        # Phase 1: Compile
        if not run_compile(build_dir, args.environment, args.verbose):
            return 1

        # Phase 2: Upload (includes self-healing port cleanup)
        if not run_upload(build_dir, args.environment, args.upload_port, args.verbose):
            return 1

        # Phase 3: Monitor serial output
        success, output = run_monitor(
            build_dir,
            args.environment,
            args.upload_port,  # Use same port for monitoring
            args.verbose,
            timeout_seconds,
            args.fail_keywords,
            args.stream,
        )

        if not success:
            return 1

        print("\n✅ All three phases completed successfully")
        return 0

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
