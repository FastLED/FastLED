#!/usr/bin/env python3
"""
Consolidated example discovery for Meson build system.

This script combines discover_example_includes.py, discover_example_sources.py,
and filter_examples_for_stub.py into a single pass over the examples directory.

Output format (one per line, using | as delimiter to avoid Windows path issues):
  EXAMPLE|<name>|<include_dir1>|<include_dir2>|...
  SOURCES|<name>|<source1>|<source2>|...
  SKIP|<name>|<reason>

Usage:
    python discover_examples_all.py <examples_dir>
"""

import os
import re
import sys
from pathlib import Path


def should_skip_for_stub(filter_str: str) -> tuple[bool, str]:
    """
    Evaluate filter expression for STUB platform (host compilation).

    Filter expressions can be:
    - Simple platform list: "STUB" or "ESP32" or "TEENSY"
    - Expression: "(platform is native)" or "(memory is high)"

    For STUB builds (Mac/Linux/Windows host compilation):
    - "STUB" (case-insensitive) -> include
    - "(platform is native)" -> include (native = STUB/host)
    - Other platforms -> skip

    Args:
        filter_str: Filter string from @filter: annotation

    Returns:
        Tuple of (should_skip, reason)
    """
    # Check if filters are disabled via environment variable
    if os.environ.get("FASTLED_IGNORE_EXAMPLE_FILTERS") == "1":
        return False, ""

    # Normalize for comparison
    filter_upper = filter_str.upper()

    # Simple check: if "STUB" appears anywhere, include it
    if "STUB" in filter_upper:
        return False, ""

    # Expression-based filter: "(platform is native)" or similar
    # Pattern: (platform is <value>) or (memory is <value>)
    platform_match = re.search(
        r"\(\s*platform\s+is\s+(\w+)\s*\)", filter_str, re.IGNORECASE
    )
    if platform_match:
        platform_value = platform_match.group(1).lower()
        # "native" means host/STUB build (Mac/Linux/Windows)
        if platform_value == "native":
            return False, ""
        # Other specific platforms (esp32, teensy, etc.) should be skipped for STUB
        return True, f"Platform-specific (@filter:{filter_str})"

    # Memory filters don't exclude STUB builds
    if re.search(r"\(\s*memory\s+is\s+\w+\s*\)", filter_str, re.IGNORECASE):
        return False, ""

    # Board filters don't exclude STUB builds
    if re.search(r"\(\s*board\s+is\s+(not\s+)?\w+\s*\)", filter_str, re.IGNORECASE):
        return False, ""

    # Default: if we don't recognize the filter and it doesn't mention STUB, skip it
    return True, f"Platform-specific (@filter:{filter_str})"


def discover_examples_all(examples_dir: Path) -> None:
    """
    Discover all examples with includes, sources, and filter annotations.

    Args:
        examples_dir: Root examples directory
    """
    # Convert examples_dir to absolute path for consistency
    examples_dir = examples_dir.resolve()

    # Find all .ino files
    ino_files = list(examples_dir.rglob("*.ino"))

    for ino_file in sorted(ino_files):
        # Example name is the filename without extension
        example_name = ino_file.stem

        # Example root directory (parent of .ino file)
        example_root = ino_file.parent

        # Collect include directories as relative paths (relative to examples_dir)
        # Start with the example root directory
        include_dirs = [
            str(example_root.relative_to(examples_dir.parent)).replace("\\", "/")
        ]

        # Check for subdirectories that should be included
        # Common patterns: src/, lib/, include/
        for subdir_name in ["src", "lib", "include"]:
            subdir = example_root / subdir_name
            if subdir.exists() and subdir.is_dir():
                include_dirs.append(
                    str(subdir.relative_to(examples_dir.parent)).replace("\\", "/")
                )

        # Output EXAMPLE line with include directories (use | delimiter)
        print(f"EXAMPLE|{example_name}|{'|'.join(include_dirs)}")

        # Collect additional .cpp source files as relative paths
        # Look in src/ and lib/ subdirectories
        cpp_sources: list[str] = []
        for subdir_name in ["src", "lib"]:
            subdir = example_root / subdir_name
            if subdir.exists() and subdir.is_dir():
                for cpp_file in subdir.rglob("*.cpp"):
                    cpp_sources.append(
                        str(cpp_file.relative_to(examples_dir.parent)).replace(
                            "\\", "/"
                        )
                    )

        # Output SOURCES line (even if empty - consistent format, use | delimiter)
        sources_str: str = "|".join(cpp_sources) if cpp_sources else ""
        print(f"SOURCES|{example_name}|{sources_str}")

        # Check for @filter annotation in .ino file
        # @filter:<platform_list> or @filter-out:<platform> indicates platform-specific
        try:
            content = ino_file.read_text(encoding="utf-8")

            # Check for @filter annotation
            if "@filter:" in content or "@filter-out:" in content:
                # Parse the filter line to extract platforms
                for line in content.split("\n"):
                    line = line.strip()
                    if line.startswith("// @filter:"):
                        filter_str = line.split("// @filter:")[1].strip()
                        should_skip, reason = should_skip_for_stub(filter_str)
                        if should_skip:
                            print(f"SKIP|{example_name}|{reason}")
                        break
                    elif line.startswith("// @filter-out:"):
                        platforms = line.split("// @filter-out:")[1].strip()
                        if "STUB" in platforms.upper():
                            print(
                                f"SKIP|{example_name}|Filtered out for STUB (@filter-out:{platforms})"
                            )
                        break
        except (UnicodeDecodeError, OSError):
            # If we can't read the file, skip silently
            pass


def main() -> None:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <examples_dir>", file=sys.stderr)
        sys.exit(1)

    examples_dir = Path(sys.argv[1])
    if not examples_dir.exists():
        print(f"Error: Directory '{examples_dir}' does not exist", file=sys.stderr)
        sys.exit(1)

    discover_examples_all(examples_dir)


if __name__ == "__main__":
    main()
