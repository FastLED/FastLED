#!/usr/bin/env python3
"""
Recursive glob utility for Meson build system.

This script performs platform-agnostic recursive file pattern matching,
replacing inline Python scripts in meson.build files.

Usage:
    python rglob.py <directory> <pattern> [exclude_path]

Arguments:
    directory: Root directory to search from
    pattern: Glob pattern to match (e.g., '*.cpp', '*.h')
    exclude_path: Optional path substring to exclude from results

Output:
    One file path per line (using forward slashes for cross-platform compatibility)
"""

import os
import pathlib
import sys


def main() -> None:
    if len(sys.argv) < 3:
        print(
            f"Usage: {sys.argv[0]} <directory> <pattern> [exclude_path]",
            file=sys.stderr,
        )
        sys.exit(1)

    directory = sys.argv[1]
    pattern = sys.argv[2]
    exclude_path = sys.argv[3] if len(sys.argv) > 3 else None

    # Validate directory exists
    dir_path = pathlib.Path(directory)
    if not dir_path.exists():
        print(f"Error: Directory '{directory}' does not exist", file=sys.stderr)
        sys.exit(1)

    # Perform recursive glob
    matches = dir_path.rglob(pattern)

    # Filter and output results
    for p in matches:
        # Normalize path for comparison (use forward slashes)
        path_str = str(p).replace(os.sep, "/")
        if exclude_path is None or exclude_path not in path_str:
            print(str(p))


if __name__ == "__main__":
    main()
