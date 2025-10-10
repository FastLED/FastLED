#!/usr/bin/env python3
"""Docker helper utilities for FastLED compilation optimization.

This module provides utilities to detect Docker availability and determine
whether Docker should be used for compilation based on system configuration.
"""

import subprocess
from typing import Optional

from ci.docker.build_image import extract_architecture, generate_config_hash


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


def get_docker_image_name(board_name: str) -> str:
    """Get the Docker image name for a specific board.

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32dev')

    Returns:
        Docker image name (e.g., 'fastled-platformio-avr-uno-abc12345')

    Raises:
        ValueError: If board configuration cannot be determined
    """
    try:
        architecture = extract_architecture(board_name)
        config_hash = generate_config_hash(board_name)
        return f"fastled-platformio-{architecture}-{board_name}-{config_hash}"
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
