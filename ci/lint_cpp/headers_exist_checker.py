#!/usr/bin/env python3
"""
Checker to ensure every test file has a corresponding header in src/.

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
from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
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
    "test_runner.cpp",  # Test runner executable that loads test DLLs
    "runner.cpp",  # Windows DLL loader for test execution
    "crash_handler_main.cpp",  # Shared infrastructure for crash handling in runner
    "example_runner.cpp",  # Generic example runner that loads and executes example DLLs
    "fltest_self_test.cpp",  # Self-test for fl::test framework (tests fltest.h)
}

# Test directories that don't follow the 1:1 mapping (legacy code)
EXCLUDED_TEST_DIRS = {
    "core",  # Legacy tests with different structure
    ".build-examples-all",  # Generated build artifacts for example compilation
}

# Pattern to match #include statements with project headers (not system headers)
INCLUDE_PATTERN = re.compile(r'^\s*#include\s+"([^"]+)"')


class HeadersExistChecker(FileContentChecker):
    """Checker that validates test files have corresponding headers in src/."""

    def __init__(self):
        # Dictionary to store violations: file_path -> error_message
        self.violations: dict[str, str] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process .cpp files in tests/ directory."""
        path = Path(file_path)

        # Must be a .cpp file
        if path.suffix != ".cpp":
            return False

        # Must be in tests/ directory
        try:
            relative_path = path.relative_to(TESTS_ROOT)
        except ValueError:
            return False

        # Skip excluded files
        if path.name in EXCLUDED_TEST_FILES:
            return False

        # Skip excluded directories
        if any(part in EXCLUDED_TEST_DIRS for part in relative_path.parts):
            return False

        # Skip build-related directories
        if any(
            part == "build"
            or part.startswith(".build-")
            or part == "example_compile_direct"
            or part == "CMakeFiles"
            for part in relative_path.parts
        ):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check if test file has corresponding header in src/."""
        test_file = Path(file_content.path)

        # Extract includes from the test file
        includes = self._extract_includes(file_content)

        # Get candidate headers that should exist
        candidates = self._get_expected_header_candidates(test_file, includes)

        # Check if at least one candidate exists
        found = any(candidate.exists() for candidate in candidates)

        if not found:
            # Build error message
            test_rel = test_file.relative_to(PROJECT_ROOT)
            msg_parts = [f"Test file {test_rel} has no corresponding header in src/"]

            if includes:
                msg_parts.append(f"  Includes: {', '.join(includes[:3])}")
                if len(includes) > 3:
                    msg_parts.append(f"  ... and {len(includes) - 3} more")

            msg_parts.append("  Expected one of:")
            for candidate in candidates[:3]:
                candidate_rel = candidate.relative_to(PROJECT_ROOT)
                exists_marker = "✓" if candidate.exists() else "✗"
                msg_parts.append(f"    {exists_marker} {candidate_rel}")

            error_msg = "\n".join(msg_parts)
            self.violations[file_content.path] = error_msg

        return []  # We collect violations internally

    def _extract_includes(self, file_content: FileContent) -> list[str]:
        """Extract all #include statements from a test file.

        Returns list of included header paths (e.g., ["fl/async.h", "fl/task.h"])
        Filters out "test.h" and other test-related includes.
        """
        includes: list[str] = []

        for line in file_content.lines:
            match = INCLUDE_PATTERN.match(line)
            if match:
                header_path = match.group(1)
                # Skip test framework headers
                if "test.h" not in header_path and not header_path.startswith(
                    "testing/"
                ):
                    includes.append(header_path)

        return includes

    def _get_expected_header_candidates(
        self, test_file: Path, includes: list[str]
    ) -> list[Path]:
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
        # For tests/ftl/*.cpp, check both src/ftl/ and src/fl/stl/
        elif relative_path.parts[0] == "ftl":
            rest_of_path = (
                Path(*relative_path.parts[1:])
                if len(relative_path.parts) > 1
                else Path(".")
            )
            candidates.append(SRC_ROOT / "ftl" / rest_of_path.parent / base_name)
            candidates.append(SRC_ROOT / "fl" / "stl" / rest_of_path.parent / base_name)
        else:
            # For other directories, use the same structure
            candidates.append(SRC_ROOT / relative_path.parent / base_name)

        # Also check headers that are actually included in the test file
        for include in includes:
            # Convert include path to absolute path
            header_path = SRC_ROOT / include
            if header_path not in candidates:
                candidates.append(header_path)

        return candidates


def main() -> None:
    """Run headers exist checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = HeadersExistChecker()
    run_checker_standalone(
        checker,
        [str(TESTS_ROOT)],
        "Found test files without corresponding headers",
        extensions=[".cpp"],
    )


if __name__ == "__main__":
    main()
