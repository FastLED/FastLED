#!/usr/bin/env python3
"""
PlatformIO Package Installation Daemon

This daemon manages PlatformIO package installations to prevent corruption
when agent processes are interrupted during package downloads. The daemon:

1. Runs as a singleton process (enforced via PID file)
2. Survives agent termination
3. Processes package installation requests sequentially
4. Provides status updates via status file
5. Auto-shuts down after idle timeout

Architecture:
    Agents -> Request File -> Daemon -> PlatformIO CLI -> Packages
                   |              |
                   v              v
              Status File    Progress Updates
"""

import json
import logging
import os
import re
import shutil
import subprocess
import sys
import time
from logging.handlers import RotatingFileHandler
from pathlib import Path
from typing import Any

import psutil
from daemoniker import Daemonizer


# Daemon configuration
DAEMON_NAME = "fastled_pio_package_daemon"
DAEMON_DIR = Path.home() / ".fastled" / "daemon"
PID_FILE = DAEMON_DIR / f"{DAEMON_NAME}.pid"
STATUS_FILE = DAEMON_DIR / "package_status.json"
REQUEST_FILE = DAEMON_DIR / "package_request.json"
LOG_FILE = DAEMON_DIR / "daemon.log"
IDLE_TIMEOUT = 300  # 5 minutes

# Network error patterns for retry detection
NETWORK_ERROR_PATTERNS = [
    r"Failed to connect",
    r"Connection timeout",
    r"Network is unreachable",
    r"Could not resolve host",
    r"Download error",
    r"SSLError",
    r"Connection reset by peer",
    r"Temporary failure in name resolution",
    r"Connection refused",
    r"No route to host",
]

# Maximum retry attempts for network errors
MAX_NETWORK_RETRIES = 3


def setup_logging(foreground: bool = False) -> None:
    """Setup logging for daemon."""
    DAEMON_DIR.mkdir(parents=True, exist_ok=True)

    # Configure root logger
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)

    # Console handler (for foreground mode)
    if foreground:
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(logging.INFO)
        console_formatter = logging.Formatter(
            "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
        )
        console_handler.setFormatter(console_formatter)
        logger.addHandler(console_handler)

    # Rotating file handler (always)
    file_handler = RotatingFileHandler(
        str(LOG_FILE),
        maxBytes=10 * 1024 * 1024,  # 10MB
        backupCount=3,
    )
    file_handler.setLevel(logging.INFO)
    file_formatter = logging.Formatter(
        "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    )
    file_handler.setFormatter(file_formatter)
    logger.addHandler(file_handler)


def read_status_file_safe() -> dict[str, Any]:
    """Read status file with corruption recovery.

    Returns:
        Status dict (or default if corrupted)
    """
    default_status = {
        "state": "idle",
        "message": "",
        "started_at": None,
        "updated_at": time.time(),
    }

    if not STATUS_FILE.exists():
        return default_status

    try:
        with open(STATUS_FILE, "r") as f:
            data = json.load(f)

        # Validate required fields
        required = ["state", "message", "updated_at"]
        for field in required:
            if field not in data:
                raise ValueError(f"Missing field: {field}")

        return data

    except (json.JSONDecodeError, ValueError) as e:
        logging.warning(f"Corrupted status file detected: {e}")
        logging.warning("Creating fresh status file")

        # Write fresh status file
        write_status_file_atomic(default_status)

        return default_status
    except KeyboardInterrupt:
        raise
    except Exception as e:
        logging.error(f"Unexpected error reading status file: {e}")
        write_status_file_atomic(default_status)
        return default_status


def write_status_file_atomic(status: dict[str, Any]) -> None:
    """Write status file atomically to prevent corruption during writes.

    Args:
        status: Status dictionary to write
    """
    temp_file = STATUS_FILE.with_suffix(".tmp")

    try:
        with open(temp_file, "w") as f:
            json.dump(status, f, indent=2)

        # Atomic rename
        temp_file.replace(STATUS_FILE)

    except KeyboardInterrupt:
        temp_file.unlink(missing_ok=True)
        raise
    except Exception as e:
        logging.error(f"Failed to write status file: {e}")
        temp_file.unlink(missing_ok=True)


def update_status(state: str, message: str, **kwargs: Any) -> None:
    """Update status file with current daemon state.

    Args:
        state: One of: idle, installing, completed, failed
        message: Human-readable status message
        **kwargs: Additional fields to include in status
    """
    status = {
        "state": state,
        "message": message,
        "updated_at": time.time(),
        **kwargs,
    }

    write_status_file_atomic(status)


def read_request_file() -> dict[str, Any] | None:
    """Read and parse request file.

    Returns:
        Request dict if valid request exists, None otherwise
    """
    if not REQUEST_FILE.exists():
        return None

    try:
        with open(REQUEST_FILE, "r") as f:
            request = json.load(f)
        return request
    except KeyboardInterrupt:
        raise
    except Exception as e:
        logging.error(f"Failed to read request file: {e}")
        return None


def clear_request_file() -> None:
    """Remove request file after processing."""
    try:
        REQUEST_FILE.unlink(missing_ok=True)
    except KeyboardInterrupt:
        raise
    except Exception as e:
        logging.error(f"Failed to clear request file: {e}")


def check_disk_space(path: Path, required_mb: int = 1000) -> tuple[bool, str]:
    """Check if sufficient disk space is available.

    Args:
        path: Path to check disk space for
        required_mb: Required space in MB (default: 1GB)

    Returns:
        (has_space, error_message)
    """
    try:
        stat = shutil.disk_usage(path)
        available_mb = stat.free / (1024 * 1024)

        if available_mb < required_mb:
            return (
                False,
                f"Insufficient disk space: {available_mb:.1f}MB available, {required_mb}MB required",
            )

        return True, ""
    except KeyboardInterrupt:
        raise
    except Exception as e:
        return False, f"Failed to check disk space: {e}"


def is_network_error(error_output: str) -> bool:
    """Detect if error is network-related.

    Args:
        error_output: Error output text to check

    Returns:
        True if network error detected, False otherwise
    """
    for pattern in NETWORK_ERROR_PATTERNS:
        if re.search(pattern, error_output, re.IGNORECASE):
            return True
    return False


def validate_request(request: dict[str, Any]) -> bool:
    """Validate package installation request.

    Args:
        request: Request dictionary

    Returns:
        True if request is valid, False otherwise
    """
    # Required fields
    if "project_dir" not in request or "environment" not in request:
        logging.error("Request missing required fields")
        return False

    project_dir = Path(request["project_dir"])

    # Verify project directory exists
    if not project_dir.exists() or not project_dir.is_dir():
        logging.error(f"Project directory does not exist: {project_dir}")
        return False

    # Verify platformio.ini exists
    if not (project_dir / "platformio.ini").exists():
        logging.error(f"platformio.ini not found in {project_dir}")
        return False

    # Verify environment is alphanumeric (prevent injection)
    env = request["environment"]
    if not isinstance(env, str) or not env:
        logging.error(f"Invalid environment: {env}")
        return False

    return True


def run_package_install_with_retry(
    cmd: list[str],
    environment: str,
    project_dir: str,
    max_retries: int = MAX_NETWORK_RETRIES,
) -> tuple[int, str]:
    """Run package install with retry logic for network errors.

    Args:
        cmd: Command to execute
        environment: Environment name (for status updates)
        project_dir: Project directory (for status updates)
        max_retries: Maximum retry attempts (default: 3)

    Returns:
        (returncode, full_output)
    """
    for attempt in range(max_retries):
        if attempt > 0:
            retry_msg = f"Retry {attempt}/{max_retries - 1} after network error"
            logging.info(retry_msg)
            update_status(
                "installing",
                retry_msg,
                environment=environment,
                project_dir=project_dir,
            )
            time.sleep(5)  # Brief delay before retry

        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        output_lines = []
        for line in proc.stdout:  # type: ignore
            line_stripped = line.strip()
            if line_stripped:
                output_lines.append(line)
                logging.info(f"PlatformIO: {line_stripped}")
                update_status(
                    "installing",
                    line_stripped,
                    environment=environment,
                    project_dir=project_dir,
                )

        returncode = proc.wait()
        full_output = "".join(output_lines)

        if returncode == 0:
            return returncode, full_output

        # Check if network error and should retry
        if is_network_error(full_output) and attempt < max_retries - 1:
            logging.warning(
                f"Network error detected, will retry (attempt {attempt + 1}/{max_retries - 1})"
            )
            continue

        # Non-network error or out of retries
        return returncode, full_output

    return 1, full_output


def process_package_request(request: dict[str, Any]) -> bool:
    """Execute package installation request.

    Args:
        request: Request dictionary with project_dir and environment

    Returns:
        True if installation successful, False otherwise
    """
    project_dir = request["project_dir"]
    environment = request["environment"]

    logging.info(f"Processing package install request: {environment} in {project_dir}")

    # Check disk space before installation
    pio_packages_dir = Path.home() / ".platformio"
    has_space, error_msg = check_disk_space(pio_packages_dir, required_mb=1000)
    if not has_space:
        logging.error(f"Disk space check failed: {error_msg}")
        update_status("failed", error_msg)
        return False

    update_status(
        "installing",
        f"Installing packages for {environment}",
        environment=environment,
        project_dir=project_dir,
        started_at=time.time(),
    )

    cmd = [
        "pio",
        "pkg",
        "install",
        "--project-dir",
        project_dir,
        "--environment",
        environment,
    ]

    try:
        returncode, full_output = run_package_install_with_retry(
            cmd,
            environment,
            project_dir,
        )

        if returncode == 0:
            logging.info("Package installation completed successfully")
            update_status("completed", "Package installation successful")
            return True
        else:
            logging.error(f"Package installation failed with exit code {returncode}")

            # Provide helpful error message for network errors
            if is_network_error(full_output):
                error_msg = f"Package installation failed due to network error after {MAX_NETWORK_RETRIES} attempts"
            else:
                error_msg = f"Package installation failed (exit {returncode})"

            update_status("failed", error_msg)
            return False

    except KeyboardInterrupt:
        logging.warning("Package installation interrupted by user")
        update_status("failed", "Installation interrupted by user")
        raise
    except Exception as e:
        logging.error(f"Package installation error: {e}")
        update_status("failed", f"Installation error: {e}")
        return False


def should_shutdown() -> bool:
    """Check if daemon should shutdown.

    Returns:
        True if shutdown signal detected, False otherwise
    """
    # Check for shutdown signal file
    shutdown_file = DAEMON_DIR / "shutdown.signal"
    if shutdown_file.exists():
        logging.info("Shutdown signal detected")
        try:
            shutdown_file.unlink()
        except Exception:
            pass
        return True
    return False


def cleanup_and_exit() -> None:
    """Clean up daemon state and exit."""
    logging.info("Daemon shutting down")

    # Remove PID file
    try:
        PID_FILE.unlink(missing_ok=True)
    except KeyboardInterrupt:
        raise
    except Exception as e:
        logging.error(f"Failed to remove PID file: {e}")

    # Set final status
    update_status("idle", "Daemon shut down")

    sys.exit(0)


def run_daemon_loop() -> None:
    """Main daemon loop: process package installation requests."""
    logging.info(f"Daemon started with PID {os.getpid()}")
    update_status("idle", "Daemon ready")

    last_activity = time.time()

    while True:
        try:
            # Check for shutdown signal
            if should_shutdown():
                cleanup_and_exit()

            # Check idle timeout
            if time.time() - last_activity > IDLE_TIMEOUT:
                logging.info(f"Idle timeout reached ({IDLE_TIMEOUT}s), shutting down")
                cleanup_and_exit()

            # Check for new requests
            request = read_request_file()
            if request:
                logging.info(f"Received request: {request}")
                last_activity = time.time()

                # Validate request
                if validate_request(request):
                    # Process request
                    process_package_request(request)
                else:
                    update_status("failed", "Invalid request")

                # Clear request file
                clear_request_file()

            # Sleep briefly to avoid busy-wait
            time.sleep(0.5)

        except KeyboardInterrupt:
            logging.warning("Daemon interrupted by user")
            cleanup_and_exit()
        except Exception as e:
            logging.error(f"Daemon error: {e}", exc_info=True)
            # Continue running despite errors
            time.sleep(1)


def main() -> int:
    """Main entry point for daemon."""
    # Parse command-line arguments
    foreground = "--foreground" in sys.argv
    verbose = "--verbose" in sys.argv

    # Setup logging
    setup_logging(foreground=foreground)

    # Ensure daemon directory exists
    DAEMON_DIR.mkdir(parents=True, exist_ok=True)

    if foreground:
        # Run in foreground (for debugging)
        logging.info("Running in foreground mode")
        # Write PID file even in foreground mode for testing
        with open(PID_FILE, "w") as f:
            f.write(str(os.getpid()))
        try:
            run_daemon_loop()
        finally:
            PID_FILE.unlink(missing_ok=True)
        return 0

    # Check if daemon already running
    if PID_FILE.exists():
        try:
            with open(PID_FILE, "r") as f:
                existing_pid = int(f.read().strip())
            if psutil.pid_exists(existing_pid):
                logging.info(f"Daemon already running with PID {existing_pid}")
                print(f"Daemon already running with PID {existing_pid}")
                return 0
            else:
                # Stale PID file
                logging.info(f"Removing stale PID file for PID {existing_pid}")
                PID_FILE.unlink()
        except KeyboardInterrupt:
            raise
        except Exception as e:
            logging.warning(f"Error checking existing PID: {e}")
            PID_FILE.unlink(missing_ok=True)

    # Use daemoniker to properly daemonize
    try:
        with Daemonizer() as (is_setup, daemonizer):
            if is_setup:
                # Pre-daemon setup (runs as parent process)
                logging.info("Initializing daemon")

            # Daemonize (this returns twice - once in parent, once in child)
            is_parent, my_pid = daemonizer(
                DAEMON_NAME,
                pid_file=str(PID_FILE),
                chdir=str(Path.home()),
            )

            if is_parent:
                # Parent process exits here
                logging.info(f"Daemon started with PID {my_pid}")
                print(f"Daemon started with PID {my_pid}")
                return 0

            # This code runs only in the daemon child process
            # Write PID file manually (daemoniker may not do it on Windows)
            with open(PID_FILE, "w") as f:
                f.write(str(os.getpid()))

            run_daemon_loop()
    except KeyboardInterrupt:
        raise
    except Exception as e:
        logging.error(f"Failed to daemonize: {e}", exc_info=True)
        # Fall back to simple background mode
        logging.info("Falling back to simple background mode")
        print(f"Note: Using simple background mode (daemonization failed: {e})")

        # Write PID file
        with open(PID_FILE, "w") as f:
            f.write(str(os.getpid()))

        try:
            run_daemon_loop()
        finally:
            PID_FILE.unlink(missing_ok=True)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nDaemon interrupted by user")
        sys.exit(130)
