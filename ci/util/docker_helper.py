from ci.util.global_interrupt_handler import handle_keyboard_interrupt


#!/usr/bin/env python3
"""Generic Docker daemon helpers used by callers that need a running Docker
engine (e.g. `bash test --docker`, `bash profile --docker`, the qemu/avr8js
emulator runners).

The compile-Docker bits that used to live here (`get_docker_image_name`,
`is_docker_image_available`, `should_use_docker_for_board`) were removed in
#2812 along with the rest of the `niteris/fastled-compiler-*` image family
and `bash compile --docker`. What remains is platform-agnostic daemon-start
logic — install detection, Docker Desktop start (Windows / macOS / Linux),
and WSL2-backend diagnostics on Windows.
"""

import os
import subprocess
import sys
import time

# Import Docker command utilities from separate module to avoid circular imports
from ci.util.docker_command import (
    find_docker_executable,
    is_docker_available,
)


def is_docker_installed() -> bool:
    """Check if Docker is installed (may not be running).

    Returns:
        True if Docker executable is found, False otherwise
    """
    return find_docker_executable() is not None


def attempt_start_docker() -> tuple[bool, str]:
    """Attempt to start Docker if it's not running.

    Returns:
        Tuple of (success, message)
        - success: True if Docker is now running, False otherwise
        - message: Explanation of what was attempted/result
    """
    # If Docker is already running, return success
    if is_docker_available():
        return True, "Docker is already running"

    # Check if Docker is installed at all
    if not is_docker_installed():
        return (
            False,
            "Docker is not installed. Please install Docker Desktop from https://www.docker.com/products/docker-desktop",
        )

    # Try platform-specific Docker startup
    if sys.platform == "win32":
        return _start_docker_windows()
    elif sys.platform == "darwin":
        return _start_docker_macos()
    else:
        return _start_docker_linux()


def _check_wsl2_docker_backend() -> tuple[bool, str]:
    """Check if Docker's WSL2 backend is running on Windows.

    Returns:
        Tuple of (is_running, diagnostic_message)
        - is_running: True if docker-desktop WSL2 distribution is running
        - diagnostic_message: Information about WSL2 status
    """
    try:
        # Check if WSL is available
        result = subprocess.run(
            ["wsl", "--list", "--verbose"],
            capture_output=True,
            text=True,
            timeout=10,
        )

        if result.returncode != 0:
            return False, "WSL2 not available or not configured"

        output = result.stdout

        # Handle WSL output encoding issues (spaces between characters in Git Bash)
        # Clean up the output by removing extra spaces
        cleaned_lines: list[str] = []
        for line in output.split("\n"):
            # Remove null bytes and collapse multiple spaces
            cleaned = line.replace("\x00", "").strip()
            # If line has pattern like "d o c k e r", remove spaces between single chars
            if " " in cleaned and len([c for c in cleaned.split() if len(c) == 1]) > 3:
                # This line has spaced-out characters - remove spaces
                cleaned = cleaned.replace(" ", "")
            cleaned_lines.append(cleaned)

        # Look for docker-desktop distribution
        for line in cleaned_lines:
            line_lower = line.lower()
            if "docker-desktop" in line_lower or "dockerdesktop" in line_lower:
                # Parse WSL status line (format: "  * docker-desktop    Running    2")
                # After cleaning: "docker-desktopStopped2" or "docker-desktop Running 2"
                parts = line.split()

                # Check if line contains status keywords
                if "running" in line_lower:
                    return True, "docker-desktop WSL2 backend is running"
                elif "stopped" in line_lower:
                    return False, "docker-desktop WSL2 backend is stopped"
                else:
                    # Try to extract status from parts
                    if len(parts) >= 2:
                        status = parts[-2] if len(parts) >= 3 else parts[-1]
                        if status.lower() == "running":
                            return True, "docker-desktop WSL2 backend is running"
                        elif status.lower() == "stopped":
                            return False, "docker-desktop WSL2 backend is stopped"
                        else:
                            return (
                                False,
                                f"docker-desktop WSL2 backend status: {status}",
                            )

                    return False, "docker-desktop found but status unclear"

        return False, "docker-desktop WSL2 distribution not found"

    except FileNotFoundError:
        return False, "WSL2 not installed (wsl command not found)"
    except subprocess.TimeoutExpired:
        return False, "WSL2 command timed out"
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        return False, f"Failed to check WSL2 status: {e}"


def _find_docker_desktop_executable() -> str | None:
    """Find Docker Desktop executable path on Windows.

    Returns:
        Path to Docker Desktop.exe if found, None otherwise
    """
    docker_desktop_paths = [
        r"C:\Program Files\Docker\Docker\Docker Desktop.exe",
        r"C:\Program Files (x86)\Docker\Docker\Docker Desktop.exe",
        os.path.expanduser(
            r"~\AppData\Local\Programs\Docker\Docker\Docker Desktop.exe"
        ),
    ]

    for docker_path in docker_desktop_paths:
        if os.path.exists(docker_path):
            return docker_path

    return None


def _kill_docker_desktop_windows() -> bool:
    """Kill Docker Desktop processes on Windows.

    Returns:
        True if processes were killed successfully, False otherwise
    """
    try:
        # Kill Docker Desktop processes
        subprocess.run(
            ["taskkill", "/F", "/IM", "Docker Desktop.exe"],
            capture_output=True,
            timeout=10,
        )

        # Also kill the com.docker.backend process
        subprocess.run(
            ["taskkill", "/F", "/IM", "com.docker.backend.exe"],
            capture_output=True,
            timeout=10,
        )

        # Give processes time to terminate
        time.sleep(3)
        return True

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return False


def _restart_docker_desktop_windows() -> tuple[bool, str]:
    """Restart Docker Desktop on Windows to fix WSL2 backend issues.

    Returns:
        Tuple of (success, message)
    """
    print("  Attempting to restart Docker Desktop to fix WSL2 backend...", flush=True)

    # Find Docker Desktop executable
    docker_path = _find_docker_desktop_executable()
    if not docker_path:
        return False, "Docker Desktop executable not found"

    # Kill existing Docker Desktop processes
    print("  Stopping Docker Desktop...", flush=True)
    _kill_docker_desktop_windows()

    # Wait a bit for cleanup
    time.sleep(5)

    # Start Docker Desktop
    print("  Starting Docker Desktop...", flush=True)
    try:
        subprocess.Popen(
            [docker_path],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        # Wait for Docker Desktop and WSL2 backend to start
        print(
            "  Waiting for Docker Desktop and WSL2 backend to initialize...", flush=True
        )

        for attempt in range(120):  # Try for up to 2 minutes
            time.sleep(1)

            # Check if Docker engine is responding
            if is_docker_available():
                # Also verify WSL2 backend is running
                wsl_running, _ = _check_wsl2_docker_backend()
                if wsl_running:
                    return (
                        True,
                        "Docker Desktop restarted successfully - WSL2 backend is running",
                    )

            # Print progress every 15 seconds
            if (attempt + 1) % 15 == 0:
                print(f"  Still waiting ({attempt + 1}s)...", flush=True)

        return (
            False,
            "Docker Desktop started but WSL2 backend failed to initialize within 2 minutes",
        )

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        return False, f"Failed to restart Docker Desktop: {e}"


def _start_docker_windows() -> tuple[bool, str]:
    """Attempt to start Docker Desktop on Windows with WSL2 backend diagnostics.

    Returns:
        Tuple of (success, message)
    """
    try:
        # First, check if Docker is already running
        if is_docker_available():
            return True, "Docker is already running"

        # Check WSL2 backend status
        print("  Checking Docker Desktop WSL2 backend status...", flush=True)
        wsl_running, wsl_message = _check_wsl2_docker_backend()
        print(f"  WSL2 Status: {wsl_message}", flush=True)

        # If Docker Desktop is installed but WSL2 backend is stopped, restart it
        if not wsl_running and "stopped" in wsl_message.lower():
            print(
                "  Issue detected: Docker Desktop app may be running but WSL2 backend is stopped",
                flush=True,
            )
            return _restart_docker_desktop_windows()

        # Try to start Docker Desktop via the executable
        docker_path = _find_docker_desktop_executable()

        if docker_path:
            try:
                print("  Starting Docker Desktop...", flush=True)
                subprocess.Popen(
                    [docker_path],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                # Wait for Docker to start - Docker Desktop can take 60+ seconds
                print("  Waiting for Docker Desktop to initialize...", flush=True)
                # Try for up to 120 seconds (2 minutes)
                for attempt in range(120):
                    time.sleep(1)
                    if is_docker_available():
                        # Verify WSL2 backend is also running
                        wsl_running, wsl_msg = _check_wsl2_docker_backend()
                        if wsl_running:
                            return True, "Docker Desktop started successfully"
                        else:
                            print(f"  Warning: {wsl_msg}", flush=True)

                    # Print progress every 10 seconds
                    if (attempt + 1) % 10 == 0:
                        print(f"  Still waiting ({attempt + 1}s)...", flush=True)

                # If we got here, Docker didn't start properly - try restart
                print(
                    "  Docker Desktop didn't start properly, attempting full restart...",
                    flush=True,
                )
                return _restart_docker_desktop_windows()

            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception as e:
                return False, f"Failed to start Docker Desktop: {e}"

        return (
            False,
            "Docker Desktop not found. Please install it from https://www.docker.com/products/docker-desktop",
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        return False, f"Failed to start Docker: {e}"


def _start_docker_macos() -> tuple[bool, str]:
    """Attempt to start Docker Desktop on macOS.

    Returns:
        Tuple of (success, message)
    """
    try:
        # Use `open -a` to launch Docker Desktop
        subprocess.run(
            ["open", "-a", "Docker"],
            capture_output=True,
            timeout=10,
            check=False,
        )
        # Wait for Docker to start - up to 60 seconds
        print("  Waiting for Docker Desktop to initialize...", flush=True)
        for attempt in range(60):
            time.sleep(1)
            if is_docker_available():
                return True, "Docker Desktop started successfully"
            if (attempt + 1) % 10 == 0:
                print(f"  Still waiting ({attempt + 1}s)...", flush=True)
        return False, "Docker Desktop did not start within 60 seconds"
    except FileNotFoundError:
        return (
            False,
            "Docker Desktop not found. Please install it from https://www.docker.com/products/docker-desktop",
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        return False, f"Failed to start Docker Desktop on macOS: {e}"


def _start_docker_linux() -> tuple[bool, str]:
    """Attempt to start Docker daemon on Linux.

    Returns:
        Tuple of (success, message)
    """
    try:
        # Try systemctl first (modern systems)
        try:
            result = subprocess.run(
                ["sudo", "systemctl", "start", "docker"],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            if result.returncode == 0:
                # Wait for Docker to be ready
                for _ in range(30):
                    time.sleep(1)
                    if is_docker_available():
                        return True, "Docker daemon started via systemctl"
                return False, "Docker daemon started but is not responding"
        except FileNotFoundError:
            pass

        # Try service command (older systems)
        try:
            result = subprocess.run(
                ["sudo", "service", "docker", "start"],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            if result.returncode == 0:
                # Wait for Docker to be ready
                for _ in range(30):
                    time.sleep(1)
                    if is_docker_available():
                        return True, "Docker daemon started via service"
                return False, "Docker daemon started but is not responding"
        except FileNotFoundError:
            pass

        return (
            False,
            "Could not find systemctl or service command. Please ensure Docker is installed.",
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return False, "Failed to start Docker: Unknown error"
