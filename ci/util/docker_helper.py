#!/usr/bin/env python3
"""Docker helper utilities for FastLED compilation optimization.

This module provides utilities to detect Docker availability and determine
whether Docker should be used for compilation based on system configuration.
"""

import os
import subprocess
import sys
import time
from typing import Optional, Tuple

from ci.docker.build_image import generate_config_hash


def is_docker_installed() -> bool:
    """Check if Docker is installed (may not be running).

    Returns:
        True if Docker executable is found, False otherwise
    """
    try:
        # Try to find docker in PATH
        result = subprocess.run(
            ["where", "docker"] if sys.platform == "win32" else ["which", "docker"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        return result.returncode == 0
    except Exception:
        return False


def is_docker_available() -> bool:
    """Check if Docker is installed and running.

    Returns:
        True if Docker is available, False otherwise
    """
    try:
        result = subprocess.run(
            ["docker", "version"],
            capture_output=True,
            text=True,
            check=False,
            timeout=5,
        )
        return result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError, Exception):
        return False


def attempt_start_docker() -> Tuple[bool, str]:
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


def _start_docker_windows() -> Tuple[bool, str]:
    """Attempt to start Docker Desktop on Windows.

    Returns:
        Tuple of (success, message)
    """
    try:
        # Try to start Docker Desktop via the executable
        docker_desktop_paths = [
            os.path.expanduser(r"C:\Program Files\Docker\Docker\Docker Desktop.exe"),
            os.path.expanduser(
                r"C:\Program Files (x86)\Docker\Docker\Docker Desktop.exe"
            ),
        ]

        for docker_path in docker_desktop_paths:
            if os.path.exists(docker_path):
                try:
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
                            return True, "Docker Desktop started successfully"
                        # Print progress every 10 seconds
                        if (attempt + 1) % 10 == 0:
                            print(f"  Still waiting ({attempt + 1}s)...", flush=True)

                    return (
                        False,
                        "Docker Desktop was started but failed to become available within 2 minutes",
                    )
                except Exception:
                    continue

        return (
            False,
            "Docker Desktop not found. Please install it from https://www.docker.com/products/docker-desktop",
        )
    except Exception:
        return False, "Failed to start Docker: Unknown error"


def _start_docker_macos() -> Tuple[bool, str]:
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
    except Exception:
        return (
            False,
            "Docker Desktop not found. Please install it from https://www.docker.com/products/docker-desktop",
        )


def _start_docker_linux() -> Tuple[bool, str]:
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
    except Exception:
        return False, "Failed to start Docker: Unknown error"


def get_docker_image_name(board_name: str) -> str:
    """Get the Docker image name for a specific board.

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32dev')

    Returns:
        Docker image name (e.g., 'fastled-platformio-uno-abc12345')

    Raises:
        ValueError: If board configuration cannot be determined
    """
    try:
        config_hash = generate_config_hash(board_name)
        return f"fastled-platformio-{board_name}-{config_hash}"
    except Exception as e:
        raise ValueError(
            f"Failed to determine Docker image name for board '{board_name}': {e}"
        ) from e


def is_docker_image_available(board_name: str) -> bool:
    """Check if a Docker image exists for the specified board.

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32dev')

    Returns:
        True if Docker image exists locally, False otherwise
    """
    try:
        image_name = get_docker_image_name(board_name)
        result = subprocess.run(
            ["docker", "image", "inspect", image_name],
            capture_output=True,
            text=True,
            check=False,
            timeout=5,
        )
        return result.returncode == 0
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
                print(f"   To build the image, run:")
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
