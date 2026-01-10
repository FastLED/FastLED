"""
Configuration for FastLED test suite.

This module contains all configuration data for test discovery, naming,
and categorization. Separating configuration from build logic makes it
easier to maintain and modify test organization.
"""

from typing import List, Set

# ============================================================================
# Test Discovery Configuration
# ============================================================================

# Files to exclude from test discovery (special tests with custom build rules)
# Includes infrastructure files like test runner and doctest main
EXCLUDED_TEST_FILES: Set[str] = {"test_runner.cpp", "doctest_main.cpp"}

# Subdirectories containing tests that should be discovered
TEST_SUBDIRS: List[str] = ["fl", "fl/channels", "fl/detail", "fl/sensors", "fx", "ftl", "lib8tion", "platforms"]

# ============================================================================
# Test Categorization Configuration
# ============================================================================

# Patterns for identifying platform-specific tests
# Tests containing these keywords will be categorized as platform tests
PLATFORM_TEST_PATTERNS: List[str] = [
    "spi",
    "isr",
    "esp32",
    "riscv",
    "parallel",
    "quad",
    "single",
    "clockless",
    "dual",
    "stub_led",
]

# Maximum tests per category before subdividing
# Large categories (like fl_tests with 100 tests) will be split into
# multiple buckets to achieve ~50 tests per compilation unit
MAX_TESTS_PER_CATEGORY: int = 50

# Number of buckets for large categories
# For example, fl_tests (100 tests) splits into fl_tests_1-2 (~50 each)
LARGE_CATEGORY_BUCKET_COUNT: int = 2
