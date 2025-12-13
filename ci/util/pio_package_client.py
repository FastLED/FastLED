#!/usr/bin/env python3
"""
PlatformIO Package Installation Client

Client interface for requesting package installations from the daemon.
Handles daemon lifecycle, request submission, and progress monitoring.
"""

import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import psutil


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
        raise
    except Exception:
        # Corrupted PID file - remove it
        try:
            PID_FILE.unlink(missing_ok=True)
        except KeyboardInterrupt:
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


def read_status_file() -> dict[str, Any]:
    """Read current daemon status with corruption recovery.

    Returns:
        Status dictionary or default status if file doesn't exist or corrupted
    """
    if not STATUS_FILE.exists():
        return {"state": "unknown", "message": "Status file not found"}

    try:
        with open(STATUS_FILE, "r") as f:
            data = json.load(f)

        # Validate required fields
        required = ["state", "message", "updated_at"]
        for field in required:
            if field not in data:
                return {
                    "state": "error",
                    "message": "Status file corrupted (missing fields)",
                }

        return data

    except (json.JSONDecodeError, ValueError):
        # Corrupted JSON - return default status
        return {
            "state": "error",
            "message": "Status file corrupted (invalid JSON)",
        }
    except KeyboardInterrupt:
        raise
    except Exception:
        return {"state": "error", "message": "Failed to read status"}


def write_request_file(request: dict[str, Any]) -> None:
    """Atomically write request file.

    Args:
        request: Request dictionary
    """
    DAEMON_DIR.mkdir(parents=True, exist_ok=True)

    # Atomic write using temporary file
    temp_file = REQUEST_FILE.with_suffix(".tmp")
    with open(temp_file, "w") as f:
        json.dump(request, f, indent=2)

    # Atomic rename
    temp_file.replace(REQUEST_FILE)


def display_status(status: dict[str, Any], prefix: str = "  ") -> None:
    """Display status update to user.

    Args:
        status: Status dictionary
        prefix: Line prefix for indentation
    """
    state = status.get("state", "unknown")
    message = status.get("message", "")

    if state == "installing":
        print(f"{prefix}📦 {message}", flush=True)
    elif state == "completed":
        print(f"{prefix}✅ {message}", flush=True)
    elif state == "failed":
        print(f"{prefix}❌ {message}", flush=True)
    else:
        print(f"{prefix}ℹ️  {message}", flush=True)


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
            print(f"Detected default environment: {default_env}")

    print(f"Checking if packages are installed for {environment}...")

    # Quick check: are packages already installed?
    if packages_already_installed(project_dir, environment):
        # Additional validation check for corrupted packages
        # Use default_env (detected default) even when environment is None
        if default_env:
            is_valid, errors = check_all_packages(project_dir, default_env)
            if not is_valid:
                print("⚠️  Detected corrupted packages, requesting reinstallation:")
                for error in errors:
                    print(f"  - {error}")
                # Fall through to daemon installation
            else:
                print("✅ Packages already installed and validated")
                return True
        else:
            print("✅ Packages already installed")
            return True

    print("📦 Packages need installation - requesting daemon...")

    # Start daemon if not running
    if not is_daemon_running():
        print("Starting package installation daemon...")
        start_daemon()
        time.sleep(2)  # Give daemon time to initialize

        # Wait up to 10 seconds for daemon to start
        for _ in range(10):
            if is_daemon_running():
                break
            time.sleep(1)
        else:
            print("❌ Failed to start daemon")
            return False

        print("✅ Daemon started")

    # Submit request
    import os

    request = {
        "project_dir": str(project_dir),
        "environment": environment,  # None is valid - daemon will use PlatformIO default
        "timestamp": time.time(),
        "caller_pid": os.getpid(),
        "caller_cwd": os.getcwd(),
    }

    # CHECK: Wait if installation already in progress
    status = read_status_file()
    if status.get("installation_in_progress", False):
        active_pid = status.get("caller_pid", "unknown")
        active_cwd = status.get("caller_cwd", "unknown")
        print(f"⏳ Another installation in progress, waiting...")
        print(f"   Active installation: PID={active_pid}, CWD={active_cwd}")
        print(f"   If stuck, run: bash daemon restart")
        # Wait for ongoing installation to complete
        timeout_start = time.time()
        while status.get("installation_in_progress", False):
            if time.time() - timeout_start > 1800:  # 30 minute timeout
                print("❌ Timeout waiting for ongoing installation")
                print("   To force restart: bash daemon restart")
                return False
            time.sleep(2)
            status = read_status_file()
        print("✅ Previous installation completed")

    print(f"Submitting package installation request...")
    write_request_file(request)

    # Wait for completion with progress updates
    print("\nPackage Installation Progress:")
    print("-" * 60)

    start_time = time.time()
    last_status = None
    last_message = None

    while True:
        try:
            elapsed = time.time() - start_time

            # Check timeout
            if elapsed > timeout:
                print(f"\n❌ Package installation timeout after {timeout}s")
                print("Note: Daemon will continue installation in background")
                return False

            # Read status
            status = read_status_file()
            current_message = status.get("message", "")

            # Display progress (only if changed)
            if current_message != last_message:
                display_status(status)
                last_message = current_message

            # Check completion
            state = status.get("state", "unknown")
            if state == "completed":
                print("-" * 60)
                print("✅ Package installation completed successfully\n")
                return True
            elif state == "failed":
                print("-" * 60)
                print(f"❌ Package installation failed: {current_message}\n")
                return False

            # Sleep before next poll
            time.sleep(1)

        except KeyboardInterrupt:
            print("\n\n⚠️  Interrupted by user")
            print("Note: Daemon will continue installation in background")
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
    status = {
        "running": is_daemon_running(),
        "pid_file_exists": PID_FILE.exists(),
        "status_file_exists": STATUS_FILE.exists(),
    }

    if PID_FILE.exists():
        try:
            with open(PID_FILE, "r") as f:
                status["pid"] = int(f.read().strip())
        except Exception:
            status["pid"] = None

    if STATUS_FILE.exists():
        status["current_status"] = read_status_file()

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
        print("\nInterrupted by user")
        sys.exit(130)
