#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
import os
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


class StdNamespaceChecker(FileContentChecker):
    """Checker class for std:: namespace usage."""

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for std:: namespace usage."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> List[str]:
        """Check file content for std:: namespace usage."""
        failings: List[str] = []
        in_multiline_comment = False

        # Check each line for std:: usage
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

            # Check for std:: usage in code portion only
            if "std::" in code_part and "// okay std namespace" not in line:
                failings.append(
                    f"Found 'std::' namespace in {file_content.path}:{line_number}"
                )

        return failings


def _test_no_std_namespace(
    test_directories: List[str],
    on_fail: Callable[[str], None],
) -> None:
    """Searches through the program files to check for std:: namespace usage."""
    # Collect files to check
    files_to_check = collect_files_to_check(test_directories)

    # Create processor and checker
    processor = MultiCheckerFileProcessor()
    checker = StdNamespaceChecker()

    # Process files
    results = processor.process_files_with_checkers(files_to_check, [checker])

    # Get results for std namespace checker
    all_failings = results.get("StdNamespaceChecker", []) or []

    if all_failings:
        msg = f"Found {len(all_failings)} std:: namespace usage(s): \n" + "\n".join(
            all_failings
        )
        for failing in all_failings:
            print(failing)

        on_fail(msg)
    else:
        print("No std:: namespace usage found.")


class TestNoStdNamespace(unittest.TestCase):
    def test_no_std_namespace_src(self) -> None:
        """Searches through the src directory to check for std:: namespace usage."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                "Use 'fl::' namespace instead of 'std::'. "
                "You can add '// okay std namespace' at the end of the line to silence this error for specific cases."
            )

        # Test directories
        test_directories = [
            os.path.join(SRC_ROOT, "fl"),
            os.path.join(SRC_ROOT, "fx"),
            os.path.join(SRC_ROOT, "sensors"),
        ]
        _test_no_std_namespace(
            test_directories=test_directories,
            on_fail=on_fail,
        )

    def test_no_std_namespace_examples(self) -> None:
        """Searches through the examples directory to check for std:: namespace usage."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                "Use 'fl::' namespace instead of 'std::'. "
                "You can add '// okay std namespace' at the end of the line to silence this error for specific cases."
            )

        test_directories = ["examples"]

        _test_no_std_namespace(
            test_directories=test_directories,
            on_fail=on_fail,
        )


if __name__ == "__main__":
    unittest.main()
