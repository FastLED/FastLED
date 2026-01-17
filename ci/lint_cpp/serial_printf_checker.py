#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure Serial.printf is not used in example files."""

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


EXAMPLES_ROOT = PROJECT_ROOT / "examples"


class SerialPrintfChecker(FileContentChecker):
    """Checker class for Serial.printf usage in example files."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for Serial.printf usage."""
        # Only check files in examples directory
        if not file_path.startswith(str(EXAMPLES_ROOT)):
            return False

        # Check file extension - .h, .cpp, .hpp, and .ino files
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for Serial.printf usage."""
        violations: list[tuple[int, str]] = []
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
                violations.append((line_number, line.strip()))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run Serial.printf checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = SerialPrintfChecker()
    run_checker_standalone(
        checker,
        [str(EXAMPLES_ROOT)],
        "Found Serial.printf usage in examples",
        extensions=[".cpp", ".h", ".hpp", ".ino"],
    )


if __name__ == "__main__":
    main()
