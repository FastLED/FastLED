from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


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

import configparser
import json
import logging
import os
import platform
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
from logging.handlers import RotatingFileHandler
from pathlib import Path
from typing import Any

import psutil
from daemoniker import Daemonizer

from ci.util.build_process_tracker import BuildProcessTracker
from ci.util.pio_package_messages import DaemonState, DaemonStatus, PackageRequest


# Daemon configuration
DAEMON_NAME = "fastled_pio_package_daemon"
DAEMON_DIR = Path.home() / ".fastled" / "daemon"
PID_FILE = DAEMON_DIR / f"{DAEMON_NAME}.pid"
STATUS_FILE = DAEMON_DIR / "package_status.json"
REQUEST_FILE = DAEMON_DIR / "package_request.json"
LOG_FILE = DAEMON_DIR / "daemon.log"
BUILD_REGISTRY_FILE = DAEMON_DIR / "build_processes.json"
ORPHAN_CHECK_INTERVAL = 5  # Check for orphaned processes every 5 seconds
IDLE_TIMEOUT = 43200  # 12 hours

# Global state to track installation status and daemon info
_installation_in_progress = False
_installation_lock = threading.Lock()
_daemon_pid: int | None = None
_daemon_started_at: float | None = None

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


def read_status_file_safe() -> DaemonStatus:
    """Read status file with corruption recovery.

    Returns:
        DaemonStatus object (or default if corrupted)
    """
    default_status = DaemonStatus(
        state=DaemonState.IDLE,
        message="",
        updated_at=time.time(),
    )

    if not STATUS_FILE.exists():
        return default_status

    try:
        with open(STATUS_FILE, "r") as f:
            data = json.load(f)

        # Parse into typed DaemonStatus
        return DaemonStatus.from_dict(data)

    except (json.JSONDecodeError, ValueError) as e:
        logging.warning(f"Corrupted status file detected: {e}")
        logging.warning("Creating fresh status file")

        # Write fresh status file
        write_status_file_atomic(default_status.to_dict())

        return default_status
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logging.error(f"Unexpected error reading status file: {e}")
        write_status_file_atomic(default_status.to_dict())
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
        handle_keyboard_interrupt_properly()
        temp_file.unlink(missing_ok=True)
        raise
    except Exception as e:
        logging.error(f"Failed to write status file: {e}")
        temp_file.unlink(missing_ok=True)


def update_status(state: DaemonState, message: str, **kwargs: Any) -> None:
    """Update status file with current daemon state.

    Args:
        state: DaemonState enum value
        message: Human-readable status message
        **kwargs: Additional fields to include in status
                 Use installation_in_progress=True/False to override global flag
    """
    global _installation_in_progress, _daemon_pid, _daemon_started_at

    # Allow explicit override of installation_in_progress flag
    if "installation_in_progress" not in kwargs:
        kwargs["installation_in_progress"] = _installation_in_progress

    # Create typed DaemonStatus object
    status_obj = DaemonStatus(
        state=state,
        message=message,
        updated_at=time.time(),
        daemon_pid=_daemon_pid,
        daemon_started_at=_daemon_started_at,
        **kwargs,
    )

    write_status_file_atomic(status_obj.to_dict())


def read_request_file() -> PackageRequest | None:
    """Read and parse request file.

    Returns:
        PackageRequest object if valid request exists, None otherwise
    """
    if not REQUEST_FILE.exists():
        return None

    try:
        with open(REQUEST_FILE, "r") as f:
            data = json.load(f)

        # Parse into typed PackageRequest
        return PackageRequest.from_dict(data)

    except (json.JSONDecodeError, ValueError, TypeError) as e:
        logging.error(f"Failed to parse request file: {e}")
        return None
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logging.error(f"Unexpected error reading request file: {e}")
        return None


def clear_request_file() -> None:
    """Remove request file after processing."""
    try:
        REQUEST_FILE.unlink(missing_ok=True)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
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
        handle_keyboard_interrupt_properly()
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


def get_clean_windows_env() -> dict[str, str]:
    """Create a clean Windows environment without Git Bash indicators.

    This removes Git Bash environment variables (MSYSTEM, TERM, SHELL, etc.)
    that might cause ESP-IDF tooling to detect Git Bash and abort installation.

    Returns:
        Clean environment dict suitable for running pio via cmd.exe
    """
    # Start with minimal system environment
    clean_env: dict[str, str] = {}

    # Git Bash environment variables to EXCLUDE (these cause ESP-IDF to abort)
    git_bash_vars = {
        "MSYSTEM",  # MSYS2/Git Bash indicator
        "MSYSTEM_CARCH",
        "MSYSTEM_CHOST",
        "MSYSTEM_PREFIX",
        "MINGW_CHOST",
        "MINGW_PACKAGE_PREFIX",
        "MINGW_PREFIX",
        "TERM",  # Often set to 'xterm' in Git Bash
        "SHELL",  # Points to bash.exe in Git Bash
        "BASH",
        "BASH_ENV",
        "SHLVL",
        "OLDPWD",
        "LS_COLORS",
        "ACLOCAL_PATH",
        "MANPATH",
        "INFOPATH",
        "PKG_CONFIG_PATH",
        "ORIGINAL_PATH",
        "MSYS",
    }

    # Copy safe environment variables from parent
    for key, value in os.environ.items():
        # Skip Git Bash indicators
        if key in git_bash_vars:
            continue

        # Skip variables starting with MSYS or MINGW prefixes
        if key.startswith(("MSYS", "MINGW", "BASH_")):
            continue

        # Keep important system variables
        clean_env[key] = value

    # Ensure critical variables are set for Windows
    if platform.system() == "Windows":
        # Force UTF-8 encoding
        clean_env["PYTHONIOENCODING"] = "utf-8"
        clean_env["PYTHONUTF8"] = "1"

        # Set COMSPEC to cmd.exe if not already set
        if "COMSPEC" not in clean_env:
            clean_env["COMSPEC"] = "C:\\Windows\\System32\\cmd.exe"

        # Ensure SystemRoot is set (required by many Windows tools)
        if "SystemRoot" not in clean_env:
            clean_env["SystemRoot"] = "C:\\Windows"

        # Ensure TEMP/TMP are set
        if "TEMP" not in clean_env:
            clean_env["TEMP"] = os.environ.get("TEMP", "C:\\Windows\\Temp")
        if "TMP" not in clean_env:
            clean_env["TMP"] = os.environ.get("TMP", "C:\\Windows\\Temp")

    logging.info("Created clean Windows environment (removed Git Bash indicators)")
    logging.debug(f"Excluded variables: {', '.join(sorted(git_bash_vars))}")

    return clean_env


def validate_request(request: PackageRequest) -> bool:
    """Validate package installation request.

    Args:
        request: PackageRequest object

    Returns:
        True if request is valid, False otherwise
    """
    project_dir = Path(request.project_dir)

    # Verify project directory exists
    if not project_dir.exists() or not project_dir.is_dir():
        logging.error(f"Project directory does not exist: {project_dir}")
        return False

    # Verify platformio.ini exists
    if not (project_dir / "platformio.ini").exists():
        logging.error(f"platformio.ini not found in {project_dir}")
        return False

    # Verify environment is valid (if provided)
    env = request.environment
    if env is not None and (not isinstance(env, str) or not env):
        logging.error(f"Invalid environment: {env}")
        return False

    return True


def run_package_install_with_retry(
    cmd: list[str],
    environment: str | None,
    project_dir: str,
    caller_pid: str | int = "unknown",
    caller_cwd: str = "unknown",
    max_retries: int = MAX_NETWORK_RETRIES,
) -> tuple[int, str]:
    """Run package install with retry logic for network errors.

    On Windows, this runs pio via cmd.exe shell with a clean environment
    (no Git Bash indicators) to prevent ESP-IDF tooling from aborting.

    Args:
        cmd: Command to execute
        environment: Environment name (for status updates), or None for default
        project_dir: Project directory (for status updates)
        caller_pid: Caller process PID (for status updates)
        caller_cwd: Caller working directory (for status updates)
        max_retries: Maximum retry attempts (default: 3)

    Returns:
        (returncode, full_output)
    """
    # Determine if we should use shell on Windows
    is_windows = platform.system() == "Windows"

    # On Windows: use cmd.exe shell with clean environment (no Git Bash)
    # On Linux/Mac: use direct execution with inherited environment
    if is_windows:
        # Use shell=True on Windows to invoke via cmd.exe
        # Convert list to string for shell execution
        cmd_str = " ".join(cmd)
        shell = True
        env = get_clean_windows_env()
        logging.info("Windows detected: using cmd.exe shell with clean environment")
        logging.debug(f"Command string: {cmd_str}")
    else:
        # Non-Windows: use direct execution (no shell)
        cmd_str = cmd
        shell = False
        env = None  # Inherit environment
        logging.info("Non-Windows platform: using direct execution")

    for attempt in range(max_retries):
        if attempt > 0:
            retry_msg = f"Retry {attempt}/{max_retries - 1} after network error"
            logging.info(retry_msg)
            update_status(
                DaemonState.INSTALLING,
                retry_msg,
                environment=environment,
                project_dir=project_dir,
                caller_pid=caller_pid,
                caller_cwd=caller_cwd,
                current_operation=retry_msg,
            )
            time.sleep(5)  # Brief delay before retry

        # Log subprocess start
        logging.info(
            f"Starting subprocess: pio pkg install (attempt {attempt + 1}/{max_retries})"
        )
        logging.debug(f"Command: {cmd_str if is_windows else ' '.join(cmd)}")

        proc = subprocess.Popen(
            cmd_str,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            shell=shell,
            env=env,
        )

        # Log subprocess PID
        logging.debug(f"Subprocess PID: {proc.pid}")

        output_lines = []
        for line in proc.stdout:  # type: ignore
            line_stripped = line.strip()
            if line_stripped:
                output_lines.append(line)  # type: ignore[reportUnknownMemberType]
                logging.info(f"PlatformIO: {line_stripped}")

                # Extract operation details for better status visibility
                current_op = None
                if "Tool Manager: Installing" in line_stripped:
                    current_op = line_stripped
                elif "Downloading" in line_stripped:
                    current_op = "Downloading packages..."
                elif "Unpacking" in line_stripped:
                    current_op = "Unpacking packages..."

                update_status(
                    DaemonState.INSTALLING,
                    line_stripped,
                    environment=environment,
                    project_dir=project_dir,
                    caller_pid=caller_pid,
                    caller_cwd=caller_cwd,
                    current_operation=current_op,
                )

        # Log stdout closure
        logging.debug("PlatformIO output stream closed (EOF received)")

        # Check subprocess state before wait
        if proc.poll() is not None:
            logging.warning(f"Subprocess already exited with code {proc.returncode}")

        returncode = proc.wait()

        # Log subprocess exit
        logging.info(f"Subprocess exited with code {returncode}")

        # Detect signal termination
        if returncode < 0:
            signal_num = -returncode
            logging.error(f"Subprocess terminated by signal {signal_num}")
        elif returncode == 15:
            logging.error(
                "Exit code 15 detected. This usually indicates SIGTERM. "
                "Check if kill_lingering_processes() killed the daemon subprocess."
            )

        full_output = "".join(output_lines)  # type: ignore[reportUnknownArgumentType]

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

    return 1, full_output  # type: ignore[reportPossiblyUnboundVariable]


def get_default_environment(project_dir: str) -> str | None:
    """Read default_envs from platformio.ini.

    Args:
        project_dir: Path to PlatformIO project directory

    Returns:
        First default environment name, or None if not found
    """
    ini_path = Path(project_dir) / "platformio.ini"
    if not ini_path.exists():
        return None

    try:
        config = configparser.ConfigParser()
        config.read(ini_path)

        # Get default_envs from [platformio] section
        if config.has_section("platformio") and config.has_option(
            "platformio", "default_envs"
        ):
            default_envs = config.get("platformio", "default_envs")
            # Can be comma-separated list, take first one
            first_env = default_envs.split(",")[0].strip()
            return first_env if first_env else None

        return None
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logging.warning(f"Failed to read default_envs from platformio.ini: {e}")
        return None


def process_package_request(request: PackageRequest) -> bool:
    """Execute package installation request.

    Args:
        request: PackageRequest object with project_dir and optional environment

    Returns:
        True if installation successful, False otherwise
    """
    project_dir = request.project_dir
    environment = request.environment
    caller_pid = request.caller_pid
    caller_cwd = request.caller_cwd

    # If no environment specified, use default_envs from platformio.ini
    # This prevents PlatformIO from installing packages for ALL environments
    if not environment:
        environment = get_default_environment(project_dir)
        if environment:
            logging.info(
                f"Using default environment from platformio.ini: {environment}"
            )
        else:
            logging.warning(
                "No default_envs found in platformio.ini, will install for all environments"
            )

    env_desc = environment if environment else "all environments"
    logging.info(f"Processing package install request: {env_desc} in {project_dir}")

    # Check disk space before installation
    pio_packages_dir = Path.home() / ".platformio"
    has_space, error_msg = check_disk_space(pio_packages_dir, required_mb=1000)
    if not has_space:
        logging.error(f"Disk space check failed: {error_msg}")
        update_status(DaemonState.FAILED, error_msg)
        return False

    update_status(
        DaemonState.INSTALLING,
        f"Installing packages for {env_desc}",
        environment=environment,
        project_dir=project_dir,
        request_started_at=time.time(),
        caller_pid=caller_pid,
        caller_cwd=caller_cwd,
    )

    # Build command - omit --environment if None to use PlatformIO default
    cmd = [
        "pio",
        "pkg",
        "install",
        "--project-dir",
        project_dir,
    ]

    if environment:
        cmd.extend(["--environment", environment])

    try:
        # Mark installation as in progress
        global _installation_in_progress
        with _installation_lock:
            _installation_in_progress = True

        returncode, full_output = run_package_install_with_retry(
            cmd,
            environment,
            project_dir,
            caller_pid,
            caller_cwd,
        )

        if returncode == 0:
            logging.info("Package installation completed successfully")

            # POST-INSTALL VALIDATION
            if environment:
                logging.info("Validating installed packages...")
                from ci.util.package_validator import check_all_packages

                try:
                    is_valid, errors = check_all_packages(
                        Path(project_dir), environment
                    )
                    if not is_valid:
                        logging.error("Post-install validation failed:")
                        for error in errors:
                            logging.error(f"  - {error}")
                        update_status(
                            DaemonState.FAILED,
                            f"Package validation failed: {errors[0]}",
                        )
                        return False

                    logging.info("Post-install validation passed")
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    logging.warning(f"Post-install validation error (non-fatal): {e}")
                    # Don't fail installation on validation error, just log

            update_status(
                DaemonState.COMPLETED,
                "Package installation successful",
                installation_in_progress=False,
            )
            return True
        else:
            logging.error(f"Package installation failed with exit code {returncode}")

            # Provide helpful error message for network errors
            if is_network_error(full_output):
                error_msg = f"Package installation failed due to network error after {MAX_NETWORK_RETRIES} attempts"
            else:
                error_msg = f"Package installation failed (exit {returncode})"

            update_status(DaemonState.FAILED, error_msg, installation_in_progress=False)
            return False

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        logging.warning("Package installation interrupted by user")
        update_status(
            DaemonState.FAILED,
            "Installation interrupted by user",
            installation_in_progress=False,
        )
        raise
    except Exception as e:
        logging.error(f"Package installation error: {e}")
        update_status(
            DaemonState.FAILED,
            f"Installation error: {e}",
            installation_in_progress=False,
        )
        return False
    finally:
        # Mark installation as complete
        with _installation_lock:
            _installation_in_progress = False


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
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            pass
        return True
    return False


def signal_handler(signum: int, frame: object) -> None:
    """Handle SIGTERM/SIGINT - refuse shutdown during installation."""
    global _installation_in_progress

    signal_name = "SIGTERM" if signum == signal.SIGTERM else "SIGINT"

    with _installation_lock:
        if _installation_in_progress:
            logging.warning(
                f"Received {signal_name} during active package installation. "
                f"Refusing graceful shutdown. Use force kill (SIGKILL) to terminate."
            )
            print(
                f"\n⚠️  {signal_name} received during package installation\n"
                f"⚠️  Cannot shutdown gracefully while installation is active\n"
                f"⚠️  Installation must complete atomically to prevent corruption\n"
                f"⚠️  Use 'kill -9 {os.getpid()}' to force termination (may corrupt packages)\n",
                flush=True,
            )
            return  # Refuse shutdown
        else:
            logging.info(f"Received {signal_name}, shutting down gracefully")
            cleanup_and_exit()


def cleanup_and_exit() -> None:
    """Clean up daemon state and exit."""
    logging.info("Daemon shutting down")

    # Remove PID file
    try:
        PID_FILE.unlink(missing_ok=True)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logging.error(f"Failed to remove PID file: {e}")

    # Set final status
    update_status(DaemonState.IDLE, "Daemon shut down")

    sys.exit(0)


def run_daemon_loop() -> None:
    """Main daemon loop: process package installation requests."""
    global _daemon_pid, _daemon_started_at

    # Register signal handlers
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)

    # Initialize daemon tracking variables
    _daemon_pid = os.getpid()
    _daemon_started_at = time.time()

    # Initialize build process tracker
    build_tracker = BuildProcessTracker(BUILD_REGISTRY_FILE)

    logging.info(f"Daemon started with PID {_daemon_pid}")
    update_status(DaemonState.IDLE, "Daemon ready")

    last_activity = time.time()
    last_orphan_check = time.time()

    while True:
        try:
            # Check for shutdown signal
            if should_shutdown():
                cleanup_and_exit()

            # Check idle timeout
            if time.time() - last_activity > IDLE_TIMEOUT:
                logging.info(f"Idle timeout reached ({IDLE_TIMEOUT}s), shutting down")
                cleanup_and_exit()

            # Periodically check for and cleanup orphaned build processes
            if time.time() - last_orphan_check >= ORPHAN_CHECK_INTERVAL:
                try:
                    orphaned_clients = build_tracker.cleanup_orphaned_processes()
                    if orphaned_clients:
                        logging.info(
                            f"Cleaned up orphaned processes for {len(orphaned_clients)} dead clients: {orphaned_clients}"
                        )
                    last_orphan_check = time.time()
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    logging.error(f"Error during orphan cleanup: {e}", exc_info=True)

            # Check for new requests
            request = read_request_file()
            if request:
                logging.info(f"Received request: {request}")

                # CHECK: Refuse if installation already in progress
                with _installation_lock:
                    if _installation_in_progress:
                        # Read current status to get active caller info
                        current_status = read_status_file_safe()
                        active_pid = current_status.caller_pid or "unknown"
                        active_cwd = current_status.caller_cwd or "unknown"

                        # Get incoming request info
                        incoming_pid = request.caller_pid
                        incoming_cwd = request.caller_cwd

                        logging.warning(
                            f"Installation already in progress, refusing concurrent request. "
                            f"Active installation: PID={active_pid}, CWD={active_cwd}. "
                            f"Incoming request: PID={incoming_pid}, CWD={incoming_cwd}"
                        )
                        update_status(
                            DaemonState.FAILED,
                            f"Installation already in progress (active PID: {active_pid}, CWD: {active_cwd})",
                        )
                        clear_request_file()
                        continue  # Skip this request

                last_activity = time.time()

                # Validate request
                if validate_request(request):
                    # Process request
                    process_package_request(request)
                else:
                    update_status(DaemonState.FAILED, "Invalid request")

                # Clear request file
                clear_request_file()

            # Sleep briefly to avoid busy-wait
            time.sleep(0.5)

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
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
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            logging.warning(f"Error checking existing PID: {e}")
            PID_FILE.unlink(missing_ok=True)

    # Use daemoniker to properly daemonize
    try:
        daemonizer_context = Daemonizer()
        with daemonizer_context as (is_setup, daemonizer):  # type: ignore[reportUnknownVariableType]
            if is_setup:
                # Pre-daemon setup (runs as parent process)
                logging.info("Initializing daemon")

            # Daemonize (this returns twice - once in parent, once in child)
            # Note: On Windows, pid_file must be the first positional arg, not a keyword arg
            result = daemonizer(
                str(PID_FILE),  # pid_file (first positional arg)
                chdir=str(Path.home()),
            )
            # Unpack result - may be just is_parent or (is_parent,)
            is_parent = result[0] if isinstance(result, (list, tuple)) else result
            my_pid = os.getpid()  # Get actual PID

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
        handle_keyboard_interrupt_properly()
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
        handle_keyboard_interrupt_properly()
        raise
        print("\nDaemon interrupted by user")
        sys.exit(130)
