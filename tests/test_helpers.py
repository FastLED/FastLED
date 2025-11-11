"""
Helper functions for FastLED test organization.

This module provides utilities for:
- Extracting test names from file paths
- Categorizing tests for unity build mode
- Organizing tests into categories
"""

from typing import Dict, List
from test_config import PLATFORM_TEST_PATTERNS, LARGE_CATEGORY_BUCKET_COUNT


def extract_test_name(test_file_path: str) -> str:
    """
    Extract a unique test name from a test file path.

    Naming rules:
    - fl/*.cpp files: prefix with "fl_" (e.g., fl/algorithm.cpp -> fl_algorithm)
    - fx/*.cpp files: prefix with "fx_" (e.g., fx/engine.cpp -> fx_engine)
    - Other files: use basename without .cpp (e.g., noise/test_noise.cpp -> test_noise)

    Args:
        test_file_path: Relative path to test file (POSIX format)

    Returns:
        Test name to use as executable name
    """
    # Get the base filename without extension
    base_name = test_file_path.split("/")[-1].replace(".cpp", "")

    # Add prefix for subdirectory files to avoid naming conflicts
    if test_file_path.startswith("fl/"):
        return f"fl_{base_name}"
    elif test_file_path.startswith("fx/"):
        return f"fx_{base_name}"
    else:
        return base_name


def categorize_test(test_name: str, test_file_path: str) -> str:
    """
    Categorize a test for unity build grouping.

    Categories:
    - fl_tests: Tests from fl/ directory (stdlib-like utilities)
    - fx_tests: Tests from fx/ directory or containing "fx" (effects framework)
    - noise_tests: Tests from noise/ directory
    - platform_tests: Tests containing platform-specific keywords
    - core_tests: All other tests

    Large categories (> 40 tests) will be subdivided by organize_tests.py.

    Args:
        test_name: Name of the test (from extract_test_name)
        test_file_path: Relative path to test file (POSIX format)

    Returns:
        Base category name for this test
    """
    # Check prefix and file path based categories first
    if test_name.startswith("fl_"):
        return "fl_tests"
    elif test_name.startswith("fx_") or "fx" in test_name:
        return "fx_tests"
    elif test_file_path.startswith("noise/"):
        return "noise_tests"

    # Check if it's a platform test
    for pattern in PLATFORM_TEST_PATTERNS:
        if pattern in test_name:
            return "platform_tests"

    # Default to core tests
    return "core_tests"


def subdivide_category(test_name: str, base_category: str) -> str:
    """
    Subdivide a test into a numbered bucket within its category.

    Used for large categories that need to be split into multiple
    compilation units (e.g., fl_tests -> fl_tests_1, fl_tests_2, etc.).

    Args:
        test_name: Name of the test
        base_category: Base category name (e.g., "fl_tests")

    Returns:
        Subdivided category name (e.g., "fl_tests_1")
    """
    # Use a deterministic hash that doesn't change between Python runs
    # Python's hash() is randomized for security, but we need consistency
    import hashlib
    hash_value = int(hashlib.md5(test_name.encode()).hexdigest(), 16)
    bucket_index = hash_value % LARGE_CATEGORY_BUCKET_COUNT
    return f"{base_category}_{bucket_index + 1}"


def organize_tests_by_category(test_file_paths: List[str]) -> Dict[str, List[str]]:
    """
    Organize test files into categories for unity build mode.

    Args:
        test_file_paths: List of relative paths to test files (POSIX format)

    Returns:
        Dictionary mapping category names to lists of test file paths
    """
    categories: Dict[str, List[str]] = {
        "fl_tests": [],
        "fx_tests": [],
        "noise_tests": [],
        "platform_tests": [],
        "core_tests": [],
    }

    for test_file_path in test_file_paths:
        if not test_file_path:  # Skip empty strings
            continue

        test_name = extract_test_name(test_file_path)
        category = categorize_test(test_name, test_file_path)
        categories[category].append(test_file_path)

    return categories
