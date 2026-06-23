#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for reinterpret_cast usage - should use fl::bit_cast or fl::reinterpret_cast instead."""

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


class ReinterpretCastChecker(FileContentChecker):
    """Checker class for reinterpret_cast usage."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for reinterpret_cast usage."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        # Exclude third-party libraries
        if "third_party" in file_path or "thirdparty" in file_path:
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for reinterpret_cast usage."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Check each line for reinterpret_cast usage
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

            # Check for reinterpret_cast usage in code portion only
            # Allow suppression with "// ok reinterpret cast" or "// okay reinterpret cast"
            if "reinterpret_cast" in code_part:
                # Skip if it's fl::reinterpret_cast_ (which is the approved wrapper)
                if "fl::reinterpret_cast_" in code_part:
                    continue
                # Check for suppression comments
                if (
                    "// ok reinterpret cast" in line
                    or "// okay reinterpret cast" in line
                ):
                    continue
                violations.append((line_number, stripped))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run reinterpret_cast checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = ReinterpretCastChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found reinterpret_cast usage - use fl::bit_cast or fl::reinterpret_cast_<> instead, or add '// ok reinterpret cast' comment",
    )


if __name__ == "__main__":
    main()
