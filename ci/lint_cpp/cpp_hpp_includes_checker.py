#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure *.cpp.hpp files are only included by _build.* or build files.

*.cpp.hpp files are implementation files that should ONLY be included by _build.* files
(in src/) or not at all (in tests/). Regular .h, .hpp, and .cpp files should NEVER
include *.cpp.hpp files. Tests should include the public .h headers instead.

This is a hard ban — no opt-out pragma is supported.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"
TESTS_ROOT = PROJECT_ROOT / "tests"

# Pre-compiled regex — shared by both scopes
_CPP_HPP_INCLUDE_PATTERN = re.compile(r'#\s*include\s+[<"]([^>"]+\.cpp\.hpp)[>"]')


class CppHppIncludesChecker(FileContentChecker):
    """Checker that bans *.cpp.hpp includes in src/ (except _build/build files) and tests/. No opt-out."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        normalized = file_path.replace("\\", "/")

        is_src = normalized.startswith(str(SRC_ROOT).replace("\\", "/") + "/")
        is_tests = normalized.startswith(str(TESTS_ROOT).replace("\\", "/") + "/")

        if not is_src and not is_tests:
            return False

        # Check C++ file extensions
        if is_src and not normalized.endswith((".cpp", ".h", ".hpp", ".cpp.hpp")):
            return False
        if is_tests and not normalized.endswith((".cpp", ".h", ".hpp")):
            return False

        # src/ exclusions: build files are ALLOWED to include .cpp.hpp
        if is_src:
            if (
                file_path.endswith("_build.hpp")
                or file_path.endswith("_build.cpp")
                or file_path.endswith("_build.cpp.hpp")
            ):
                return False

            build_dir = str(SRC_ROOT / "fl" / "build").replace("\\", "/")
            if normalized.startswith(build_dir + "/"):
                return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for includes of other *.cpp.hpp files."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False
        is_test = file_content.path.replace("\\", "/").startswith(
            str(TESTS_ROOT).replace("\\", "/") + "/"
        )

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue  # Skip the line with closing */

            if in_multiline_comment:
                continue

            if stripped.startswith("//"):
                continue

            code_part = line.split("//")[0]

            match = _CPP_HPP_INCLUDE_PATTERN.search(code_part)
            if match:
                included_file = match.group(1)
                if is_test:
                    h_header = re.sub(r"\.cpp\.hpp$", ".h", included_file)
                    h_header = re.sub(r"\.impl\.h$", ".h", h_header)
                    violations.append(
                        (
                            line_number,
                            f"{stripped} - Including *.cpp.hpp files in tests is banned (hard ban, no opt-out). "
                            f'Include the public header instead: #include "{h_header}"',
                        )
                    )
                else:
                    violations.append(
                        (
                            line_number,
                            f"{stripped} - *.cpp.hpp files should ONLY be included by _build.* files "
                            f"(hard ban, no opt-out). Found include of '{included_file}' in non-build file. "
                            f"Move this include to the appropriate _build.hpp file.",
                        )
                    )

        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run *.cpp.hpp includes checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = CppHppIncludesChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT), str(TESTS_ROOT)],
        "Found *.cpp.hpp includes in non-build files",
        extensions=[".cpp", ".h", ".hpp", ".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
