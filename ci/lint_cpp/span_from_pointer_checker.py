#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for unnecessary span<T>(ptr, size) construction from containers.

fl::span has container constructors that accept vectors and other containers
directly. Writing span<T>(vec.data(), vec.size()) is verbose and unnecessary
when span<T>(vec) works.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker


# Pattern: span<...>( ... .data() ... .size() ... )
# Matches things like:
#   fl::span<const int>(vec.data(), vec.size())
#   span<CRGB>(mBuffer.data(), mBuffer.size())
_SPAN_DATA_SIZE_RE = re.compile(
    r"span<[^>]+>\s*\("  # span<T>(
    r"[^)]*\.data\(\)"  # something.data()
    r"[^)]*\.size\(\)"  # something.size()
    r"[^)]*\)"  # closing paren
)

# Suppression comment
_SUPPRESSION = "// ok span from pointer"


class SpanFromPointerChecker(FileContentChecker):
    """Checker for span<T>(container.data(), container.size()) patterns.

    These should be simplified to span<T>(container) since fl::span has
    container constructors.
    """

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        # Exclude third-party code
        if "third_party" in file_path or "thirdparty" in file_path:
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for span<T>(container.data(), container.size()) patterns."""
        # Quick check: skip files that don't mention span at all
        full_text = file_content.content
        if "span<" not in full_text and "span <" not in full_text:
            return []

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

            if in_multiline_comment:
                continue

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Check for suppression comment
            if _SUPPRESSION in line:
                continue

            # Remove single-line comment portion before checking
            code_part = line.split("//")[0]

            # Quick check before regex
            if "span<" not in code_part and "span <" not in code_part:
                continue

            if ".data()" not in code_part or ".size()" not in code_part:
                continue

            # Full regex match
            if _SPAN_DATA_SIZE_RE.search(code_part):
                violations.append(
                    (
                        line_number,
                        f"{stripped}  →  Use span<T>(container) instead of span<T>(container.data(), container.size()). "
                        f"Suppress with '{_SUPPRESSION}'",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []  # Violations collected internally


def main() -> None:
    """Run span from pointer checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = SpanFromPointerChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src"), str(PROJECT_ROOT / "tests")],
        "Found span<T>(container.data(), container.size()) - use span<T>(container) instead, "
        f"or add '{_SUPPRESSION}' comment to suppress",
    )


if __name__ == "__main__":
    main()
