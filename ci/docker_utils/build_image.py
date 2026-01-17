from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""Docker image build utilities for FastLED PlatformIO compilation.

This module provides functions for building Docker images with pre-cached
PlatformIO dependencies for faster cross-platform compilation.
"""

import hashlib
import json
import re
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
    """Extract architecture family from board configuration.

    Maps board names to their architecture family for Docker image naming.
    This enables grouping related boards under a common architecture namespace.

    Args:
        board_name: Name of the board (e.g., 'uno', 'esp32s3', 'esp8266')

    Returns:
        Architecture name (e.g., 'avr', 'esp', 'teensy', 'stm32')

    Examples:
        >>> extract_architecture('uno')
        'avr'
        >>> extract_architecture('esp32s3')
        'esp'
        >>> extract_architecture('teensy41')
        'teensy'
    """
    try:
        board = create_board(board_name, no_project_options=True)
        platform = board.platform or ""

        # Map platform to architecture
        arch_map = {
            "atmelavr": "avr",
            "atmelmegaavr": "avr",
            "atmelsam": "arm",
            "espressif32": "esp",
            "espressif8266": "esp8266",
            "teensy": "teensy",
            "ststm32": "stm32",
            "nordicnrf52": "nrf52",
            "renesas-ra": "renesas",
            "raspberrypi": "rp2040",
            "native": "native",
        }

        # Check if platform is a URL (e.g., ESP32 boards with custom platform packages)
        if platform.startswith("http"):
            if "espressif32" in platform:
                return "esp"
            elif "espressif8266" in platform:
                return "esp8266"
            elif "raspberrypi" in platform:
                return "rp2040"
            elif "nordicnrf52" in platform:
                return "nrf52"
            elif "apollo3" in platform:
                return "apollo3"
            elif "ststm32" in platform:
                return "stm32"
            elif "siliconlabsefm32" in platform:
                return "silabs"

        # Direct platform name lookup
        for key, arch in arch_map.items():
            if key in platform.lower():
                return arch

        # Fallback: use sanitized platform name
        platform_name = platform.split("/")[-1] if platform else "unknown"
        # Remove common suffixes
        platform_name = platform_name.replace(".git", "").replace(".zip", "")
        return platform_name

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        # Fallback if board creation fails - use board name
        return board_name


def generate_docker_tag(board_name: str) -> str:
    """Generate semantic Docker tag from board configuration.

    Extracts meaningful version information from the board's platform configuration
    to create semantic tags like 'idf5.4', 'idf5.3', or 'latest'.

    Args:
        board_name: Name of the board (e.g., 'esp32s3', 'uno')

    Returns:
        Docker tag string (e.g., 'idf5.4', 'latest', 'stable')

    Examples:
        >>> generate_docker_tag('esp32s3')  # Uses IDF 5.4
        'idf5.4'
        >>> generate_docker_tag('esp32dev')  # Uses IDF 5.3
        'idf5.3'
        >>> generate_docker_tag('uno')
        'latest'
    """
    try:
        board = create_board(board_name, no_project_options=True)
        platform = board.platform or ""

        # ESP32 boards: Extract IDF version from URL
        if "espressif32" in platform:
            # Check for stable release
            if "stable" in platform.lower():
                return "stable"

            # Extract version from URL patterns like:
            # releases/download/53.03.10/ -> idf5.3
            # releases/download/54.03.20/ -> idf5.4
            # releases/download/55.03.30-2/ -> idf5.5
            version_match = re.search(r"/(\d{2})\.(\d{2})\.(\d{2})", platform)
            if version_match:
                major = version_match.group(1)
                # Convert 53 -> 5.3, 54 -> 5.4, etc.
                idf_major = major[0]
                idf_minor = major[1]
                return f"idf{idf_major}.{idf_minor}"

            # Check for git branches
            if "#develop" in platform:
                return "develop"

            # Fallback for ESP32
            return "latest"

        # ESP8266 boards
        if "espressif8266" in platform or "esp8266" in platform.lower():
            return "latest"

        # All other platforms use :latest
        # Future: Can extract GCC version or other toolchain info
        return "latest"

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        # Fallback if board creation fails
        return "latest"


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
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
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
        "platform_packages": (
            sorted(board.platform_packages.split(","))
            if board.platform_packages
            else []
        ),
        # Include PlatformIO version for full reproducibility
        "platformio_version": get_platformio_version(),
    }

    config_json = json.dumps(config_data, sort_keys=True)
    return hashlib.sha256(config_json.encode()).hexdigest()[:8]


if __name__ == "__main__":
    print("Note: Please use 'uv run ci/build_docker_image_pio.py' instead")
    print("This module provides utility functions only.")
    sys.exit(1)
