from ci.util.global_interrupt_handler import handle_keyboard_interrupt


"""Docker image build helpers.

After the compilation-Docker decommission (#2812) this module exists only
to support the qemu emulator runner (`ci/docker_utils/qemu_esp32_docker.py`,
which imports `generate_config_hash`). The compile-Docker-specific helpers
(`extract_architecture`, `generate_docker_tag`, the
`niteris/fastled-compiler-*` image-name generators) were removed along with
the rest of the compiler-image family.
"""

import hashlib
import json
import sys
from typing import Optional

from running_process import RunningProcess

from ci.boards import create_board


def get_platformio_version() -> str:
    """Get the installed PlatformIO version.

    Returns:
        Version string (e.g., '6.1.15')
    """
    try:
        result = RunningProcess.run(
            ["pio", "--version"],
            cwd=None,
            check=False,
            timeout=10,
        )
        # Output format: "PlatformIO Core, version 6.1.15"
        version_line = result.stdout.strip()
        if "version" in version_line:
            return version_line.split("version")[-1].strip()
        return version_line
    except (RuntimeError, FileNotFoundError):
        # If PlatformIO is not installed, return a default version
        return "unknown"


def generate_config_hash(board_name: str, framework: Optional[str] = None) -> str:
    """Generate deterministic hash from board configuration.

    Used by `qemu_esp32_docker.py` for image-name derivation. The hash is
    based on:
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
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
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
    print("Note: this module provides utility functions only.")
    sys.exit(1)
