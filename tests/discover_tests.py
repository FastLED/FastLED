"""
Test file discovery for FastLED test suite.

This module finds and reports all test files that should be built and executed.
It's designed to be called by the Meson build system during configuration.

Discovery is fully automatic: every *.cpp / *.ino file under tests/ is a
test, except for files in EXCLUDED_TEST_FILES and directories in
EXCLUDED_TEST_DIRS. .ino is included for parity with the rest of the
codebase (CPP_EXTS, FILE_PATTERNS, fingerprint rules, etc.).
"""

import sys
from itertools import chain
from pathlib import Path


# Extensions discovered as host test sources. Kept in sync with the
# CPP_EXTS frozenset in ci/early_exit_cache.py and ci/meson/cache_utils.py.
TEST_SOURCE_GLOBS = ("*.cpp", "*.ino")


def discover_test_files(
    tests_dir: Path, excluded_files: set[Path], excluded_dirs: set[Path]
) -> list[str]:
    """
    Discover all test files in the tests directory.

    Recursively scans the entire tests/ tree for *.cpp and *.ino files,
    excluding infrastructure files and directories. No hardcoded subdirectory
    list needed.

    Args:
        tests_dir: Root directory containing test files
        excluded_files: Set of full paths to exclude (e.g., {Path(".../tests/doctest_main.cpp")})
        excluded_dirs: Set of full directory paths to exclude (e.g., {Path(".../tests/shared")})

    Returns:
        List of relative paths (POSIX format) to test files, sorted
    """
    test_files: list[str] = []

    for f in chain.from_iterable(tests_dir.rglob(g) for g in TEST_SOURCE_GLOBS):
        resolved = f.resolve()

        # Skip excluded files
        if resolved in excluded_files:
            continue

        # Skip files inside excluded directories
        if any(resolved.is_relative_to(d) for d in excluded_dirs):
            continue

        # Skip hidden directories and .dir directories
        rel = f.relative_to(tests_dir)
        parent_parts = rel.parts[:-1]  # directory components only
        if any(part.startswith(".") or part.endswith(".dir") for part in parent_parts):
            continue

        test_files.append(rel.as_posix())

    # Sort and deduplicate
    return sorted(set(test_files))


def main() -> None:
    """Main entry point for test discovery."""
    if len(sys.argv) != 2:
        print("Usage: discover_tests.py <tests_directory>", file=sys.stderr)
        sys.exit(1)

    tests_dir = Path(sys.argv[1]).resolve()

    if not tests_dir.exists():
        print(f"Error: Tests directory not found: {tests_dir}", file=sys.stderr)
        sys.exit(1)

    from test_config import EXCLUDED_TEST_DIRS, EXCLUDED_TEST_FILES

    test_files = discover_test_files(tests_dir, EXCLUDED_TEST_FILES, EXCLUDED_TEST_DIRS)

    # Output one file path per line
    for test_file in test_files:
        print(test_file)


if __name__ == "__main__":
    main()
