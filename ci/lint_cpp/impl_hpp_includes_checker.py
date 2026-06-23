#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure *.impl.hpp files are only included by *.impl.cpp.hpp files.

*.impl.hpp files are platform-specific implementation fragments that should ONLY
be included by router files (*.impl.cpp.hpp). Regular .h, .hpp, .cpp, and .cpp.hpp
files should NEVER include *.impl.hpp files directly.

This is a strict rule with no suppression mechanism.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"


class ImplHppIncludesChecker(FileContentChecker):
    """Checker to ensure *.impl.hpp files are only included by *.impl.cpp.hpp files."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Only check files in src/ and tests/ directories
        normalized = file_path.replace("\\", "/")
        if "/src/" not in normalized and "/tests/" not in normalized:
            return False

        # Check all C++ source and header files
        if not file_path.endswith((".cpp", ".h", ".hpp", ".cpp.hpp", ".impl.cpp.hpp")):
            return False

        # *.impl.cpp.hpp router files ARE allowed to include *.impl.hpp
        if file_path.endswith(".impl.cpp.hpp"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for includes of *.impl.hpp files."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Match #include "path/to/file.impl.hpp" or #include <path/to/file.impl.hpp>
        impl_hpp_include_pattern = re.compile(
            r'#\s*include\s+[<"]([^>"]+\.impl\.hpp)[>"]'
        )

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

            if stripped.startswith("//"):
                continue

            # Check code portion only (before any inline comment)
            code_part = line.split("//")[0]

            match = impl_hpp_include_pattern.search(code_part)
            if match:
                included_file = match.group(1)
                violations.append(
                    (
                        line_number,
                        f"{stripped} - *.impl.hpp files must ONLY be included by "
                        f"*.impl.cpp.hpp router files. Found include of "
                        f"'{included_file}' in non-router file.",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []  # Collect violations internally


def main() -> None:
    """Run *.impl.hpp includes checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = ImplHppIncludesChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT), str(PROJECT_ROOT / "tests")],
        "Found *.impl.hpp includes in non-router files (*.impl.hpp should only be included by *.impl.cpp.hpp)",
        extensions=[".cpp", ".h", ".hpp", ".cpp.hpp", ".impl.cpp.hpp"],
    )


if __name__ == "__main__":
    main()
