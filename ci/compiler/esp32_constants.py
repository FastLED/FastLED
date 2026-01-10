"""ESP32 constants for compilation and QEMU support.

This module centralizes all ESP32-related constants including flash offsets,
file names, and default configurations.
"""

from dataclasses import dataclass


@dataclass(frozen=True)
class ESP32FlashOffsets:
    """ESP32 flash memory offsets for various components."""

    BOOTLOADER_CLASSIC: int = 0x1000  # ESP32 classic
    BOOTLOADER_MODERN: int = 0x0  # ESP32-C3, ESP32-S3, and newer
    PARTITIONS: int = 0x8000
    BOOT_APP0: int = 0xE000
    FIRMWARE: int = 0x10000


@dataclass(frozen=True)
class ESP32FileNames:
    """Standard file names for ESP32 build artifacts."""

    FIRMWARE: str = "firmware.bin"
    BOOTLOADER: str = "bootloader.bin"
    PARTITIONS_BIN: str = "partitions.bin"
    PARTITIONS_CSV: str = "partitions.csv"
    BOOT_APP0: str = "boot_app0.bin"
    MERGED: str = "merged.bin"
    FLASH: str = "flash.bin"
    SPIFFS: str = "spiffs.bin"


# Singleton instances for easy access
FLASH_OFFSETS = ESP32FlashOffsets()
FILE_NAMES = ESP32FileNames()

# Valid flash sizes in MB
VALID_FLASH_SIZES: set[int] = {2, 4, 8, 16}

# Default boot_app0.bin size
BOOT_APP0_SIZE: int = 8192

# Default partitions CSV for QEMU
DEFAULT_PARTITIONS_CSV: str = """# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
otadata,  data, ota,     0xf000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
spiffs,   data, spiffs,  0x150000,0xb0000,
"""


def get_bootloader_offset(board_name: str) -> int:
    """Get the bootloader offset for a given ESP32 board.

    Args:
        board_name: Name of the board (e.g., "esp32dev", "esp32c3", "esp32s3")

    Returns:
        Bootloader offset in bytes
    """
    # ESP32 classic uses 0x1000, newer chips use 0x0
    modern_chips = ["c3", "s3", "c2", "c5", "c6", "h2", "p4"]

    if board_name.startswith("esp32"):
        for chip in modern_chips:
            if board_name.startswith(f"esp32{chip}"):
                return FLASH_OFFSETS.BOOTLOADER_MODERN

    return FLASH_OFFSETS.BOOTLOADER_CLASSIC


def is_esp32_board(board_name: str) -> bool:
    """Check if a board is an ESP32 variant.

    Args:
        board_name: Name of the board

    Returns:
        True if the board is an ESP32 variant
    """
    return board_name.startswith("esp32") or board_name.startswith("esp8266")


def get_all_esp32_file_names() -> list[str]:
    """Get a list of all ESP32 artifact file names.

    Returns:
        List of file names
    """
    return [
        FILE_NAMES.FIRMWARE,
        FILE_NAMES.BOOTLOADER,
        FILE_NAMES.PARTITIONS_BIN,
        FILE_NAMES.PARTITIONS_CSV,
        FILE_NAMES.BOOT_APP0,
        FILE_NAMES.MERGED,
        FILE_NAMES.FLASH,
        FILE_NAMES.SPIFFS,
    ]
