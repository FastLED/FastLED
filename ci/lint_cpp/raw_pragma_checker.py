#!/usr/bin/env python3
"""Checker for raw _Pragma() usage in C++ code.

_Pragma() is the C99/C++11 operator form of #pragma. It is not universally
supported across all compilers targeted by FastLED (some older embedded
toolchains lack it). All diagnostic pragmas should go through the portable
FL_DISABLE_WARNING_* macros defined in fl/stl/compiler_control.h.

Exception:
    compiler_control.h itself, which defines the FL_DISABLE_WARNING macros
    using _Pragma internally.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Match _Pragma( with optional whitespace
RAW_PRAGMA_PATTERN = re.compile(r"\b_Pragma\s*\(")


class RawPragmaChecker(FileContentChecker):
    """Checker that flags raw _Pragma() usage.

    Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING(<name>) / FL_DISABLE_WARNING_POP
    from fl/stl/compiler_control.h instead.
    """

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        normalized = file_path.replace("\\", "/")

        # Skip compiler_control.h — it defines the FL_DISABLE_WARNING macros using _Pragma
        if normalized.endswith("compiler_control.h"):
            return False

        # Skip third-party code — not under our control
        if "/third_party/" in normalized:
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for raw _Pragma() usage."""
        violations: list[tuple[int, str]] = []
        lines = file_content.lines
        in_multiline_comment = False

        for line_number, line in enumerate(lines, 1):
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

            if RAW_PRAGMA_PATTERN.search(line):
                violations.append(
                    (
                        line_number,
                        f"Raw _Pragma() usage: {stripped}\n"
                        f"      Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING_* / "
                        f"FL_DISABLE_WARNING_POP from fl/stl/compiler_control.h instead.\n"
                        f"      _Pragma() is not portable across all target compilers.",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []  # Violations collected internally


def main() -> None:
    """Run raw pragma checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = RawPragmaChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src"),
            str(PROJECT_ROOT / "examples"),
            str(PROJECT_ROOT / "tests"),
        ],
        "Found raw _Pragma() usage - use FL_DISABLE_WARNING macros instead",
        extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
