#!/usr/bin/env python3
"""
Discover additional source files for Arduino examples.

This script scans example directories to find .cpp files that should be
compiled along with the main .ino file.

Output format: SOURCES:<example_name>:<source1>:<source2>:...
Each line represents one example with its additional source files (relative to project root).

Usage:
    python ci/discover_example_sources.py <examples_dir>
"""

import sys
from pathlib import Path


def find_sources_for_example(ino_file: Path, project_root: Path) -> list[str]:
    """
    Find all .cpp files that should be compiled for the given example.
    This includes:
    1. .cpp files in the example's root directory (same dir as .ino)
    2. .cpp files in subdirectories (e.g., src/, lib/, etc.)

    Args:
        ino_file: Path to the .ino file
        project_root: Project root directory for relative path calculation

    Returns:
        List of .cpp file paths (relative to project root) that should be compiled
    """
    example_dir = ino_file.parent
    source_files: list[str] = []

    # Excluded directory names
    excluded_dirs = {".git", "__pycache__", ".pio", ".vscode", "fastled_js", "build"}

    # 1. Find .cpp files in the example root directory (same level as .ino)
    for cpp_file in example_dir.glob("*.cpp"):
        if cpp_file.is_file():
            try:
                rel_path = cpp_file.relative_to(project_root)
                source_files.append(str(rel_path))
            except ValueError:
                source_files.append(str(cpp_file))

    # 2. Find all .cpp files in subdirectories
    for subdir in example_dir.iterdir():
        if (
            subdir.is_dir()
            and subdir.name not in excluded_dirs
            and not subdir.name.startswith(".")
        ):
            # Find all .cpp files recursively in this subdirectory
            cpp_files = list(subdir.rglob("*.cpp"))
            for cpp_file in cpp_files:
                try:
                    rel_path = cpp_file.relative_to(project_root)
                    source_files.append(str(rel_path))
                except ValueError:
                    source_files.append(str(cpp_file))

    return source_files


def main() -> int:
    """Main entry point."""
    if len(sys.argv) != 2:
        print(
            "Usage: python ci/discover_example_sources.py <examples_dir>",
            file=sys.stderr,
        )
        return 1

    examples_dir = Path(sys.argv[1]).resolve()
    if not examples_dir.exists():
        print(f"Error: Examples directory not found: {examples_dir}", file=sys.stderr)
        return 1

    # Find project root (parent of examples dir)
    project_root = examples_dir.parent

    # Find all .ino files
    ino_files = sorted(examples_dir.rglob("*.ino"))

    for ino_file in ino_files:
        example_name = ino_file.stem
        source_files = find_sources_for_example(ino_file, project_root)

        # Output format: SOURCES:<example_name>:<source1>:<source2>:...
        # If no additional sources, output only the example name
        if source_files:
            print(f"SOURCES:{example_name}:{':'.join(source_files)}")
        else:
            print(f"SOURCES:{example_name}:")

    return 0


if __name__ == "__main__":
    sys.exit(main())
