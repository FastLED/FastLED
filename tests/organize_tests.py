"""
Organize tests and output metadata for Meson build system.

This script discovers test files, extracts names, and categorizes them,
outputting all metadata in a format that Meson can easily parse.

Large categories (> MAX_TESTS_PER_CATEGORY) are automatically subdivided
into numbered buckets for optimal compilation performance.

Output format: TEST:<name>:<file_path>:<category>
Example: TEST:fl_algorithm:fl/algorithm.cpp:fl_tests_1
"""

import sys
from pathlib import Path
from typing import Dict, List
from discover_tests import discover_test_files
from test_config import EXCLUDED_TEST_FILES, MAX_TESTS_PER_CATEGORY, TEST_SUBDIRS
from test_helpers import extract_test_name, categorize_test, subdivide_category


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
    test_files = discover_test_files(tests_dir, EXCLUDED_TEST_FILES, TEST_SUBDIRS)

    # First pass: categorize all tests and count per category
    test_metadata: List[tuple[str, str, str]] = []  # (test_name, test_file_path, base_category)
    category_counts: Dict[str, int] = {}

    for test_file_path in test_files:
        if not test_file_path:  # Skip empty strings
            continue

        test_name = extract_test_name(test_file_path)
        base_category = categorize_test(test_name, test_file_path)

        test_metadata.append((test_name, test_file_path, base_category))
        category_counts[base_category] = category_counts.get(base_category, 0) + 1

    # Second pass: subdivide large categories and output
    for test_name, test_file_path, base_category in test_metadata:
        # Subdivide large categories
        if category_counts[base_category] > MAX_TESTS_PER_CATEGORY:
            final_category = subdivide_category(test_name, base_category)
        else:
            final_category = base_category

        # Output in parseable format
        print(f"TEST:{test_name}:{test_file_path}:{final_category}")


if __name__ == "__main__":
    main()
