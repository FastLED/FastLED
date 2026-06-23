#!/usr/bin/env python3
"""Checker for raw platform-specific #pragma directives.

FastLED wraps compiler-specific diagnostic pragmas behind portable
FL_DISABLE_WARNING_* macros defined in fl/stl/compiler_control.h.
Using raw #pragma GCC/clang/warning directives directly ties code to
a single compiler and breaks portability.

Escape hatch:
    Place FL_ALLOW_PLATFORM_PRAGMA on the line immediately before the
    raw #pragma to silence this check.  This macro is defined in
    fl/stl/compiler_control.h and expands to nothing.

Detected patterns:
    #pragma GCC diagnostic ...
    #pragma clang diagnostic ...
    #pragma warning(...)           (MSVC)
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Patterns that match raw platform-specific diagnostic pragmas
PLATFORM_PRAGMA_PATTERNS = [
    re.compile(r"^\s*#\s*pragma\s+GCC\s+diagnostic\b"),
    re.compile(r"^\s*#\s*pragma\s+clang\s+diagnostic\b"),
    re.compile(r"^\s*#\s*pragma\s+warning\s*\("),
]

ESCAPE_HATCH = "FL_ALLOW_PLATFORM_PRAGMA"


class PlatformPragmaChecker(FileContentChecker):
    """Checker that flags raw platform-specific #pragma directives.

    Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING(name) / FL_DISABLE_WARNING_POP
    from fl/stl/compiler_control.h instead.  If the FL_DISABLE_WARNING macros cannot
    express the needed pragma, place FL_ALLOW_PLATFORM_PRAGMA on the preceding line.
    """

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        normalized = file_path.replace("\\", "/")

        # Skip third-party code — not under our control
        if "/third_party/" in normalized:
            return False

        # Skip platforms/ — inherently platform-specific
        if "/platforms/" in normalized:
            return False

        # Skip compiler_control.h itself — it defines the macros
        if normalized.endswith("compiler_control.h"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for raw platform-specific pragma usage."""
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

            # Check escape hatch on the preceding line
            if line_number >= 2:
                prev_line = lines[line_number - 2]  # 0-indexed
                if ESCAPE_HATCH in prev_line:
                    continue

            # Also allow escape hatch as trailing comment on the same line
            if ESCAPE_HATCH in line:
                continue

            # Test against each platform-pragma pattern
            for pattern in PLATFORM_PRAGMA_PATTERNS:
                if pattern.search(line):
                    violations.append(
                        (
                            line_number,
                            f"Raw platform-specific pragma: {stripped}\n"
                            f"      Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING(<name>) / "
                            f"FL_DISABLE_WARNING_POP from fl/stl/compiler_control.h.\n"
                            f"      If the FL macros cannot express this pragma, place "
                            f"FL_ALLOW_PLATFORM_PRAGMA on the preceding line as an escape hatch.",
                        )
                    )
                    break  # One violation per line is enough

        if violations:
            self.violations[file_content.path] = violations

        return []  # Violations collected internally


def main() -> None:
    """Run platform pragma checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = PlatformPragmaChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src"),
            str(PROJECT_ROOT / "examples"),
            str(PROJECT_ROOT / "tests"),
        ],
        "Found raw platform-specific pragmas - use FL_DISABLE_WARNING macros instead",
        extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
