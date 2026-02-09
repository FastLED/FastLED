"""
Helper functions for FastLED test organization.

This module provides utilities for:
- Extracting test names from file paths
- Categorizing tests by directory and type
"""


def extract_test_name(test_file_path: str) -> str:
    """
    Extract a unique test name from a test file path.

    Naming rules:
    - fl/*.cpp files: prefix with "fl_" and include subdirectories
      (e.g., fl/algorithm.cpp -> fl_algorithm, fl/channels/spi.cpp -> fl_channels_spi)
    - fx/*.cpp files: prefix with "fx_" and include subdirectories
      (e.g., fx/engine.cpp -> fx_engine)
    - Other files: use basename without .cpp (e.g., noise/test_noise.cpp -> test_noise)

    Args:
        test_file_path: Relative path to test file (POSIX format)

    Returns:
        Test name to use as executable name
    """
    # Remove .cpp extension
    path_no_ext = test_file_path.replace(".cpp", "")

    # For fl/, fx/ subdirectories, convert path to name with underscores
    # This ensures uniqueness for nested files (e.g., fl/channels/spi.cpp vs fl/spi.cpp)
    if test_file_path.startswith("fl/"):
        # Replace / with _ to create unique name: fl/channels/spi.cpp -> fl_channels_spi
        return path_no_ext.replace("/", "_")
    elif test_file_path.startswith("fx/"):
        return path_no_ext.replace("/", "_")
    else:
        # For other files, just use basename
        return path_no_ext.split("/")[-1]


def categorize_test(test_name: str, test_file_path: str) -> str:
    """
    Categorize a test by directory and type.

    Categories:
    - fl_tests: Tests from fl/ directory (stdlib-like utilities)
    - fx_tests: Tests from fx/ directory or containing "fx" (effects framework)
    - noise_tests: Tests from noise/ directory
    - platform_tests: Tests from platforms/ directory
    - core_tests: All other tests

    Args:
        test_name: Name of the test (from extract_test_name)
        test_file_path: Relative path to test file (POSIX format)

    Returns:
        Category name for this test
    """
    # Categorize by directory path (consistent with how all other categories work)
    if test_name.startswith("fl_"):
        return "fl_tests"
    elif test_name.startswith("fx_") or "fx" in test_name:
        return "fx_tests"
    elif test_file_path.startswith("noise/"):
        return "noise_tests"
    elif test_file_path.startswith("platforms/"):
        return "platform_tests"

    # Default to core tests
    return "core_tests"
