"""
Source directory configuration for FastLED Meson build system.

This module defines which directories contain C++ source files that should
be compiled into libfastled.a. Separating configuration from build logic
makes it easier to add/remove source directories.
"""

# Source directories to scan for C++ files
# Format: (directory_path, recursive)
# - directory_path: Path relative to project root
# - recursive: True to use rglob (search subdirectories), False to use glob (direct children only)
SOURCE_DIRECTORIES: list[tuple[str, bool]] = [
    ("src/lib8tion", False),  # lib8tion utilities
    ("src/platforms/stub", False),  # Stub platform implementations
    ("src/platforms", False),  # Platform-specific code (top-level only)
    ("src/fl", True),  # FastLED stdlib-like utilities (recursive)
    ("src/platforms/shared", True),  # Shared platform code (recursive)
    ("src/platforms/esp", True),  # ESP32/ESP8266 specific code (recursive)
    (
        "src/platforms/arm",
        True,
    ),  # ARM-based platforms (RP2040, Teensy, etc.) (recursive)
    ("src/third_party", True),  # Third-party libraries (recursive)
    ("src/fl/codec", False),  # Codec implementations
    ("src/fx", True),  # Effects framework (recursive)
    ("src/sensors", False),  # Sensor support
]
