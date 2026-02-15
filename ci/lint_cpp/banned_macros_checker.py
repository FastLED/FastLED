#!/usr/bin/env python3
"""Checker for banned C/C++ preprocessor macros and features.

FastLED uses portable FL_* wrappers instead of compiler-specific or
platform-specific preprocessor features to ensure consistent behavior
across all platforms and compilers.

Banned macros/features checked:
- __has_include(...) (C++17/compiler extension) -> FL_HAS_INCLUDE(...)

Rationale:
- __has_include() is not universally supported across all compilers/platforms
- FL_HAS_INCLUDE provides a fallback implementation (returns 0) for compilers
  without __has_include support
- Ensures code compiles consistently across all target platforms
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Mapping of banned macros to their FL_* macro equivalents
MACRO_MAPPINGS = {
    "__has_include": "FL_HAS_INCLUDE",
}

# Pattern to match __has_include(...) usage
# Matches: __has_include(<header>) or __has_include("header")
HAS_INCLUDE_PATTERN = re.compile(r"\b__has_include\s*\(")


class BannedMacrosChecker(FileContentChecker):
    """Checker for banned preprocessor macros - should use FL_* wrappers instead."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Check C++ file extensions
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        # Skip the file that defines FL_HAS_INCLUDE
        if file_path.endswith("has_include.h"):
            return False

        # Skip third-party libraries (they define their own macros)
        if "/third_party/" in file_path.replace("\\", "/"):
            return False

        # Skip external test frameworks (doctest.h, etc.)
        if file_path.endswith(("doctest.h", "catch.hpp", "gtest.h")):
            return False

        # Skip documentation files
        if file_path.endswith((".md", ".txt")):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for banned preprocessor macro usage."""
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

            # Skip lines that define FL_HAS_INCLUDE macro itself
            if "#define" in code_part and "FL_HAS_INCLUDE" in code_part:
                continue

            # Check for __has_include(...) usage
            if HAS_INCLUDE_PATTERN.search(code_part):
                # Skip if it's in a #ifndef __has_include guard (definition pattern)
                # This allows: #ifndef __has_include / #define FL_HAS_INCLUDE(x) 0 / #else ...
                if "#ifndef" in code_part or "#ifdef" in code_part:
                    continue

                violations.append(
                    (
                        line_number,
                        f"Use FL_HAS_INCLUDE instead of __has_include: {stripped}\n"
                        f"      Rationale: __has_include is not universally supported. "
                        f"FL_HAS_INCLUDE provides a portable wrapper with fallback for older compilers.\n"
                        f"      Include 'fl/has_include.h' and replace __has_include(...) with FL_HAS_INCLUDE(...)",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run banned macros checker standalone."""
    import sys
    from pathlib import Path

    from ci.util.check_files import (
        MultiCheckerFileProcessor,
        collect_files_to_check,
        run_checker_standalone,
    )

    # Check if a single file path was provided as argument
    if len(sys.argv) > 1:
        # Single file mode
        file_path = Path(sys.argv[1]).resolve()
        if not file_path.exists():
            print(f"Error: File not found: {file_path}")
            sys.exit(1)

        checker = BannedMacrosChecker()
        processor = MultiCheckerFileProcessor()
        processor.process_files_with_checkers([str(file_path)], [checker])

        # Check for violations
        if hasattr(checker, "violations") and checker.violations:
            violations = checker.violations
            print(
                f"❌ Found banned preprocessor macros - use FL_* wrappers instead ({len(violations)} file(s)):\n"
            )
            for file_path_str, violation_list in violations.items():
                rel_path = Path(file_path_str).relative_to(PROJECT_ROOT)
                print(f"{rel_path}:")
                for line_num, message in violation_list:
                    print(f"  Line {line_num}: {message}")
            sys.exit(1)
        else:
            print("✅ No banned preprocessor macros found.")
            sys.exit(0)
    else:
        # Normal mode - scan all directories
        checker = BannedMacrosChecker()
        run_checker_standalone(
            checker,
            [
                str(PROJECT_ROOT / "src"),
                str(PROJECT_ROOT / "examples"),
                str(PROJECT_ROOT / "tests"),
            ],
            "Found banned preprocessor macros - use FL_* wrappers instead",
            extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
        )


if __name__ == "__main__":
    main()
