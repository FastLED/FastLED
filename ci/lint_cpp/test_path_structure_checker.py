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

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check if test file path matches the source file directory structure."""
        test_path = Path(file_content.path)

        # Get the relative path from tests root: tests/fl/flat_map.cpp -> fl/flat_map
        rel_from_tests = test_path.relative_to(TESTS_ROOT)
        test_name_no_ext = rel_from_tests.with_suffix("")  # Remove .cpp

        # Convert to source path to search for
        # tests/fl/flat_map.cpp -> search for src/fl/**/flat_map.h
        potential_src_dir = SRC_ROOT / test_name_no_ext.parent
        test_basename = test_name_no_ext.name

        # Check if the source file exists at the expected location
        expected_header = SRC_ROOT / test_name_no_ext.with_suffix(".h")

        # If expected location exists, no issue
        if expected_header.exists():
            return []

        # Now search for the actual location of this header file in subdirectories
        # Only search within the same top-level directory (e.g., fl/, platforms/, etc.)
        # This prevents false positives from unrelated files with the same name
        actual_header_path = None

        # Search for any .h file with this basename under the expected directory and its subdirs
        if potential_src_dir.exists():
            # Search recursively for the header file
            for header_path in potential_src_dir.rglob(f"{test_basename}.h"):
                actual_header_path = header_path
                break  # Take the first match

        # If we found the header in a different subdirectory structure, flag this
        if actual_header_path is not None:
            # Get the relative path from src root
            actual_rel_from_src = actual_header_path.relative_to(SRC_ROOT)
            expected_rel_from_src = expected_header.relative_to(SRC_ROOT)

            # Check if paths differ (excluding extension)
            actual_path_no_ext = actual_rel_from_src.with_suffix("")
            expected_path_no_ext = expected_rel_from_src.with_suffix("")

            if actual_path_no_ext != expected_path_no_ext:
                # Paths differ - test file is in wrong location
                correct_test_path = TESTS_ROOT / actual_path_no_ext.with_suffix(".cpp")
                rel_correct_test = correct_test_path.relative_to(PROJECT_ROOT)
                rel_actual_src = actual_header_path.relative_to(PROJECT_ROOT)
                rel_current_test = test_path.relative_to(PROJECT_ROOT)

                message = (
                    f"Test file path does not match source file structure. "
                    f"Source file is at '{rel_actual_src}' but test is at '{rel_current_test}'. "
                    f"Move test to '{rel_correct_test}' to match source structure."
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
