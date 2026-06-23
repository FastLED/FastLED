#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure 'namespace platforms' (plural) is used instead of 'namespace platform' (singular)."""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"


class NamespacePlatformsChecker(FileContentChecker):
    """Checker class for namespace platforms naming convention."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for namespace convention."""
        # Only check files in src/platforms directory
        if not file_path.startswith(str(SRC_ROOT / "platforms")):
            return False

        # Check file extension - .h, .cpp, .hpp files
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for incorrect 'namespace platform' usage."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Regex to match 'namespace platform' but NOT 'namespace platforms'
        # Word boundary ensures we don't match 'namespace platforms'
        pattern = re.compile(r"\bnamespace\s+platform\s*\{")

        # Check each line for incorrect namespace usage
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

            # Check for 'namespace platform' (singular) in code portion only
            if pattern.search(code_part):
                violations.append((line_number, line.strip()))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run namespace platforms checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = NamespacePlatformsChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT / "platforms")],
        "Found 'namespace platform' (singular) - use 'namespace platforms' (plural) instead",
        extensions=[".cpp", ".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
