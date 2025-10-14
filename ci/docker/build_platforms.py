"""
Docker Platform to Boards Mapping

This module defines which boards belong to which Docker platform family.
Each Docker platform image (e.g., fastled-compiler-avr-uno) pre-caches
toolchains for ALL boards in its family, not just one representative board.

This is the single source of truth for platform→boards relationships used
during Docker image builds.
"""

from typing import Dict, List, Optional


# Platform families and their included boards
# Key = platform family name (used in Docker image names)
# Value = list of board names (from ci/boards.py) to pre-cache
DOCKER_PLATFORMS: Dict[str, List[str]] = {
    # AVR Platform - Classic and Modern AVR boards
    # Image: niteris/fastled-compiler-avr-uno:latest
    # Platforms: atmelavr (classic) + atmelmegaavr (modern)
    "avr": [
        "uno",
        "yun",
        "attiny85",
        "attiny88",
        "attiny4313",
        "nano_every",
        "ATtiny1604",
        "ATtiny1616",
    ],
    # ESP Platform - All Espressif chips (ESP32 + ESP8266)
    # Image: niteris/fastled-compiler-esp-esp32dev:latest
    # Platform: espressif32, espressif8266
    # IDF versions: Mixed (pioarduino provides latest)
    "esp": [
        "esp32dev",
        "esp32s3",
        "esp32c3",
        "esp32c2",
        "esp32c5",
        "esp32c6",
        "esp32h2",
        "esp32p4",
        "esp32s2",
        "esp8266",
    ],
    # Teensy Platform - All Teensy variants
    # Image: niteris/fastled-compiler-teensy-teensy41:latest
    # Platform: teensy
    # Despite differences (3.x = M4, 4.x = M7), toolchains are small enough
    # to cache all variants in one image
    "teensy": [
        "teensy41",  # Representative board (most capable, used in image name)
        "teensy40",
        "teensy31",
        "teensy30",
        "teensylc",
    ],
    # STM32 Platform - Various STM32 boards
    # Image: niteris/fastled-compiler-stm32-bluepill:latest
    # Platform: ststm32
    "stm32": [
        "bluepill",
        "blackpill",
        "maple_mini",
        "hy_tinystm103tb",
        "giga_r1",
    ],
    # RP2040 Platform - Raspberry Pi Pico family
    # Image: niteris/fastled-compiler-rp2040-rpipico:latest
    # Platform: raspberrypi (maxgerhardt fork)
    "rp2040": [
        "rpipico",
        "rpipico2",
    ],
    # NRF52 Platform - Nordic nRF52 boards
    # Image: niteris/fastled-compiler-nrf52-nrf52840_dk:latest
    # Platform: nordicnrf52
    "nrf52": [
        "nrf52840_dk",
        "adafruit_feather_nrf52840_sense",
        "xiaoblesense",
    ],
    # SAM Platform - Atmel SAM (ARM Cortex-M3)
    # Image: niteris/fastled-compiler-sam-due:latest
    # Platform: atmelsam
    "sam": [
        "due",
        "digix",
    ],
}

# Reverse mapping: board_name → platform_family
# Automatically generated from DOCKER_PLATFORMS
# Used to determine which platform family a board belongs to
BOARD_TO_PLATFORM: Dict[str, str] = {
    board: platform for platform, boards in DOCKER_PLATFORMS.items() for board in boards
}


def get_platform_for_board(board_name: str) -> Optional[str]:
    """
    Get the platform family name for a given board.

    Args:
        board_name: Board name (e.g., "uno", "esp32dev")

    Returns:
        Platform family name (e.g., "avr", "esp") or None if not found

    Example:
        >>> get_platform_for_board("uno")
        'avr'
        >>> get_platform_for_board("esp32s3")
        'esp'
    """
    return BOARD_TO_PLATFORM.get(board_name)


def get_boards_for_platform(platform: str) -> List[str]:
    """
    Get all board names for a given platform family.

    Args:
        platform: Platform family name (e.g., "avr", "esp")

    Returns:
        List of board names, or empty list if platform not found

    Example:
        >>> get_boards_for_platform("avr")
        ['uno', 'yun', 'attiny85', ...]
    """
    return DOCKER_PLATFORMS.get(platform, [])


def get_docker_image_name(platform: str, board: Optional[str] = None) -> str:
    """
    Get the Docker image name for a platform family.

    Uses consistent naming: niteris/fastled-compiler-{platform}-{board}
    For platforms with a single representative board, use first board in list.

    Args:
        platform: Platform family name (e.g., "avr", "esp")
        board: Representative board name (optional, inferred if not provided)

    Returns:
        Full Docker image name without tag

    Example:
        >>> get_docker_image_name("avr", "uno")
        'niteris/fastled-compiler-avr-uno'
        >>> get_docker_image_name("esp", "esp32dev")
        'niteris/fastled-compiler-esp-esp32dev'
        >>> get_docker_image_name("teensy", "teensy41")
        'niteris/fastled-compiler-teensy-teensy41'
    """
    # Use provided board or first board in platform
    if board is None:
        boards = get_boards_for_platform(platform)
        if not boards:
            return f"niteris/fastled-compiler-{platform}"
        board = boards[0]

    # Consistent naming for all platforms: niteris/fastled-compiler-{platform}-{board}
    return f"niteris/fastled-compiler-{platform}-{board}"


# Validation: Ensure no duplicate boards across platforms
_all_boards = [board for boards in DOCKER_PLATFORMS.values() for board in boards]
_duplicates = [board for board in _all_boards if _all_boards.count(board) > 1]
if _duplicates:
    raise ValueError(f"Duplicate boards found across platforms: {set(_duplicates)}")
