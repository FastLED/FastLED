"""
Collect all FastLED source files for Meson build system.

This script discovers all C++ source files according to the configuration
in source_config.py and outputs them one per line for Meson to parse.
"""

import sys
from pathlib import Path

from discover_sources import discover_sources
from source_config import SOURCE_DIRECTORIES


def main() -> None:
    """Main entry point for source collection."""
    if len(sys.argv) != 1:
        print("Usage: collect_sources.py", file=sys.stderr)
        print("Outputs all source files defined in source_config.py", file=sys.stderr)
        sys.exit(1)

    all_sources = []

    # Discover sources from each configured directory
    for dir_path, recursive in SOURCE_DIRECTORIES:
        sources = discover_sources(dir_path, "*.cpp", recursive)
        all_sources.extend(sources)

    # Output one file path per line
    for source in all_sources:
        print(source)


if __name__ == "__main__":
    main()
