#!/usr/bin/env python3
"""Checker to ensure __attribute__((weak)) is replaced with FL_LINK_WEAK macro."""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Pattern to match __attribute__((weak)) but not FL_LINK_WEAK definition
WEAK_ATTR_PATTERN = re.compile(r"__attribute__\s*\(\s*\(\s*weak\s*\)\s*\)")


class WeakAttributeChecker(FileContentChecker):
    """Checker for raw __attribute__((weak)) usage - should use FL_LINK_WEAK instead."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Check C++ file extensions
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        # Skip the file that defines FL_LINK_WEAK
        if file_path.endswith("compiler_control.h"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for raw __attribute__((weak)) usage."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            # Skip if inside multi-line comment
            if in_multiline_comment:
                continue

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion before checking
            code_part = line.split("//")[0]

            # Skip lines that define FL_LINK_WEAK macro
            if "#define" in code_part and "FL_LINK_WEAK" in code_part:
                continue

            # Check for __attribute__((weak)) pattern
            if WEAK_ATTR_PATTERN.search(code_part):
                violations.append(
                    (
                        line_number,
                        f"Use FL_LINK_WEAK instead of __attribute__((weak)): {stripped}",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run weak attribute checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = WeakAttributeChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src"), str(PROJECT_ROOT / "examples")],
        "Found __attribute__((weak)) - use FL_LINK_WEAK instead",
        extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
