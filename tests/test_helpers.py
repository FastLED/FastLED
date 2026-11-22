"""
Helper functions for FastLED test organization.

This module provides utilities for:
- Extracting test names from file paths
- Categorizing tests by directory and type
"""

from discover_tests import split_tier


def extract_test_name(test_file_path: str) -> str:
    """
    Extract a unique test name from a test file path.

    The leading tier directory (unit/, integration/) is not part of the name,
    so moving a test between tiers never renames it.

    Naming rules (applied to the path inside the tier):
    - fl/*.cpp files: prefix with "fl_" and include subdirectories
      (e.g., unit/fl/algorithm.cpp -> fl_algorithm,
      unit/fl/channels/spi.cpp -> fl_channels_spi)
    - fx/*.cpp files: prefix with "fx_" and include subdirectories
      (e.g., unit/fx/engine.cpp -> fx_engine)
    - Other files: use basename without .cpp
      (e.g., unit/misc/containers.cpp -> containers)

    Args:
        test_file_path: Path relative to tests/ (POSIX format), including the
            tier directory

    Returns:
        Test name to use as executable name
    """
    _, in_tier = split_tier(test_file_path)

    # Remove .cpp extension
    path_no_ext = in_tier.replace(".cpp", "")

    # For fl/, fx/ subdirectories, convert path to name with underscores
    # This ensures uniqueness for nested files (e.g., fl/channels/spi.cpp vs fl/spi.cpp)
    if in_tier.startswith("fl/"):
        # Replace / with _ to create unique name: fl/channels/spi.cpp -> fl_channels_spi
        return path_no_ext.replace("/", "_")
    elif in_tier.startswith("fx/"):
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
        test_file_path: Path relative to tests/ (POSIX format), including the
            tier directory

    Returns:
        Category name for this test
    """
    _, in_tier = split_tier(test_file_path)

    # Categorize by directory path (consistent with how all other categories work)
    if test_name.startswith("fl_"):
        return "fl_tests"
    elif test_name.startswith("fx_") or "fx" in test_name:
        return "fx_tests"
    elif in_tier.startswith("noise/"):
        return "noise_tests"
    elif in_tier.startswith("platforms/"):
        return "platform_tests"

    # Default to core tests
    return "core_tests"
