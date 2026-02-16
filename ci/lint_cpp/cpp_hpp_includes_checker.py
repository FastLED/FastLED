#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure *.cpp.hpp files are only included by _build.hpp files.

*.cpp.hpp files are implementation files that should ONLY be included by _build.hpp files.
Regular .h, .hpp, and .cpp files should NEVER include *.cpp.hpp files.
This ensures proper separation between interface (.h/.hpp) and implementation (.cpp.hpp).
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"


class CppHppIncludesChecker(FileContentChecker):
    """Checker class to ensure *.cpp.hpp files are only included by _build.hpp files."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Only check files in src/ directory
        if not file_path.startswith(str(SRC_ROOT)):
            return False

        # Check all C++ source and header files (including .cpp.hpp)
        if not file_path.endswith((".cpp", ".h", ".hpp", ".cpp.hpp")):
            return False

        # _build.hpp, _build.cpp, and _build.cpp.hpp files are ALLOWED to include .cpp.hpp files (that's their purpose)
        if (
            file_path.endswith("_build.hpp")
            or file_path.endswith("_build.cpp")
            or file_path.endswith("_build.cpp.hpp")
        ):
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

        # Pattern to match opt-out pragma (without the // prefix since we split it off)
        opt_out_pattern = re.compile(r"\s*ok\s+include\s+cpp\.hpp")

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

            # Check for *.cpp.hpp includes in code portion
            match = cpp_hpp_include_pattern.search(code_part)
            if match:
                # Check if line has opt-out pragma
                if opt_out_pattern.search(comment_part):
                    continue  # Skip this violation due to opt-out

                included_file = match.group(1)
                violations.append(
                    (
                        line_number,
                        f"{line.strip()} - *.cpp.hpp files should ONLY be included by _build.hpp files. "
                        f"Found include of '{included_file}' in non-build file. "
                        f"Move this include to the appropriate _build.hpp file.",
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
        "Found *.cpp.hpp includes in non-_build.hpp files (*.cpp.hpp should only be included by _build.hpp)",
        extensions=[".cpp", ".h", ".hpp", ".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
