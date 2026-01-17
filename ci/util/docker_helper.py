from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""Docker helper utilities for FastLED compilation optimization.

This module provides utilities to detect Docker availability and determine
whether Docker should be used for compilation based on system configuration.
"""

import os
import subprocess
import sys
import time
from typing import Optional

from ci.docker_utils.build_platforms import (
    get_docker_image_name as get_platform_image_name,
)
from ci.docker_utils.build_platforms import (
    get_platform_for_board,
)

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
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
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

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
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

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
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

            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                return False, f"Failed to start Docker Desktop: {e}"

        return (
            False,
            "Docker Desktop not found. Please install it from https://www.docker.com/products/docker-desktop",
        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        return False, f"Failed to start Docker: {e}"


def _start_docker_macos() -> tuple[bool, str]:
    """Attempt to start Docker Desktop on macOS.

    Returns:
        Tuple of (success, message)
    """
    try:
        # Try to open Docker Desktop app
        subprocess.run(
            ["open", "-a", "Docker"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5,
        )

        # Wait for Docker to become available
        for _ in range(30):  # Try for 30 seconds
            time.sleep(1)
            if is_docker_available():
                return True, "Docker Desktop started successfully"

        return (
            False,
            "Docker Desktop started but failed to become available after 30 seconds",
        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        return (
            False,
            "Docker Desktop not found. Please install it from https://www.docker.com/products/docker-desktop",
        )


def _start_docker_linux() -> tuple[bool, str]:
    """Attempt to start Docker on Linux using systemctl or service.

    Returns:
        Tuple of (success, message)
    """
    try:
        # Try systemctl first (modern systemd systems)
        result = subprocess.run(
            ["sudo", "systemctl", "start", "docker"],
            capture_output=True,
            text=True,
            timeout=10,
        )

        if result.returncode == 0:
            # Wait a bit for Docker to start
            for _ in range(10):
                time.sleep(1)
                if is_docker_available():
                    return True, "Docker service started successfully"

            return False, "Docker service started but failed to become available"

        # Try service command as fallback
        result = subprocess.run(
            ["sudo", "service", "docker", "start"],
            capture_output=True,
            text=True,
            timeout=10,
        )

        if result.returncode == 0:
            for _ in range(10):
                time.sleep(1)
                if is_docker_available():
                    return True, "Docker service started successfully"

            return False, "Docker service started but failed to become available"

        return (
            False,
            "Failed to start Docker service. Please ensure Docker is installed or start it manually.",
        )
    except subprocess.TimeoutExpired:
        return False, "Timeout attempting to start Docker service"
    except FileNotFoundError:
        return (
            False,
            "Could not find systemctl or service command. Please ensure Docker is installed.",
        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        return False, "Failed to start Docker: Unknown error"


def get_docker_image_name(board_name: str) -> str:
    """Get the Docker image name for a specific board.

    Uses the new platform-based naming scheme: niteris/fastled-compiler-{platform}:latest

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32dev', 'esp32s3')

    Returns:
        Docker image name with tag (e.g., 'niteris/fastled-compiler-avr:latest',
        'niteris/fastled-compiler-esp-32s3:latest')

    Raises:
        ValueError: If board is not mapped to a platform
    """
    # Get platform for this board using the new mapping
    platform = get_platform_for_board(board_name)
    if not platform:
        raise ValueError(
            f"Board '{board_name}' is not mapped to a Docker platform family. "
            f"Please check ci/docker_utils/build_platforms.py"
        )

    # Generate full image name with tag
    image_base = get_platform_image_name(platform)
    return f"{image_base}:latest"


def is_docker_image_available(board_name: str) -> bool:
    """Check if a Docker image exists for the specified board.

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32dev')

    Returns:
        True if Docker image exists locally, False otherwise
    """
    try:
        docker_exe = find_docker_executable()
        if not docker_exe:
            return False

        image_name = get_docker_image_name(board_name)
        result = subprocess.run(
            [docker_exe, "image", "inspect", image_name],
            capture_output=True,
            text=True,
            check=False,
            timeout=5,
        )
        return result.returncode == 0
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except (subprocess.TimeoutExpired, ValueError, Exception):
        return False


def should_use_docker_for_board(
    board_name: str, verbose: bool = False
) -> tuple[bool, Optional[str]]:
    """Determine if Docker should be used for compiling a specific board.

    This function checks:
    1. If Docker is installed and running
    2. If the Docker image for the board exists locally

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32dev')
        verbose: Whether to print diagnostic messages

    Returns:
        Tuple of (should_use_docker, reason)
        - should_use_docker: True if Docker should be used, False otherwise
        - reason: Optional string explaining the decision
    """
    # Check if Docker is available
    if not is_docker_available():
        reason = "Docker not installed or not running"
        if verbose:
            print(f"ℹ  {reason} - using native compilation")
        return False, reason

    # Check if Docker image exists
    if not is_docker_image_available(board_name):
        try:
            image_name = get_docker_image_name(board_name)
            reason = f"Docker image '{image_name}' not found"
            if verbose:
                print(f"ℹ  {reason} - using native compilation")
                print("   To build the image, run:")
                print(f"   bash compile --docker --build {board_name} Blink")
        except ValueError as e:
            reason = str(e)
            if verbose:
                print(f"ℹ  {reason} - using native compilation")
        return False, reason

    # Docker is available and image exists
    try:
        image_name = get_docker_image_name(board_name)
        reason = f"Using Docker image '{image_name}'"
        if verbose:
            print(f"✓ {reason} for faster compilation")
        return True, reason
    except ValueError as e:
        reason = str(e)
        if verbose:
            print(f"ℹ  {reason} - using native compilation")
        return False, reason


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python docker_helper.py <board_name>")
        print("Example: python docker_helper.py uno")
        sys.exit(1)

    board = sys.argv[1]
    use_docker, reason = should_use_docker_for_board(board, verbose=True)

    if use_docker:
        print(f"\nResult: Docker SHOULD be used for {board}")
        sys.exit(0)
    else:
        print(f"\nResult: Docker should NOT be used for {board}")
        print(f"Reason: {reason}")
        sys.exit(1)
