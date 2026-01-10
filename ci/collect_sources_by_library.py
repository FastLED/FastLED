"""
Collect FastLED source files organized by library.

This script discovers C++ source files and organizes them by library/directory.

Output format: LIBRARY:<library_name>:<file_path>
Example: LIBRARY:fl:src/fl/algorithm.cpp
"""

import sys

from discover_sources import discover_sources


# Source directories organized by library
SOURCE_LIBRARIES: dict[str, list[tuple[str, bool]]] = {
    # Core FastLED library - base functionality (src/*.cpp, src/lib8tion)
    "core": [
        ("src/lib8tion", False),  # Math utilities
    ],
    # FL stdlib-like utilities (src/fl/**)
    "fl": [
        ("src/fl", True),  # FastLED stdlib utilities (recursive)
    ],
    # Effects framework (src/fx/**)
    "fx": [
        ("src/fx", True),  # Effects framework (recursive)
    ],
    # Platform-specific code (src/platforms/**)
    "platforms": [
        ("src/platforms/stub", False),  # Stub platform implementations
        ("src/platforms", False),  # Platform-specific code (top-level only)
        ("src/platforms/shared", True),  # Shared platform code (recursive)
        ("src/platforms/esp", True),  # ESP32/ESP8266 specific code (recursive)
        (
            "src/platforms/arm",
            True,
        ),  # ARM-based platforms (RP2040, Teensy, etc.) (recursive)
    ],
    # Third-party libraries (src/third_party/**)
    "thirdparty": [
        ("src/third_party", True),  # Third-party libraries (recursive)
    ],
    # Sensor support (src/fl/sensors)
    "sensors": [
        ("src/fl/sensors", False),  # Sensor support
    ],
}


def main() -> None:
    """Main entry point for library-organized source collection."""
    if len(sys.argv) != 1:
        print("Usage: collect_sources_by_library.py", file=sys.stderr)
        print("Outputs sources organized by library", file=sys.stderr)
        sys.exit(1)

    # Discover sources for each library
    for library_name, directories in SOURCE_LIBRARIES.items():
        for dir_path, recursive in directories:
            sources = discover_sources(dir_path, "*.cpp", recursive)
            for source in sources:
                # Output in format: LIBRARY:<library_name>:<file_path>
                print(f"LIBRARY:{library_name}:{source}")


if __name__ == "__main__":
    main()
