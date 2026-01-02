#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for .cpp file includes."""

import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


class CppIncludeChecker(FileContentChecker):
    """Checker class for .cpp file includes."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for .cpp includes."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for .cpp file includes."""
        violations: list[tuple[int, str]] = []
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
                violations.append((line_number, f'#include "{cpp_file}"'))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run .cpp include checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = CppIncludeChecker()
    run_checker_standalone(
        checker, [str(PROJECT_ROOT / "src")], "Found .cpp file includes"
    )


if __name__ == "__main__":
    main()
