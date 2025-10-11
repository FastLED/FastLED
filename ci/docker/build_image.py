"""Docker image build utilities for FastLED PlatformIO compilation.

This module provides functions for building Docker images with pre-cached
PlatformIO dependencies for faster cross-platform compilation.
"""

import hashlib
import json
import subprocess
import sys
from typing import Optional

from ci.boards import create_board


def get_platformio_version() -> str:
    """Get the installed PlatformIO version.

    Returns:
        Version string (e.g., '6.1.15')
    """
    try:
        result = subprocess.run(
            ["pio", "--version"],
            capture_output=True,
            text=True,
            check=True,
        )
        # Output format: "PlatformIO Core, version 6.1.15"
        version_line = result.stdout.strip()
        if "version" in version_line:
            return version_line.split("version")[-1].strip()
        return version_line
    except (subprocess.CalledProcessError, FileNotFoundError):
        # If PlatformIO is not installed, return a default version
        return "unknown"


def extract_architecture(board_name: str) -> str:
    """Return board name directly - no mapping needed.

    This function previously mapped board names to architecture names via a large
    lookup table, but that mapping was unnecessary. The board name is already
    unique and is passed through correctly to compilation commands.

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32s3', 'esp8266')

    Returns:
        The board name itself (used for Docker image naming)
    """
    # Simply return the board name - no mapping table needed
    # The board name is already unique and works correctly for compilation
    return board_name


def generate_config_hash(board_name: str, framework: Optional[str] = None) -> str:
    """Generate deterministic hash from board configuration.

    This hash is used for Docker image naming to ensure automatic cache
    invalidation when platform configuration changes.

    The hash is based on:
    - platform: Platform name (e.g., 'espressif32')
    - framework: Framework name (e.g., 'arduino')
    - board: Board identifier (e.g., 'esp32-s3-devkitc-1')
    - platform_packages: Custom platform packages/URLs
    - platformio_version: PlatformIO version for reproducibility

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32s3')
        framework: Optional framework override (e.g., 'arduino')

    Returns:
        8-character hex hash of the configuration

    Raises:
        ValueError: If board not found
    """
    try:
        board = create_board(board_name, no_project_options=True)
    except Exception as e:
        raise ValueError(f"Failed to create board '{board_name}': {e}") from e

    # Use framework override if provided
    if framework:
        board.framework = framework

    # Create deterministic hash from config
    config_data: dict[str, str | list[str]] = {
        "platform": board.platform or "",
        "framework": board.framework or "",
        "board_name": board.board_name,
        "platform_packages": sorted(board.platform_packages.split(","))
        if board.platform_packages
        else [],
        # Include PlatformIO version for full reproducibility
        "platformio_version": get_platformio_version(),
    }

    config_json = json.dumps(config_data, sort_keys=True)
    return hashlib.sha256(config_json.encode()).hexdigest()[:8]


if __name__ == "__main__":
    print("Note: Please use 'uv run ci/build_docker_image_pio.py' instead")
    print("This module provides utility functions only.")
    sys.exit(1)
