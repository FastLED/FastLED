"""
Test file discovery for FastLED test suite.

This module finds and reports all test files that should be built and executed.
It's designed to be called by the Meson build system during configuration.
"""

import sys
from pathlib import Path
from typing import List, Set


def discover_test_files(tests_dir: Path, excluded: Set[str]) -> List[str]:
    """
    Discover all test files in the tests directory.

    Args:
        tests_dir: Root directory containing test files
        excluded: Set of filenames to exclude from discovery

    Returns:
        List of relative paths (POSIX format) to test files, sorted
    """
    test_files: List[str] = []

    # Find all test_*.cpp files recursively
    for f in sorted(tests_dir.rglob("test_*.cpp")):
        if f.name not in excluded:
            rel_path = f.resolve().relative_to(tests_dir)
            test_files.append(rel_path.as_posix())

    # Also find *.cpp files directly in fl/, fx/ subdirectories
    for subdir in ["fl", "fx"]:
        subdir_path = tests_dir / subdir
        if subdir_path.exists():
            for f in sorted(subdir_path.glob("*.cpp")):
                rel_path = f.resolve().relative_to(tests_dir)
                test_files.append(rel_path.as_posix())

    return test_files


def main() -> None:
    """Main entry point for test discovery."""
    if len(sys.argv) != 2:
        print("Usage: discover_tests.py <tests_directory>", file=sys.stderr)
        sys.exit(1)

    tests_dir = Path(sys.argv[1]).resolve()

    if not tests_dir.exists():
        print(f"Error: Tests directory not found: {tests_dir}", file=sys.stderr)
        sys.exit(1)

    # Exclude special files that have their own build rules or aren't standard tests
    excluded: Set[str] = set()

    test_files = discover_test_files(tests_dir, excluded)

    # Output one file path per line
    for test_file in test_files:
        print(test_file)


if __name__ == "__main__":
    main()
