"""
Organize tests and output metadata for Meson build system.

This script discovers test files, extracts names, and categorizes them,
outputting all metadata in a format that Meson can easily parse.

Output format: TEST:<name>:<file_path>:<category>
Example: TEST:fl_algorithm:fl/algorithm.cpp:fl_tests
"""

import sys
from pathlib import Path
from discover_tests import discover_test_files
from test_config import EXCLUDED_TEST_FILES
from test_helpers import extract_test_name, categorize_test


def main() -> None:
    """Main entry point for test organization."""
    if len(sys.argv) != 2:
        print("Usage: organize_tests.py <tests_directory>", file=sys.stderr)
        sys.exit(1)

    tests_dir = Path(sys.argv[1]).resolve()

    if not tests_dir.exists():
        print(f"Error: Tests directory not found: {tests_dir}", file=sys.stderr)
        sys.exit(1)

    # Discover all test files
    test_files = discover_test_files(tests_dir, EXCLUDED_TEST_FILES)

    # Output metadata for each test
    for test_file_path in test_files:
        if not test_file_path:  # Skip empty strings
            continue

        test_name = extract_test_name(test_file_path)
        category = categorize_test(test_name, test_file_path)

        # Output in parseable format
        print(f"TEST:{test_name}:{test_file_path}:{category}")


if __name__ == "__main__":
    main()
