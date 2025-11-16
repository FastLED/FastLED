#!/usr/bin/env python3
"""Upload and monitor attached device with self-healing port management.

This script compiles a PlatformIO project, uploads it to an attached device,
and monitors the serial output. It includes self-healing logic to kill any
lingering monitor processes that might prevent upload.

Usage:
    uv run ci/debug_attached.py                          # Auto-detect environment
    uv run ci/debug_attached.py esp32dev                 # Specific environment
    uv run ci/debug_attached.py --verbose                # Verbose mode
    uv run ci/debug_attached.py --upload-port /dev/ttyUSB0  # Specific port
    uv run ci/debug_attached.py esp32dev --verbose --upload-port COM3
"""

import argparse
import sys
import time
from pathlib import Path

import psutil
from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream

from ci.compiler.build_utils import get_utf8_env
from ci.util.global_interrupt_handler import notify_main_thread
from ci.util.output_formatter import TimestampFormatter


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


def run_upload_monitor(
    build_dir: Path,
    environment: str | None = None,
    upload_port: str | None = None,
    verbose: bool = False,
    timeout: int = 45,
) -> tuple[bool, list[str]]:
    """Upload firmware and monitor serial output.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: PlatformIO environment to upload (None = default)
        upload_port: Serial port to use (None = auto-detect)
        verbose: Enable verbose output
        timeout: Maximum time to run (seconds)

    Returns:
        Tuple of (success, output_lines)
    """
    cmd = [
        "pio",
        "run",
        "--project-dir",
        str(build_dir),
        "-t",
        "upload",
        "-t",
        "monitor",
    ]
    if environment:
        cmd.extend(["--environment", environment])
    if upload_port:
        cmd.extend(["--upload-port", upload_port])
    if verbose:
        cmd.append("--verbose")

    print("=" * 60)
    print("UPLOADING AND MONITORING")
    print(f"Timeout: {timeout} seconds")
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

    try:
        while True:
            # Check timeout
            elapsed = time.time() - start_time
            if elapsed >= timeout:
                print(f"\n⏱️  Timeout reached ({timeout}s), stopping monitor...")
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
            except TimeoutError:
                # No output within 30 seconds - continue waiting (check overall timeout on next loop)
                continue

    except KeyboardInterrupt:
        print("\nKeyboardInterrupt: Stopping upload/monitor")
        proc.terminate()
        notify_main_thread()
        raise

    proc.wait()
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

    if success:
        print("✅ Upload and monitor completed successfully")
    else:
        print(f"❌ Upload/monitor failed (exit code {proc.returncode})")

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
        type=int,
        default=45,
        help="Timeout for upload and monitor phase in seconds (default: 45)",
    )
    parser.add_argument(
        "--project-dir",
        "-d",
        type=Path,
        default=Path.cwd(),
        help="PlatformIO project directory (default: current directory)",
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
        # Step 1: Compile
        if not run_compile(build_dir, args.environment, args.verbose):
            return 1

        # Step 2: Kill lingering monitors (self-healing)
        kill_lingering_monitors()

        # Step 3: Upload and monitor
        success, output = run_upload_monitor(
            build_dir,
            args.environment,
            args.upload_port,
            args.verbose,
            args.timeout,
        )

        if not success:
            return 1

        print("\n✅ All steps completed successfully")
        return 0

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
