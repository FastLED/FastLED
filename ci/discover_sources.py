"""
Source file discovery for FastLED Meson build system.

This module discovers C++ source files in specified directories and outputs
them in a format that Meson can parse. It's designed to replace inline Python
scripts that were embedded in meson.build.
"""

import sys
from pathlib import Path


def discover_sources(
    base_dir: str, pattern: str = "*.cpp", recursive: bool = False
) -> list[str]:
    """
    Discover source files in a directory.

    Args:
        base_dir: Directory to search in (relative to project root)
        pattern: Glob pattern for files to match (default: "*.cpp")
        recursive: If True, search recursively (rglob), else search only direct children (glob)

    Returns:
        List of file paths (as strings) sorted alphabetically
    """
    base_path = Path(base_dir)

    if not base_path.exists():
        return []

    if recursive:
        files = sorted(base_path.rglob(pattern))
    else:
        files = sorted(base_path.glob(pattern))

    return [str(f) for f in files]


def main() -> None:
    """Main entry point for source discovery."""
    if len(sys.argv) < 2:
        print(
            "Usage: discover_sources.py <base_dir> [pattern] [recursive]",
            file=sys.stderr,
        )
        sys.exit(1)

    base_dir = sys.argv[1]
    pattern = sys.argv[2] if len(sys.argv) > 2 else "*.cpp"
    recursive = sys.argv[3].lower() == "true" if len(sys.argv) > 3 else False

    sources = discover_sources(base_dir, pattern, recursive)

    # Output one file path per line
    for source in sources:
        print(source)


if __name__ == "__main__":
    main()
