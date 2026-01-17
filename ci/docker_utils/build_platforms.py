"""
Docker Platform to Boards Mapping

This module defines which boards belong to which Docker platform family.
Each Docker platform image (e.g., niteris/fastled-compiler-avr) pre-caches
toolchains for ALL boards in its family.

This is the single source of truth for platform→boards relationships used
during Docker image builds.
"""

from typing import Optional


# Platform families and their included boards
# Key = platform family name (used in Docker image names)
# Value = list of board names (from ci/boards.py) to pre-cache
DOCKER_PLATFORMS: dict[str, list[str]] = {
    # AVR Platform - Classic and Modern AVR boards
    # Image: niteris/fastled-compiler-avr:latest
    # Platforms: atmelavr (classic) + atmelmegaavr (modern)
    "avr": [
        "uno",
        "leonardo",
        "attiny85",
        "attiny88",
        "attiny4313",
        "nano_every",
        "attiny1616",
    ],
    # ESP32 Platforms - Individual images per board (flat structure)
    # These replace the previous grouped esp-riscv and esp-xtensa images
    # to prevent build artifact accumulation issues
    # ESP32 Original (Xtensa)
    # Image: niteris/fastled-compiler-esp-32dev:latest
    "esp-32dev": ["esp32dev"],
    # ESP32-S2 (Xtensa)
    # Image: niteris/fastled-compiler-esp-32s2:latest
    "esp-32s2": ["esp32s2"],
    # ESP32-S3 (Xtensa)
    # Image: niteris/fastled-compiler-esp-32s3:latest
    "esp-32s3": ["esp32s3"],
    # ESP8266 (Xtensa)
    # Image: niteris/fastled-compiler-esp-8266:latest
    "esp-8266": ["esp8266"],
    # ESP32-C2 (RISC-V)
    # Image: niteris/fastled-compiler-esp-32c2:latest
    "esp-32c2": ["esp32c2"],
    # ESP32-C3 (RISC-V)
    # Image: niteris/fastled-compiler-esp-32c3:latest
    "esp-32c3": ["esp32c3"],
    # ESP32-C5 (RISC-V)
    # Image: niteris/fastled-compiler-esp-32c5:latest
    "esp-32c5": ["esp32c5"],
    # ESP32-C6 (RISC-V)
    # Image: niteris/fastled-compiler-esp-32c6:latest
    "esp-32c6": ["esp32c6"],
    # ESP32-H2 (RISC-V)
    # Image: niteris/fastled-compiler-esp-32h2:latest
    "esp-32h2": ["esp32h2"],
    # ESP32-P4 (RISC-V)
    # Image: niteris/fastled-compiler-esp-32p4:latest
    "esp-32p4": ["esp32p4"],
    # Teensy Platform - All Teensy variants
    # Image: niteris/fastled-compiler-teensy:latest
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
    # STM32 Platforms - Individual images per board (flat structure)
    # These replace the previous grouped stm32 image
    # to prevent build artifact accumulation issues
    # STM32F103C8 Blue Pill
    # Image: niteris/fastled-compiler-stm32-f103c8:latest
    "stm32-f103c8": ["stm32f103c8"],
    # STM32F411CE Black Pill
    # Image: niteris/fastled-compiler-stm32-f411ce:latest
    "stm32-f411ce": ["stm32f411ce"],
    # STM32F103CB Maple Mini
    # Image: niteris/fastled-compiler-stm32-f103cb:latest
    "stm32-f103cb": ["stm32f103cb"],
    # STM32F103TB Tiny STM
    # Image: niteris/fastled-compiler-stm32-f103tb:latest
    "stm32-f103tb": ["stm32f103tb"],
    # STM32H747XI Arduino Giga R1
    # Image: niteris/fastled-compiler-stm32-h747xi:latest
    "stm32-h747xi": ["stm32h747xi"],
    # RP Platform - Raspberry Pi Pico family
    # Image: niteris/fastled-compiler-rp:latest
    # Platforms: raspberrypi (maxgerhardt fork)
    "rp": [
        "rp2040",
        "rp2350",
        "rp2350B",
    ],
    # NRF52 Platform - Nordic nRF52 boards
    # Image: niteris/fastled-compiler-nrf52:latest
    # Platform: nordicnrf52
    "nrf52": [
        "nrf52840_dk",
        "adafruit_feather_nrf52840_sense",
        "xiaoblesense",
    ],
    # SAM Platforms - Individual images per board (flat structure)
    # These replace the previous grouped sam image
    # to prevent build artifact accumulation issues
    # Atmel SAM3X8E Due
    # Image: niteris/fastled-compiler-sam-3x8e:latest
    "sam-3x8e": ["sam3x8e_due"],
}

# Reverse mapping: board_name → platform_family
# Automatically generated from DOCKER_PLATFORMS
# Used to determine which platform family a board belongs to
BOARD_TO_PLATFORM: dict[str, str] = {
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


def get_boards_for_platform(platform: str) -> list[str]:
    """
    Get all board names for a given platform family.

    Args:
        platform: Platform family name (e.g., "avr", "esp")

    Returns:
        List of board names, or empty list if platform not found

    Example:
        >>> get_boards_for_platform("avr")
        ['uno', 'atmega32u4_leonardo', 'attiny85', ...]
    """
    return DOCKER_PLATFORMS.get(platform, [])


def get_docker_image_name(platform: str, board: Optional[str] = None) -> str:
    """
    Get the Docker image name for a platform family.

    All platform images use the naming convention: niteris/fastled-compiler-{platform}
    without board suffixes, as each platform image contains pre-cached toolchains
    for ALL boards in that platform family.

    Args:
        platform: Platform family name (e.g., "avr", "esp", "rp")
        board: Reserved for future use (currently ignored)

    Returns:
        Full Docker image name without tag

    Example:
        >>> get_docker_image_name("avr")
        'niteris/fastled-compiler-avr'
        >>> get_docker_image_name("esp")
        'niteris/fastled-compiler-esp'
        >>> get_docker_image_name("rp")
        'niteris/fastled-compiler-rp'
    """
    # All platform images use consistent naming without board suffix
    return f"niteris/fastled-compiler-{platform}"


# Validation: Ensure no duplicate boards across platforms
_all_boards = [board for boards in DOCKER_PLATFORMS.values() for board in boards]
_duplicates = [board for board in _all_boards if _all_boards.count(board) > 1]
if _duplicates:
    raise ValueError(f"Duplicate boards found across platforms: {set(_duplicates)}")
