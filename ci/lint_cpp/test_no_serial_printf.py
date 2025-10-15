#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Test to ensure Serial.printf is not used in example files."""

import os
import unittest
from typing import Callable, List

from ci.util.check_files import (
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


EXAMPLES_ROOT = PROJECT_ROOT / "examples"


class SerialPrintfChecker(FileContentChecker):
    """Checker class for Serial.printf usage in example files."""

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for Serial.printf usage."""
        # Only check files in examples directory
        if not file_path.startswith(str(EXAMPLES_ROOT)):
            return False

        # Check file extension - only .h, .cpp, .hpp files
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> List[str]:
        """Check file content for Serial.printf usage."""
        failings: List[str] = []
        in_multiline_comment = False

        # Check each line for Serial.printf usage
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

            # Check for Serial.printf usage in code portion only
            if "Serial.printf" in code_part:
                failings.append(
                    f"Found 'Serial.printf' in {file_content.path}:{line_number}"
                )

        return failings


def _test_no_serial_printf(
    test_directories: List[str],
    on_fail: Callable[[str], None],
) -> None:
    """Searches through the example files to check for Serial.printf usage."""
    # Collect files to check - only .h, .cpp, .hpp files
    files_to_check = collect_files_to_check(
        test_directories, extensions=[".h", ".cpp", ".hpp"]
    )

    # Create processor and checker
    processor = MultiCheckerFileProcessor()
    checker = SerialPrintfChecker()

    # Process files
    results = processor.process_files_with_checkers(files_to_check, [checker])

    # Get results for Serial.printf checker
    all_failings = results.get("SerialPrintfChecker", []) or []

    if all_failings:
        msg = f"Found {len(all_failings)} Serial.printf usage(s): \n" + "\n".join(
            all_failings
        )
        for failing in all_failings:
            print(failing)

        on_fail(msg)
    else:
        print("No Serial.printf usage found in examples.")


class TestNoSerialPrintf(unittest.TestCase):
    def test_no_serial_printf_examples(self) -> None:
        """Searches through the examples directory to check for Serial.printf usage."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg
                + "\n"
                + "Serial.printf is not allowed in example files. "
                + "Use FL_DBG() or FL_WARN() macros from fl/compiler_control.h instead."
            )

        test_directories = [str(EXAMPLES_ROOT)]

        _test_no_serial_printf(
            test_directories=test_directories,
            on_fail=on_fail,
        )


if __name__ == "__main__":
    unittest.main()
