from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
"""Linter to check for 'namespace fl {' declarations in src/ root files.

This prevents namespace fl declarations directly in src/*.h and src/*.cpp files,
but allows them in subdirectories like src/chipsets/, src/protocols/, etc.
"""

import os
import re
import unittest
from pathlib import Path

from ci.util.check_files import (
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
)
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4


class NamespaceFlDeclarationChecker(FileContentChecker):
    """FileContentChecker implementation for detecting 'namespace fl {' in src/ root files."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed (only src/ root files)."""
        # Only check .h and .cpp files directly in src/ directory (not subdirectories)
        path_obj = Path(file_path)

        # Check if file is directly in src/ (parent directory is src/ AND parent's parent is project root)
        # This ensures we only match PROJECT_ROOT/src/*.{h,cpp} and not src/third_party/*/src/*.{h,cpp}
        if path_obj.parent.name != "src":
            return False

        # Ensure parent is actually PROJECT_ROOT/src, not some nested src/
        try:
            if path_obj.parent.resolve() != SRC_ROOT.resolve():
                return False
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            return False

        # Only check .h and .cpp files
        return path_obj.suffix in [".h", ".cpp"]

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for 'namespace fl {' declarations."""
        violations: list[tuple[int, str]] = []

        for line_num, line in enumerate(file_content.lines, 1):
            # Skip comment lines
            if line.strip().startswith("//"):
                continue
            # Match 'namespace fl {' pattern
            if re.search(r"\bnamespace\s+fl\s*\{", line):
                violations.append((line_num, line.strip()))

        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


class NoNamespaceFlDeclarationTester(unittest.TestCase):
    def check_file(self, file_path: str) -> list[str]:
        """Check if a file has namespace fl { declarations."""
        failings: list[str] = []
        try:
            with open(file_path, "r", encoding="utf-8") as f:
                for line_number, line in enumerate(f, 1):
                    # Skip comment lines
                    if line.strip().startswith("//"):
                        continue
                    # Match 'namespace fl {' pattern
                    if re.search(r"\bnamespace\s+fl\s*\{", line):
                        failings.append(f"{file_path}:{line_number}: {line.strip()}")
        except (UnicodeDecodeError, IOError):
            # Skip files that can't be read
            pass
        return failings

    def test_no_namespace_fl_in_src_root(self) -> None:
        """Check that 'namespace fl {' is not in src/*.h and src/*.cpp files.

        This only checks files directly in src/, not in subdirectories like
        src/chipsets/, src/protocols/, etc.
        """
        # Collect only files directly in src/ directory
        files_to_check: list[str] = []
        try:
            src_path = Path(SRC_ROOT)
            for file_path in src_path.glob("*"):
                # Only process .h and .cpp files directly in src/
                if file_path.is_file() and file_path.suffix in [".h", ".cpp"]:
                    files_to_check.append(str(file_path))
        except (OSError, IOError):
            pass

        # Create checker and processor
        checker = NamespaceFlDeclarationChecker()
        processor = MultiCheckerFileProcessor()

        # Process all files in a single pass
        processor.process_files_with_checkers(files_to_check, [checker])

        # Get violations from checker
        violations = checker.violations

        if violations:
            all_failings: list[str] = []
            for file_path, line_info in violations.items():
                for line_num, line_content in line_info:
                    all_failings.append(f"{file_path}:{line_num}: {line_content}")

            msg = (
                f"Found {len(all_failings)} file(s) with 'namespace fl {{' "
                f"in src/ root directory (not allowed):\n" + "\n".join(all_failings)
            )
            for failing in all_failings:
                print(failing)
            self.fail(msg)
        else:
            print("✅ No 'namespace fl {' declarations found in src/ root files.")


if __name__ == "__main__":
    unittest.main()
