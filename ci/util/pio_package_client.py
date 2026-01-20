from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
PlatformIO Package Installation Client

Client interface for requesting package installations from the daemon.
Handles daemon lifecycle, request submission, and progress monitoring.
"""

import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import psutil

from ci.util.banner_print import (
    format_elapsed_time,
    print_error_banner,
    print_phase_banner,
    print_progress_section,
    print_success_banner,
    print_tree_status,
)
from ci.util.pio_package_messages import DaemonState, DaemonStatus, PackageRequest


# Import package validator
try:
    from ci.util.package_validator import check_all_packages
except ImportError:
    # Fallback if import fails (shouldn't happen in normal operation)
    def check_all_packages(
        project_dir: Path, environment: str
    ) -> tuple[bool, list[str]]:
        return True, []


# Daemon configuration (must match daemon settings)
DAEMON_NAME = "fastled_pio_package_daemon"
DAEMON_DIR = Path.home() / ".fastled" / "daemon"
PID_FILE = DAEMON_DIR / f"{DAEMON_NAME}.pid"
STATUS_FILE = DAEMON_DIR / "package_status.json"
REQUEST_FILE = DAEMON_DIR / "package_request.json"


def is_daemon_running() -> bool:
    """Check if daemon is running, clean up stale PID files.

    Returns:
        True if daemon is running, False otherwise
    """
    if not PID_FILE.exists():
        return False

    try:
        with open(PID_FILE, "r") as f:
            pid = int(f.read().strip())

        # Check if process exists
        if psutil.pid_exists(pid):
            return True
        else:
            # Stale PID file - remove it
            print(f"Removing stale PID file: {PID_FILE}")
            PID_FILE.unlink()
            return False
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        # Corrupted PID file - remove it
        try:
            PID_FILE.unlink(missing_ok=True)
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            pass
        return False


def start_daemon() -> None:
    """Start the package daemon process."""
    daemon_script = Path(__file__).parent / "pio_package_daemon.py"

    if not daemon_script.exists():
        raise RuntimeError(f"Daemon script not found: {daemon_script}")

    # Start daemon in background (daemoniker handles detachment)
    # Use same Python executable as current process
    subprocess.Popen(
        [sys.executable, str(daemon_script)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        stdin=subprocess.DEVNULL,
    )


def read_status_file() -> DaemonStatus:
    """Read current daemon status with corruption recovery.

    Returns:
        DaemonStatus object (or default status if file doesn't exist or corrupted)
    """
    if not STATUS_FILE.exists():
        return DaemonStatus(
            state=DaemonState.UNKNOWN,
            message="Status file not found",
            updated_at=time.time(),
        )

    try:
        with open(STATUS_FILE, "r") as f:
            data = json.load(f)

        # Parse into typed DaemonStatus
        return DaemonStatus.from_dict(data)

    except (json.JSONDecodeError, ValueError):
        # Corrupted JSON - return default status
        return DaemonStatus(
            state=DaemonState.UNKNOWN,
            message="Status file corrupted (invalid JSON)",
            updated_at=time.time(),
        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        return DaemonStatus(
            state=DaemonState.UNKNOWN,
            message="Failed to read status",
            updated_at=time.time(),
        )


def write_request_file(request: PackageRequest) -> None:
    """Atomically write request file.

    Args:
        request: PackageRequest object
    """
    DAEMON_DIR.mkdir(parents=True, exist_ok=True)

    # Atomic write using temporary file
    temp_file = REQUEST_FILE.with_suffix(".tmp")
    with open(temp_file, "w") as f:
        json.dump(request.to_dict(), f, indent=2)

    # Atomic rename
    temp_file.replace(REQUEST_FILE)


def display_status(status: DaemonStatus, prefix: str = "  ") -> None:
    """Display status update to user with enhanced formatting.

    Args:
        status: DaemonStatus object
        prefix: Line prefix for indentation
    """
    # Show current operation if available, otherwise use message
    display_text = status.current_operation or status.message

    if status.state == DaemonState.INSTALLING:
        print(f"{prefix}📦 {display_text}", flush=True)
    elif status.state == DaemonState.COMPLETED:
        print(f"{prefix}✅ {display_text}", flush=True)
    elif status.state == DaemonState.FAILED:
        print(f"{prefix}❌ {display_text}", flush=True)
    else:
        print(f"{prefix}ℹ️  {display_text}", flush=True)


def packages_already_installed(project_dir: Path, environment: str | None) -> bool:
    """Quick check if packages are already installed.

    This is a fast-path optimization to skip daemon interaction when
    packages are already present.

    Args:
        project_dir: PlatformIO project directory
        environment: PlatformIO environment name (or None for all)

    Returns:
        True if packages appear to be installed, False otherwise
    """
    # Run quick PlatformIO check
    # We use 'pio pkg list' which is fast when packages are installed
    try:
        cmd = ["pio", "pkg", "list", "--project-dir", str(project_dir)]
        if environment:
            cmd.extend(["--environment", environment])

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=10,
        )

        # If command succeeds and has output, packages are likely installed
        # PlatformIO will trigger install if needed, but we want to avoid that
        # So we just check if the command runs successfully
        return result.returncode == 0

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        # If check fails, assume packages need installation
        return False


def ensure_packages_installed(
    project_dir: Path,
    environment: str | None,
    timeout: int = 1800,
) -> bool:
    """Ensure PlatformIO packages are installed via daemon.

    Enhanced with banner-style logging and typed message protocol.

    Args:
        project_dir: PlatformIO project directory
        environment: PlatformIO environment name (or None for all)
        timeout: Maximum wait time in seconds (default: 30 minutes)

    Returns:
        True if packages installed successfully, False otherwise
    """
    # Detect default environment if None (for package validation)
    default_env = environment
    if default_env is None:
        from ci.util.pio_package_daemon import get_default_environment

        default_env = get_default_environment(str(project_dir))
        if default_env:
            environment = default_env  # Use detected default for display

    # ═══════════════════════════════════════════════════════════
    # PHASE BANNER
    # ═══════════════════════════════════════════════════════════
    print_phase_banner(
        "PHASE 0: PACKAGE INSTALLATION",
        details={
            "Project": str(project_dir),
            "Environment": environment or "default",
            "Caller PID": str(os.getpid()),
        },
    )

    # ───────────────────────────────────────────────────────────
    # PRE-FLIGHT CHECK
    # ───────────────────────────────────────────────────────────
    # Quick check: are packages already installed?
    if packages_already_installed(project_dir, environment):
        # Additional validation check for corrupted packages
        if default_env:
            is_valid, errors = check_all_packages(project_dir, default_env)
            if not is_valid:
                print_tree_status(
                    "🔍 Checking packages...",
                    [
                        ("", "Validating integrity...", None),
                        ("", f"❌ Corrupted: {errors[0]}", "error"),
                        ("", "📦 Reinstallation required", None),
                    ],
                )
                # Fall through to daemon installation
            else:
                print_tree_status(
                    "🔍 Checking packages...",
                    [
                        ("", "Validating integrity...", None),
                        ("", "✅ Packages valid", "success"),
                    ],
                )
                return True
        else:
            print_tree_status(
                "🔍 Checking packages...",
                [
                    ("", "✅ Packages already installed", "success"),
                ],
            )
            return True
    else:
        print_tree_status(
            "🔍 Checking packages...",
            [
                ("", "Validating integrity...", None),
                ("", "📦 Installation required", None),
            ],
        )

    # ───────────────────────────────────────────────────────────
    # DAEMON CONNECTION
    # ───────────────────────────────────────────────────────────
    daemon_pid = None
    daemon_status_age = None

    # Start daemon if not running
    if not is_daemon_running():
        print_tree_status(
            "🔗 Connecting to daemon...",
            [
                ("", "Status: NOT RUNNING", None),
                ("", "Starting daemon...", None),
            ],
        )

        start_daemon()
        time.sleep(2)  # Give daemon time to initialize

        # Wait up to 10 seconds for daemon to start
        for _ in range(10):
            if is_daemon_running():
                break
            time.sleep(1)
        else:
            print_error_banner(
                "Daemon Connection FAILED",
                "Failed to start package installation daemon",
                recommendations=[
                    "Check daemon logs: bash daemon logs",
                    "Manually start daemon: uv run python ci/util/pio_package_daemon.py --foreground",
                    "Check for port conflicts or permission issues",
                ],
            )
            return False

        print("   └─ ✅ Daemon started")
    else:
        # Daemon is running - get health info
        status = read_status_file()
        daemon_pid = status.daemon_pid
        daemon_status_age = status.get_age_seconds()

        health_status = "OK" if not status.is_stale() else "STALE"
        health_color = None if not status.is_stale() else "warning"

        print_tree_status(
            "🔗 Connecting to daemon...",
            [
                ("", f"Status: RUNNING (PID {daemon_pid or 'unknown'})", None),
                (
                    "",
                    f"Health: {health_status} (updated {daemon_status_age:.1f}s ago)",
                    health_color,
                ),
                ("", "✅ Connected", "success"),
            ],
        )

    # ───────────────────────────────────────────────────────────
    # SUBMIT REQUEST
    # ───────────────────────────────────────────────────────────
    request = PackageRequest(
        project_dir=str(project_dir),
        environment=environment,  # None is valid - daemon will use PlatformIO default
        timestamp=time.time(),
        caller_pid=os.getpid(),
        caller_cwd=os.getcwd(),
    )

    # CHECK: Wait if installation already in progress
    status = read_status_file()
    if status.installation_in_progress:
        print_tree_status(
            "📤 Submitting request...",
            [
                ("", "⏳ Queue position: waiting", None),
                (
                    "",
                    f"Active: PID {status.caller_pid} ({status.caller_cwd})",
                    None,
                ),
                ("", "Waiting for slot...", None),
            ],
        )

        # Wait for ongoing installation to complete
        timeout_start = time.time()
        last_progress_update = time.time()
        while status.installation_in_progress:
            current_time = time.time()
            elapsed = current_time - timeout_start

            # Check timeout (30 minute timeout)
            if elapsed > 1800:
                print_error_banner(
                    "Request Submission FAILED",
                    f"Timeout waiting for active installation (PID {status.caller_pid})",
                    recommendations=[
                        "Check if active installation is stuck: bash daemon logs-tail",
                        "Force restart daemon: bash daemon restart (may corrupt packages)",
                    ],
                )
                return False

            # Print progress update every 10 seconds
            if current_time - last_progress_update >= 10:
                elapsed_str = format_elapsed_time(elapsed)
                print(
                    f"   └─ Still queued... (elapsed: {elapsed_str}, active PID: {status.caller_pid})",
                    flush=True,
                )
                last_progress_update = current_time

            time.sleep(2)
            status = read_status_file()

        print("   └─ ✅ Queue clear, proceeding...")

    print_tree_status(
        "📤 Submitting request...",
        [
            ("", f"Request ID: {request.request_id}", None),
            ("", "✅ Submitted", "success"),
        ],
    )
    write_request_file(request)

    # ───────────────────────────────────────────────────────────
    # MONITOR INSTALLATION PROGRESS
    # ───────────────────────────────────────────────────────────
    print_progress_section("Package Installation Progress:")

    start_time = time.time()
    last_message = None
    last_heartbeat = time.time()

    while True:
        try:
            elapsed = time.time() - start_time

            # Check timeout
            if elapsed > timeout:
                timeout_str = format_elapsed_time(timeout)
                print_error_banner(
                    "Package Installation TIMEOUT",
                    f"Installation did not complete within {timeout_str}",
                    recommendations=[
                        "Daemon continues in background - check logs: bash daemon logs-tail",
                        "Check network connectivity if downloading packages",
                        "Increase timeout if needed (current: {}s)".format(timeout),
                    ],
                )
                return False

            # Read status
            status = read_status_file()

            # Display progress when message changes
            if status.message != last_message:
                display_status(status)
                last_message = status.message
                last_heartbeat = time.time()  # Reset heartbeat on message change

            # Heartbeat: print periodic updates during long operations (every 30s)
            if time.time() - last_heartbeat >= 30:
                elapsed_str = format_elapsed_time(elapsed)
                status_age = status.get_age_seconds()
                print(
                    f"  ⏱️  Still {status.state.value}... (elapsed: {elapsed_str}, last update: {status_age:.1f}s ago)",
                    flush=True,
                )
                last_heartbeat = time.time()

                # Warn if daemon appears stale
                if status.is_stale(timeout_seconds=60):
                    print(
                        f"  ⚠️  Daemon status is stale (no update for {status_age:.0f}s) - check logs: bash daemon logs-tail",
                        flush=True,
                    )

            # Check completion
            if status.state == DaemonState.COMPLETED:
                total_time = format_elapsed_time(elapsed)
                print_success_banner("Package installation completed", total_time)
                return True
            elif status.state == DaemonState.FAILED:
                print_error_banner(
                    "Package Installation FAILED",
                    status.message,
                    recommendations=[
                        "Check daemon logs for details: bash daemon logs",
                        "Verify network connectivity and disk space",
                        "Try again: installation may have been interrupted",
                    ],
                )
                return False

            # Sleep before next poll
            time.sleep(1)

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            print("\n\n⚠️  Interrupted by user")
            print("Note: Daemon will continue installation in background")
            print("      Check progress: bash daemon logs-tail")
            raise


def stop_daemon() -> bool:
    """Stop the daemon gracefully.

    Returns:
        True if daemon was stopped, False otherwise
    """
    if not is_daemon_running():
        print("Daemon is not running")
        return False

    # Create shutdown signal file
    shutdown_file = DAEMON_DIR / "shutdown.signal"
    shutdown_file.touch()

    # Wait for daemon to exit
    print("Stopping daemon...")
    for _ in range(10):
        if not is_daemon_running():
            print("✅ Daemon stopped")
            return True
        time.sleep(1)

    print("⚠️  Daemon did not stop gracefully")
    return False


def get_daemon_status() -> dict[str, Any]:
    """Get current daemon status.

    Returns:
        Dictionary with daemon status information
    """
    status: dict[str, Any] = {
        "running": is_daemon_running(),
        "pid_file_exists": PID_FILE.exists(),
        "status_file_exists": STATUS_FILE.exists(),
    }

    if PID_FILE.exists():
        try:
            with open(PID_FILE, "r") as f:
                status["pid"] = int(f.read().strip())
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            status["pid"] = None

    if STATUS_FILE.exists():
        daemon_status = read_status_file()
        # Convert DaemonStatus to dict for JSON serialization
        status["current_status"] = daemon_status.to_dict()

    return status


def main() -> int:
    """Command-line interface for client."""
    import argparse

    parser = argparse.ArgumentParser(
        description="PlatformIO Package Installation Client"
    )
    parser.add_argument("--status", action="store_true", help="Show daemon status")
    parser.add_argument("--stop", action="store_true", help="Stop the daemon")
    parser.add_argument(
        "--test-request",
        action="store_true",
        help="Submit test request to daemon",
    )
    parser.add_argument(
        "--project-dir",
        type=Path,
        help="PlatformIO project directory",
    )
    parser.add_argument(
        "--environment",
        help="PlatformIO environment",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=1800,
        help="Timeout in seconds (default: 1800)",
    )

    args = parser.parse_args()

    if args.status:
        status = get_daemon_status()
        print("Daemon Status:")
        print(json.dumps(status, indent=2))
        return 0

    if args.stop:
        return 0 if stop_daemon() else 1

    if args.test_request:
        # Use current directory as test
        project_dir = Path.cwd()
        if not (project_dir / "platformio.ini").exists():
            print("❌ No platformio.ini found in current directory")
            return 1

        success = ensure_packages_installed(
            project_dir,
            args.environment,
            args.timeout,
        )
        return 0 if success else 1

    if args.project_dir:
        success = ensure_packages_installed(
            args.project_dir,
            args.environment,
            args.timeout,
        )
        return 0 if success else 1

    parser.print_help()
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\nInterrupted by user")
        sys.exit(130)
