#!/usr/bin/env python3
"""
Lint check: Ensure every test file has a corresponding header in src/.

This validates that test files in tests/**/*.cpp have matching headers
in src/**/*.h by checking the actual #include statements in test files.

How it works:
    1. Find all test .cpp files
    2. Parse #include statements to find project headers (not test.h)
    3. Verify at least one included header exists in src/
    4. Check if the main matching header exists (based on test filename)

Examples:
    tests/fl/algorithm.cpp includes "ftl/algorithm.h" → checks src/ftl/algorithm.h
    tests/fl/async.cpp includes "fl/async.h" → checks src/fl/async.h
    tests/fx/engine.cpp includes "fx/engine.h" → checks src/fx/engine.h

Exceptions:
    - tests/doctest_main.cpp (test framework entry point)
    - tests/core/*.cpp (legacy tests that may not have 1:1 mapping)
    - Tests can be excluded by adding them to EXCLUDED_TEST_FILES
"""

import re
import unittest
from pathlib import Path

from ci.util.paths import PROJECT_ROOT


TESTS_ROOT = PROJECT_ROOT / "tests"
SRC_ROOT = PROJECT_ROOT / "src"

# Test files that don't need corresponding headers
EXCLUDED_TEST_FILES = {
    "doctest_main.cpp",  # Test framework entry point
    "sketch_runner_demo.cpp",  # Demo file, not a real test
    "sketch_runner.cpp",  # Self-contained test for sketch runner functionality
    "test_spi_batching_logic.cpp",  # Self-contained algorithm validation test
    "serial_printf.cpp",  # Tests Arduino Serial API, not a FastLED header
}

# Test directories that don't follow the 1:1 mapping (legacy code)
EXCLUDED_TEST_DIRS = {
    "core",  # Legacy tests with different structure
    ".build-examples-all",  # Generated build artifacts for example compilation
}

# Pattern to match #include statements with project headers (not system headers)
INCLUDE_PATTERN = re.compile(r'^\s*#include\s+"([^"]+)"')


def find_test_files() -> list[Path]:
    """Find all test .cpp files that should have corresponding headers."""
    test_files: list[Path] = []

    for test_file in TESTS_ROOT.rglob("*.cpp"):
        # Skip excluded files
        if test_file.name in EXCLUDED_TEST_FILES:
            continue

        # Skip excluded directories
        relative_path = test_file.relative_to(TESTS_ROOT)
        if any(part in EXCLUDED_TEST_DIRS for part in relative_path.parts):
            continue

        # Skip build-related directories (with pattern matching)
        # Matches: build, .build-examples-*, example_compile_direct, CMakeFiles
        if any(
            part == "build"
            or part.startswith(".build-")
            or part == "example_compile_direct"
            or part == "CMakeFiles"
            for part in relative_path.parts
        ):
            continue

        test_files.append(test_file)

    return test_files


def extract_includes(test_file: Path) -> list[str]:
    """Extract all #include statements from a test file.

    Returns list of included header paths (e.g., ["fl/async.h", "fl/task.h"])
    Filters out "test.h" and other test-related includes.
    """
    includes: list[str] = []

    try:
        content = test_file.read_text(encoding="utf-8")
        for line in content.splitlines():
            match = INCLUDE_PATTERN.match(line)
            if match:
                header_path = match.group(1)
                # Skip test framework headers
                if "test.h" not in header_path and not header_path.startswith(
                    "testing/"
                ):
                    includes.append(header_path)
    except Exception:
        # If we can't read the file, return empty list
        pass

    return includes


def get_expected_header_candidates(test_file: Path) -> list[Path]:
    """Get candidate header paths that this test file should be testing.

    Returns multiple candidates based on common patterns:
    1. Same relative path in src/fl/, src/ftl/, src/fx/ etc.
    2. Headers actually included by the test file

    Examples:
        tests/fl/algorithm.cpp → [src/fl/algorithm.h, src/ftl/algorithm.h]
        tests/fx/engine.cpp → [src/fx/engine.h]
    """
    candidates: list[Path] = []

    # Get path relative to tests root
    relative_path = test_file.relative_to(TESTS_ROOT)

    # Get the base name without extension
    base_name = relative_path.stem + ".h"

    # For tests/fl/*.cpp, check both src/fl/ and src/ftl/
    if relative_path.parts[0] == "fl":
        rest_of_path = (
            Path(*relative_path.parts[1:])
            if len(relative_path.parts) > 1
            else Path(".")
        )
        candidates.append(SRC_ROOT / "fl" / rest_of_path.parent / base_name)
        candidates.append(SRC_ROOT / "ftl" / rest_of_path.parent / base_name)
    else:
        # For other directories, use the same structure
        candidates.append(SRC_ROOT / relative_path.parent / base_name)

    # Also check headers that are actually included in the test file
    includes = extract_includes(test_file)
    for include in includes:
        # Convert include path to absolute path
        header_path = SRC_ROOT / include
        if header_path not in candidates:
            candidates.append(header_path)

    return candidates


class TestTestHeadersExist(unittest.TestCase):
    """Unit tests for test-to-header mapping validation."""

    def test_all_test_files_have_headers(self) -> None:
        """Verify every test file has a corresponding header in src/.

        This test checks that:
        1. Each test file includes at least one project header from src/
        2. At least one of the expected header candidates exists
        3. Test files are in the correct directory (tests/fl/ vs tests/ftl/)
        """
        test_files = find_test_files()
        missing_headers: list[tuple[Path, list[Path], list[str]]] = []
        wrong_directory: list[tuple[Path, str, str]] = []

        for test_file in test_files:
            # Get all candidate headers for this test
            candidates = get_expected_header_candidates(test_file)

            # Check if at least one candidate exists
            found = any(candidate.exists() for candidate in candidates)

            if not found:
                # Get includes to show what the test is trying to include
                includes = extract_includes(test_file)
                missing_headers.append((test_file, candidates, includes))

            # Check if test is in the wrong directory
            # If test includes ftl/* headers, it should be in tests/ftl/
            # If test includes fl/* headers (non-ftl), it should be in tests/fl/
            relative_path = test_file.relative_to(TESTS_ROOT)
            test_dir = relative_path.parts[0] if len(relative_path.parts) > 0 else ""

            # Only check tests in tests/fl/ directory
            if test_dir == "fl":
                includes = extract_includes(test_file)
                # Check what headers are actually included
                has_ftl_includes = any(inc.startswith("ftl/") for inc in includes)
                has_fl_includes = any(
                    inc.startswith("fl/") and not inc.startswith("ftl/")
                    for inc in includes
                )

                # If it includes ftl/ headers, it should be in tests/ftl/
                if has_ftl_includes and not has_fl_includes:
                    expected_path = "tests/ftl/" + "/".join(relative_path.parts[1:])
                    wrong_directory.append((test_file, "ftl", expected_path))

        if missing_headers or wrong_directory:
            msg_lines: list[str] = []

            # Report missing headers first
            if missing_headers:
                msg_lines.append(
                    f"Found {len(missing_headers)} test file(s) without corresponding headers:\n"
                )

                for test_file, candidates, includes in missing_headers:
                    test_rel = test_file.relative_to(PROJECT_ROOT)
                    msg_lines.append(f"  {test_rel}")

                    if includes:
                        msg_lines.append(f"    Includes: {', '.join(includes[:3])}")
                        if len(includes) > 3:
                            msg_lines.append(
                                f"              ... and {len(includes) - 3} more"
                            )

                    msg_lines.append("    Expected one of:")
                    for candidate in candidates[:3]:  # Show first 3 candidates
                        candidate_rel = candidate.relative_to(PROJECT_ROOT)
                        exists_marker = "✓" if candidate.exists() else "✗"
                        msg_lines.append(f"      {exists_marker} {candidate_rel}")

                    msg_lines.append("")  # Empty line between entries

                msg_lines.extend(
                    [
                        "REASON: Test files should have corresponding headers in src/ to ensure",
                        "proper test-to-source mapping and maintainability.",
                        "",
                        "SOLUTION:",
                        "  1. Create the missing header file in src/ (check the 'Expected one of' list)",
                        "  2. OR add the test file to EXCLUDED_TEST_FILES if it's a special case",
                        "  3. OR add the directory to EXCLUDED_TEST_DIRS for legacy test directories",
                        "  4. OR check if the test includes the correct headers (see 'Includes' list)",
                        "",
                    ]
                )

            # Report wrong directory placement
            if wrong_directory:
                msg_lines.append(
                    f"Found {len(wrong_directory)} test file(s) in the wrong directory:\n"
                )

                for test_file, expected_dir, expected_path in wrong_directory:
                    test_rel = test_file.relative_to(PROJECT_ROOT)
                    msg_lines.append(f"  {test_rel}")
                    msg_lines.append(f"    Should be: {expected_path}")
                    includes = extract_includes(test_file)
                    ftl_includes = [inc for inc in includes if inc.startswith("ftl/")]
                    if ftl_includes:
                        msg_lines.append(
                            f"    Includes ftl/ headers: {', '.join(ftl_includes[:3])}"
                        )
                    msg_lines.append("")  # Empty line between entries

                msg_lines.extend(
                    [
                        "REASON: Test files should mirror the src/ directory structure.",
                        "Tests that include 'ftl/*' headers should be in tests/ftl/,",
                        "and tests that include 'fl/*' headers should be in tests/fl/.",
                        "",
                        "SOLUTION:",
                        "  1. Move the test file to the correct directory (see 'Should be' path)",
                        "  2. Create tests/ftl/ directory if it doesn't exist",
                        "  3. Update the test's #include statements if they're incorrect",
                    ]
                )

            self.fail("\n".join(msg_lines))


if __name__ == "__main__":
    unittest.main()
