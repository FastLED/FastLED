#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure *.cpp.hpp files do not include other *.cpp.hpp files.

*.cpp.hpp files are implementation files that should ONLY be included by _build.hpp files.
They should never include other *.cpp.hpp files themselves.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"


class CppHppIncludesChecker(FileContentChecker):
    """Checker class for *.cpp.hpp files including other *.cpp.hpp files."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Only check *.cpp.hpp files in src/ directory
        if not file_path.startswith(str(SRC_ROOT)):
            return False

        # Only check *.cpp.hpp files (not .cpp, .hpp, or .h)
        if not file_path.endswith(".cpp.hpp"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for includes of other *.cpp.hpp files."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Pattern to match #include statements for *.cpp.hpp files
        # Matches: #include "path/to/file.cpp.hpp" or #include <path/to/file.cpp.hpp>
        cpp_hpp_include_pattern = re.compile(
            r'#\s*include\s+[<"]([^>"]+\.cpp\.hpp)[>"]'
        )

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

            # Split line to separate code from inline comments
            code_part = line.split("//")[0]
            comment_part = line.split("//", 1)[1] if "//" in line else ""

            # Check if exception comment is present
            has_exception = "ok include cpp.hpp" in comment_part.lower()

            # Check for *.cpp.hpp includes in code portion
            match = cpp_hpp_include_pattern.search(code_part)
            if match and not has_exception:
                violations.append(
                    (
                        line_number,
                        f"{line.strip()} - *.cpp.hpp files should only be included by _build.hpp files, not by other *.cpp.hpp files. "
                        f"Move this include to the appropriate _build.hpp file. "
                        f"Add '// ok include cpp.hpp' comment to suppress.",
                    )
                )

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run *.cpp.hpp includes checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = CppHppIncludesChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT)],
        "Found *.cpp.hpp files including other *.cpp.hpp files",
        extensions=[".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
