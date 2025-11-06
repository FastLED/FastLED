#!/usr/bin/env python3
"""
Discover include directories for Arduino examples.

This script scans example directories to find subdirectories that contain
header files (.h/.hpp), which should be added as include paths for compilation.

Output format: EXAMPLE:<name>:<include_dir1>:<include_dir2>:...
Each line represents one example with its include directories (relative to project root).

Usage:
    python ci/discover_example_includes.py <examples_dir>
"""

import sys
from pathlib import Path


def find_include_dirs_for_example(ino_file: Path, project_root: Path) -> list[str]:
    """
    Find all directories that should be added as include paths for the given example.
    This includes the example root directory and any subdirectories that contain header files.

    Args:
        ino_file: Path to the .ino file
        project_root: Project root directory for relative path calculation

    Returns:
        List of directory paths (relative to project root) that should be added as -I flags
    """
    example_dir = ino_file.parent
    include_dirs: list[str] = []

    # Always include the example root directory (relative to project root)
    try:
        rel_path = example_dir.relative_to(project_root)
        include_dirs.append(str(rel_path))
    except ValueError:
        # If example_dir is not relative to project_root, use absolute path
        include_dirs.append(str(example_dir))

    # Excluded directory names
    excluded_dirs = {".git", "__pycache__", ".pio", ".vscode", "fastled_js", "build"}

    # Check for header files in subdirectories
    for subdir in example_dir.iterdir():
        if (
            subdir.is_dir()
            and subdir.name not in excluded_dirs
            and not subdir.name.startswith(".")
        ):
            # Check if this directory contains header files
            header_files = [
                f
                for f in subdir.rglob("*")
                if f.is_file() and f.suffix in [".h", ".hpp"]
            ]
            if header_files:
                try:
                    rel_path = subdir.relative_to(project_root)
                    include_dirs.append(str(rel_path))
                except ValueError:
                    include_dirs.append(str(subdir))

    return include_dirs


def main() -> int:
    """Main entry point."""
    if len(sys.argv) != 2:
        print(
            "Usage: python ci/discover_example_includes.py <examples_dir>",
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
        include_dirs = find_include_dirs_for_example(ino_file, project_root)

        # Output format: EXAMPLE:<name>:<include_dir1>:<include_dir2>:...
        print(f"EXAMPLE:{example_name}:{':'.join(include_dirs)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
