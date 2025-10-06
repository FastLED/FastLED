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
    """Extract architecture name from board platform.

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32s3')

    Returns:
        Architecture name for Docker image naming (e.g., 'avr', 'esp32')

    Raises:
        ValueError: If board not found or platform cannot be determined
    """
    try:
        board = create_board(board_name, no_project_options=True)
    except Exception as e:
        raise ValueError(f"Failed to create board '{board_name}': {e}") from e

    platform = board.platform
    if not platform:
        raise ValueError(f"Board '{board_name}' has no platform defined")

    # Map platform URLs/names to architecture names
    platform_lower = platform.lower()

    # Handle GitHub URLs and package names
    if "atmelavr" in platform_lower or "platform-atmelavr" in platform_lower:
        return "avr"
    elif "atmelsam" in platform_lower or "platform-atmelsam" in platform_lower:
        return "sam"
    elif "espressif32" in platform_lower or "platform-espressif32" in platform_lower:
        return "esp32"
    elif (
        "espressif8266" in platform_lower or "platform-espressif8266" in platform_lower
    ):
        return "esp8266"
    elif "ststm32" in platform_lower or "platform-ststm32" in platform_lower:
        return "stm32"
    elif "teensy" in platform_lower or "platform-teensy" in platform_lower:
        return "teensy"
    elif "nordicnrf52" in platform_lower or "platform-nordicnrf52" in platform_lower:
        return "nrf52"
    elif "raspberrypi" in platform_lower or "platform-raspberrypi" in platform_lower:
        return "rp2040"
    elif "renesas" in platform_lower or "platform-renesas" in platform_lower:
        return "renesas"
    elif "native" in platform_lower:
        return "native"
    elif (
        "siliconlabsefm32" in platform_lower
        or "platform-siliconlabsefm32" in platform_lower
    ):
        return "efm32"
    else:
        # Fallback: try to extract from platform name
        # Handle cases like "platform/native" -> "native"
        parts = platform.split("/")
        if len(parts) > 1:
            return parts[-1].replace("platform-", "")
        return platform.replace("platform-", "").replace("platformio/", "")


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
