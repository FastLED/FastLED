#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to detect use of raw thread_local keyword.

The built-in C++ thread_local keyword does not work on AVR and other
single-threaded embedded platforms. Use fl::SingletonThreadLocal<T>::instance()
instead, which provides a portable, leak-safe, never-destroyed thread-local
singleton that compiles to a simple global on single-threaded targets and
POSIX thread-local storage on multi-threaded ones.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


class ThreadLocalKeywordChecker(FileContentChecker):
    """Checker class for raw thread_local keyword usage."""

    # Pre-compiled regex for matching thread_local keyword with word boundaries
    _THREAD_LOCAL_PATTERN = re.compile(r"\bthread_local\b")

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for thread_local usage."""
        # Check file extension - .h, .cpp, .hpp, and .ino files
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for thread_local keyword usage."""
        # Fast file-level check: skip if "thread_local" not in file at all
        if "thread_local" not in file_content.content:
            return []

        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Check each line for thread_local usage
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

            # Remove string literals to avoid matching in #include "fl/thread_local.h"
            # Simple approach: remove anything between quotes
            code_without_strings = self._remove_strings(code_part)

            # Check for thread_local usage in code portion only
            if self._THREAD_LOCAL_PATTERN.search(code_without_strings):
                # Allow suppression with "// ok thread_local" on same line
                if "// ok thread_local" not in line:
                    violations.append(
                        (
                            line_number,
                            f"❌ Raw 'thread_local' keyword is banned — "
                            f"use fl::SingletonThreadLocal<T>::instance() instead "
                            f"(portable, never-destroyed, LSAN-safe): "
                            f"{stripped}",
                        )
                    )

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally

    @staticmethod
    def _remove_strings(code: str) -> str:
        """Remove string literals from code to avoid false positives in filenames."""
        result = []
        in_string = False
        in_char = False
        i = 0
        while i < len(code):
            char = code[i]

            # Handle escape sequences
            if (in_string or in_char) and char == "\\" and i + 1 < len(code):
                i += 2
                continue

            # Toggle string state
            if char == '"' and not in_char:
                in_string = not in_string
                i += 1
                continue

            # Toggle char state
            if char == "'" and not in_string:
                in_char = not in_char
                i += 1
                continue

            # Add character if not in string/char literal
            if not in_string and not in_char:
                result.append(char)

            i += 1

        return "".join(result)


def main() -> None:
    """Run thread_local checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = ThreadLocalKeywordChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT)],
        "Found raw thread_local keyword — use fl::SingletonThreadLocal<T>::instance()",
        extensions=[".cpp", ".h", ".hpp", ".ino"],
    )


if __name__ == "__main__":
    main()
