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
    tests/fl/algorithm.cpp includes "fl/stl/algorithm.h" → checks src/fl/stl/algorithm.h
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
    "sketch_runner.cpp",  # Self-contained test for sketch runner functionality
    "spi_batching_logic.cpp",  # Self-contained algorithm validation test
    "serial_printf.cpp",  # Tests Arduino Serial API, not a FastLED header
    "test_runner.cpp",  # Test runner executable that loads test DLLs
    "runner.cpp",  # Windows DLL loader for test execution
    "crash_handler_main.cpp",  # Shared infrastructure for crash handling in runner
    "example_runner.cpp",  # Generic example runner that loads and executes example DLLs
    "fltest_self_test.cpp",  # Self-test for fl::test framework (tests fltest.h)
    "asan_leak.cpp",  # ASAN/LSAN symbolization verification test (no src header)
    "test_helpers.hpp",  # Shared test helper utilities (not 1:1 with source)
}

# Test directories that don't follow the 1:1 mapping (legacy code)
EXCLUDED_TEST_DIRS = {
    "core",  # Legacy tests with different structure
    "shared",  # Shared test infrastructure (not 1:1 with source)
    "test_utils",  # Test utilities (not 1:1 with source)
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
        """Only process .cpp and .hpp test files in tests/ directory."""
        path = Path(file_path)

        # Must be a .cpp or .hpp file (sub-tests use .hpp extension)
        if path.suffix not in (".cpp", ".hpp"):
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

        # Skip tests in tests/misc/ and tests/profile/ (these don't need to match source structure)
        try:
            rel_path = test_file.relative_to(TESTS_ROOT)
            if rel_path.parts[0] in ("misc", "profile"):
                return []
        except (ValueError, IndexError):
            pass

        # Check if file has "// ok standalone" comment in first few lines
        for line in file_content.lines[:5]:
            if "// ok standalone" in line.lower():
                return []  # Exempt from header existence checks

        # Extract includes from the test file
        includes = self._extract_includes(file_content)

        # Get candidate headers that should exist
        primary_candidate, fallback_candidates = self._get_expected_header_candidates(
            test_file, includes
        )

        # Check if primary exists (ideal case)
        primary_exists = primary_candidate.exists()

        # Check if any fallback exists
        fallback_exists = (
            any(candidate.exists() for candidate in fallback_candidates)
            if fallback_candidates
            else False
        )

        # LENIENT: Pass if ANY header exists (primary or fallback)
        # BUT: Warn if directory structure looks wrong
        if not primary_exists and not fallback_exists:
            # FAIL: No headers found at all
            test_rel = test_file.relative_to(PROJECT_ROOT)
            primary_rel = primary_candidate.relative_to(PROJECT_ROOT)

            msg_parts = [
                f"Test file {test_rel} has no corresponding header in src/",
                f"  Expected: {primary_rel}",
            ]

            if includes:
                msg_parts.append(f"  Includes: {', '.join(includes[:3])}")
                if len(includes) > 3:
                    msg_parts.append(f"  ... and {len(includes) - 3} more")
            else:
                msg_parts.append("  No project headers included!")

            error_msg = "\n".join(msg_parts)
            self.violations[file_content.path] = error_msg

        elif not primary_exists and fallback_exists:
            # WARN: Directory structure mismatch (like tests/ftl/ including fl/stl/)
            # Only warn if NO included header's directory matches the test's directory
            if includes:
                test_rel = test_file.relative_to(TESTS_ROOT)
                test_dir_path = str(test_rel.parent).replace("\\", "/")

                # Check if ANY include matches the test directory
                any_include_matches = False
                first_mismatched_include_dir = None
                for include in includes:
                    include_parts = include.rsplit("/", 1)
                    include_dir_path = (
                        include_parts[0] if len(include_parts) > 1 else ""
                    )
                    if (
                        test_dir_path
                        and include_dir_path
                        and test_dir_path == include_dir_path
                    ):
                        any_include_matches = True
                        break
                    if (
                        first_mismatched_include_dir is None
                        and test_dir_path
                        and include_dir_path
                        and test_dir_path != include_dir_path
                    ):
                        first_mismatched_include_dir = include_dir_path

                # Only warn if NO include matches the test directory
                # Also skip warning if the source directory matching the test path has headers
                src_mirror_dir = SRC_ROOT / test_dir_path
                has_source_headers = src_mirror_dir.is_dir() and any(
                    f.suffix in (".h", ".hpp") or f.name.endswith(".cpp.hpp")
                    for f in src_mirror_dir.iterdir()
                    if f.is_file()
                )
                if (
                    not any_include_matches
                    and first_mismatched_include_dir
                    and not has_source_headers
                ):
                    test_full_rel = test_file.relative_to(PROJECT_ROOT)
                    msg = (
                        f"⚠️  Test file {test_full_rel} may be in wrong directory:\n"
                        f"  Test location: tests/{test_dir_path}/\n"
                        f"  Includes headers from: src/{first_mismatched_include_dir}/\n"
                        f"  Expected location: tests/{first_mismatched_include_dir}/"
                    )
                    self.violations[file_content.path] = msg

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
                # Skip test framework headers and test utilities
                if (
                    "test.h" not in header_path
                    and not header_path.startswith("testing/")
                    and not header_path.startswith("test_utils/")
                ):
                    includes.append(header_path)

        return includes

    def _get_expected_header_candidates(
        self, test_file: Path, includes: list[str]
    ) -> tuple[Path, list[Path]]:
        """Get candidate header paths that this test file should be testing.

        Returns:
            (primary_candidate, fallback_candidates)
            - primary: Expected header based on test directory structure
            - fallbacks: Headers actually included by the test

        Examples:
            tests/fl/algorithm.cpp → (src/fl/algorithm.h, [included headers])
            tests/fx/engine.cpp → (src/fx/engine.h, [included headers])
        """
        # Get path relative to tests root
        relative_path = test_file.relative_to(TESTS_ROOT)

        # Get the base name without extension
        base_name = relative_path.stem + ".h"

        # PRIMARY: Expected header based on directory structure
        # tests/fl/stl/algorithm.cpp -> src/fl/stl/algorithm.h
        primary_candidate = SRC_ROOT / relative_path.parent / base_name

        # FALLBACKS: Headers actually included in the test file
        fallback_candidates: list[Path] = []
        for include in includes:
            # Convert include path to absolute path
            header_path = SRC_ROOT / include
            if header_path != primary_candidate:
                fallback_candidates.append(header_path)

        return primary_candidate, fallback_candidates


def main() -> None:
    """Run headers exist checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = HeadersExistChecker()
    run_checker_standalone(
        checker,
        [str(TESTS_ROOT)],
        "Found test files without corresponding headers",
        extensions=[".cpp", ".hpp"],
    )


if __name__ == "__main__":
    main()
