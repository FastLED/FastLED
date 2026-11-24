"""
Configuration for FastLED test suite.

This module contains all configuration data for test discovery, naming,
and categorization. Separating configuration from build logic makes it
easier to maintain and modify test organization.
"""

from pathlib import Path


# ============================================================================
# Test Discovery Configuration
# ============================================================================

# Root directory for test files
TESTS_DIR: Path = Path(__file__).parent.resolve()

# Doctest tier roots (see discover_tests.TEST_TIERS). Discovery only scans
# these, so the shared infrastructure beside them (tests/shared, tests/data,
# tests/driver, tests/profile, tests/ci, tests/fbuild_qemu_smoke, test.h,
# doctest_main.cpp, ...) is never mistaken for a test.
UNIT_DIR: Path = TESTS_DIR / "unit"
INTEGRATION_DIR: Path = TESTS_DIR / "integration"

# Files to exclude from test discovery (special tests with custom build rules)
EXCLUDED_TEST_FILES: set[Path] = {
    UNIT_DIR
    / "misc"
    / "coroutine_impl.cpp",  # Compiled into test_shared_objs, not a test
    UNIT_DIR
    / "fx"
    / "profile_chasing_spirals.cpp",  # Standalone profiling binary, not a test
    UNIT_DIR
    / "fl"
    / "remote"
    / "transport"
    / "http"
    / "test_utils"
    / "server_thread.cpp.hpp",  # Utility class, not a test (included by loopback.cpp)
    # Grouped into sibling test files via #include
    UNIT_DIR / "fl" / "grid.cpp",  # → geometry.cpp
    UNIT_DIR / "fl" / "tile2x2.cpp",  # → geometry.cpp
    UNIT_DIR / "fl" / "traverse_grid.cpp",  # → geometry.cpp
    UNIT_DIR / "fl" / "sin32.cpp",  # → trig8.cpp
    UNIT_DIR / "fl" / "time_alpha.cpp",  # → timeout.cpp
    UNIT_DIR / "fl" / "assume_aligned.cpp",  # → align.cpp
    UNIT_DIR / "fl" / "map_range.cpp",  # → clamp.cpp
    UNIT_DIR / "fl" / "unordered_map_lru.cpp",  # → hash.cpp
    UNIT_DIR / "fl" / "insert_result.cpp",  # → hash.cpp
    UNIT_DIR / "fl" / "alloca.cpp",  # → memory.cpp
    UNIT_DIR / "fl" / "numeric_limits.cpp",  # → types.cpp
    UNIT_DIR / "fl" / "trace.cpp",  # → log.cpp
    UNIT_DIR / "fl" / "eorder.cpp",  # → hsv16.cpp
    UNIT_DIR / "fl" / "xymap.cpp",  # → screenmap.cpp
    UNIT_DIR / "fl" / "splat.cpp",  # → downscale.cpp
    # Dedicated no-PCH tier-layout executable; registered directly in Meson.
    UNIT_DIR / "fl" / "channels" / "color_profile_tiny_layout.cpp",
    UNIT_DIR / "fl" / "channels" / "color_profile_tiny_runtime.cpp",
    UNIT_DIR / "fl" / "channels" / "engine_worker_defaults.cpp",
}

# Directories inside a tier to exclude from recursive test discovery.
# Directories starting with '.' or ending in '.dir' are auto-excluded
# (see discover_tests.py).
EXCLUDED_TEST_DIRS: set[Path] = {
    # Consolidated test directories (individual files included via parent .cpp)
    UNIT_DIR / "fl" / "chipsets" / "encoders",
    UNIT_DIR / "fl" / "log",
    # NOTE: tests/unit/fl/audio/detector/ is NOT excluded — it contains standalone .cpp test files
    UNIT_DIR / "fl" / "channels" / "detail" / "validation",
    UNIT_DIR / "fl" / "remote" / "rpc",
    UNIT_DIR / "fl" / "codec",
    UNIT_DIR / "fl" / "fx" / "2d",
}
