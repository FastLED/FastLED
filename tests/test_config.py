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
EXCLUDED_TEST_FILES: Set[str] = set()

# Subdirectories containing tests that should be discovered
TEST_SUBDIRS: List[str] = ["fl", "fx"]

# ============================================================================
# Test Categorization Configuration (for Unity Builds)
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

# Category definitions for unity build mode
# Tests are grouped into these categories to reduce compilation overhead
UNITY_BUILD_CATEGORIES: List[str] = [
    "fl_tests",       # fl/ directory tests (stdlib-like utilities)
    "fx_tests",       # fx/ directory tests (effects framework)
    "noise_tests",    # noise/ directory tests
    "platform_tests", # Platform-specific tests (SPI, ISR, ESP32, etc.)
    "core_tests",     # Core FastLED and general tests
]
