#!/usr/bin/env python3
"""Generalized checker to ensure C++ standard attributes use FL_* macros.

Checks for C++11/14/17/20 standard attributes and ensures they use
FastLED's portable FL_* macro wrappers for better compiler compatibility.

Standard attributes checked:
- [[maybe_unused]] (C++17) -> FL_MAYBE_UNUSED
- [[nodiscard]] (C++17) -> FL_NODISCARD
- [[fallthrough]] (C++17) -> FL_FALLTHROUGH
- [[deprecated]] (C++14) -> FL_DEPRECATED
- [[noreturn]] (C++11) -> FL_NORETURN
- [[likely]] (C++20) -> FL_LIKELY
- [[unlikely]] (C++20) -> FL_UNLIKELY
- [[no_unique_address]] (C++20) -> FL_NO_UNIQUE_ADDRESS
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Mapping of C++ standard attributes to their FL_* macro equivalents
ATTRIBUTE_MAPPINGS = {
    "maybe_unused": "FL_MAYBE_UNUSED",
    "nodiscard": "FL_NODISCARD",
    "fallthrough": "FL_FALLTHROUGH",
    "deprecated": "FL_DEPRECATED",
    "noreturn": "FL_NORETURN",
    "likely": "FL_LIKELY",
    "unlikely": "FL_UNLIKELY",
    "no_unique_address": "FL_NO_UNIQUE_ADDRESS",
}

# Pattern to match any C++ standard attribute: [[attribute_name]]
ATTRIBUTE_PATTERN = re.compile(r"\[\[\s*([a-z_]+)\s*\]\]")


class AttributeChecker(FileContentChecker):
    """Checker for C++ standard attributes - should use FL_* macros instead."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Check C++ file extensions
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        # Skip the file that defines FL_* macros
        if file_path.endswith("compiler_control.h"):
            return False

        # Skip third-party libraries (they define their own attributes)
        if "/third_party/" in file_path.replace("\\", "/"):
            return False

        # Skip external test frameworks (doctest.h, etc.)
        if file_path.endswith(("doctest.h", "catch.hpp", "gtest.h")):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for C++ standard attribute usage."""
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

            # Skip lines that define FL_* macros
            if "#define" in code_part and "FL_" in code_part:
                continue

            # Check for C++ standard attributes
            for match in ATTRIBUTE_PATTERN.finditer(code_part):
                attr_name = match.group(1)

                # Check if this is a known standard attribute
                if attr_name in ATTRIBUTE_MAPPINGS:
                    fl_macro = ATTRIBUTE_MAPPINGS[attr_name]
                    violations.append(
                        (
                            line_number,
                            f"Use {fl_macro} instead of [[{attr_name}]]: {stripped}",
                        )
                    )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run attribute checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = AttributeChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src"),
            str(PROJECT_ROOT / "examples"),
            str(PROJECT_ROOT / "tests"),
        ],
        "Found C++ standard attributes - use FL_* macros instead",
        extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
