#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
import os
import re
import unittest
from typing import Callable, List

from ci.util.check_files import (
    EXCLUDED_FILES,
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"


class CppIncludeChecker(FileContentChecker):
    """Checker class for .cpp file includes."""

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for .cpp includes."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> List[str]:
        """Check file content for .cpp file includes."""
        failings: List[str] = []
        in_multiline_comment = False

        # Regex pattern to match #include "..." or #include <...> with .cpp extension
        # Matches both quotes and angle brackets
        cpp_include_pattern = re.compile(r'#include\s+[<"]([^>"]+\.cpp)[>"]')

        # Check each line for .cpp includes
        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue  # Skip the line with closing */

            # Skip if we're inside a multi-line comment
            if in_multiline_comment:
                continue

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion before checking
            code_part = line.split("//")[0]

            # Check for .cpp includes in code portion
            match = cpp_include_pattern.search(code_part)
            if match:
                cpp_file = match.group(1)
                failings.append(
                    f"Found .cpp file include '{cpp_file}' in {file_content.path}:{line_number}"
                )

        return failings


def _test_no_cpp_includes(
    test_directories: List[str],
    on_fail: Callable[[str], None],
) -> None:
    """Searches through the program files to check for .cpp file includes."""
    # Collect files to check
    files_to_check = collect_files_to_check(test_directories)

    # Create processor and checker
    processor = MultiCheckerFileProcessor()
    checker = CppIncludeChecker()

    # Process files
    results = processor.process_files_with_checkers(files_to_check, [checker])

    # Get results for cpp include checker
    all_failings = results.get("CppIncludeChecker", []) or []

    if all_failings:
        msg = f"Found {len(all_failings)} .cpp file include(s): \n" + "\n".join(
            all_failings
        )
        for failing in all_failings:
            print(failing)

        on_fail(msg)
    else:
        print("No .cpp file includes found.")


class TestNoCppIncludes(unittest.TestCase):
    def test_no_cpp_includes_src(self) -> None:
        """Searches through the src directory to check for .cpp file includes."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                ".cpp files should not be included. Only include header files (.h, .hpp). "
                "If you need to share implementation, move it to a header file or use proper linking."
            )

        # Test directories
        test_directories = [
            os.path.join(SRC_ROOT, "fl"),
            os.path.join(SRC_ROOT, "fx"),
            os.path.join(SRC_ROOT, "sensors"),
            os.path.join(SRC_ROOT, "platforms"),
            os.path.join(SRC_ROOT, "lib8tion"),
        ]
        _test_no_cpp_includes(
            test_directories=test_directories,
            on_fail=on_fail,
        )

    def test_no_cpp_includes_examples(self) -> None:
        """Searches through the examples directory to check for .cpp file includes."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                ".cpp files should not be included. Only include header files (.h, .hpp). "
                "If you need to share implementation, move it to a header file or use proper linking."
            )

        test_directories = ["examples"]

        _test_no_cpp_includes(
            test_directories=test_directories,
            on_fail=on_fail,
        )


if __name__ == "__main__":
    unittest.main()
