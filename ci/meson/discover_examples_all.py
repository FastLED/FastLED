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

import sys
from pathlib import Path


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
                        platforms = line.split("// @filter:")[1].strip()
                        if "STUB" not in platforms.upper():
                            print(
                                f"SKIP|{example_name}|Platform-specific (@filter:{platforms})"
                            )
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
