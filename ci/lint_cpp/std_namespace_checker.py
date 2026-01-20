#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for std:: namespace usage - should use fl:: instead."""

from ci.util.check_files import FileContent, FileContentChecker, is_excluded_file


class StdNamespaceChecker(FileContentChecker):
    """Checker class for std:: namespace usage."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for std:: namespace usage."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list (handles both Windows and Unix paths)
        if is_excluded_file(file_path):
            return False

        # Exclude third-party libraries
        if "third_party" in file_path or "thirdparty" in file_path:
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for std:: namespace usage."""
        violations: list[tuple[int, str]] = []
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
                violations.append((line_number, stripped))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run std:: namespace checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = StdNamespaceChecker()
    run_checker_standalone(
        checker, [str(PROJECT_ROOT / "src")], "Found std:: namespace usage"
    )


if __name__ == "__main__":
    main()
