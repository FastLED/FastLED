#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure test files mirror the directory structure of source files.

This checker validates that test files in tests/ follow the same directory
structure as their corresponding source files in src/. For example:
- If src/fl/stl/flat_map.h exists, the test should be at tests/fl/stl/flat_map.cpp
- If the test is at tests/fl/flat_map.cpp, this is flagged as an error

This ensures test organization matches source organization for maintainability.
"""

from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


TESTS_ROOT = PROJECT_ROOT / "tests"
SRC_ROOT = PROJECT_ROOT / "src"

# Test files that are exempt from path matching (infrastructure/entry points)
EXCLUDED_TEST_FILES = {
    "doctest_main.cpp",  # Test framework entry point
}


class TestPathStructureChecker(FileContentChecker):
    """Checker class for test file path structure validation."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for path structure validation."""
        # Only check .cpp files in tests directory
        if not file_path.startswith(str(TESTS_ROOT)):
            return False

        # Only check .cpp files (test files)
        if not file_path.endswith(".cpp"):
            return False

        test_path = Path(file_path)

        # Skip excluded files (infrastructure/entry points)
        if test_path.name in EXCLUDED_TEST_FILES:
            return False

        # Skip tests/misc/ directory (these tests don't need to match source structure)
        try:
            rel_path = test_path.relative_to(TESTS_ROOT)
            if rel_path.parts[0] == "misc":
                return False
        except (ValueError, IndexError):
            pass

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check if test file path matches the source file directory structure.

        Rule: tests/**/file.cpp must match src/**/file.{h,cpp,cpp.hpp}
        Exception: Tests in tests/misc/ are exempt (don't need to match).
        Exception: Tests with '// standalone test' comment are exempt.
        """
        test_path = Path(file_content.path)

        # Get the relative path from tests root: tests/fl/flat_map.cpp -> fl/flat_map
        rel_from_tests = test_path.relative_to(TESTS_ROOT)
        test_name_no_ext = rel_from_tests.with_suffix("")  # Remove .cpp

        # Check for source file at exact matching path with various extensions
        source_extensions = [".h", ".cpp", ".cpp.hpp", ".hpp"]
        expected_source_base = SRC_ROOT / test_name_no_ext

        # If any matching source file exists at the expected location, no issue
        for ext in source_extensions:
            if expected_source_base.with_suffix(ext).exists():
                return []

        # Check if file has "// ok standalone" comment in first few lines
        for line in file_content.lines[:5]:  # Check first 5 lines
            if "// ok standalone" in line.lower():
                return []  # Exempt from path matching requirement

        # Source file doesn't exist at expected location
        # Flag as violation (test file has no corresponding source at matching path)
        rel_current_test = test_path.relative_to(PROJECT_ROOT)

        message = (
            f"Test file has no corresponding source file at matching path. "
            f"Test is at '{rel_current_test}' but no source file found at "
            f"'src/{rel_from_tests.with_suffix('')}.{{h,cpp,cpp.hpp}}'. "
            f"\n\n"
            f"REQUIRED ACTIONS (in order of preference):\n"
            f"  1. RENAME the test to match the source file it's testing (best option)\n"
            f"  2. MERGE this test into an existing test file if it tests the same source\n"
            f"  3. MOVE to 'tests/misc/{test_path.name}' if this truly doesn't test a specific source file\n"
            f"  4. ONLY as a last resort: add '// ok standalone' comment at the top if this absolutely cannot be organized otherwise\n\n"
            f"Test organization should mirror source organization for maintainability."
        )

        self.violations[file_content.path] = [(1, message)]
        return []  # We collect violations internally


def main() -> None:
    """Run test path structure checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = TestPathStructureChecker()
    run_checker_standalone(
        checker,
        [str(TESTS_ROOT)],
        "Found test files with incorrect path structure",
        extensions=[".cpp"],
    )


if __name__ == "__main__":
    main()
