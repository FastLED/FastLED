#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for internal headers using FastLED.h instead of fl/fastled.h."""

import os
import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_DIR = str(PROJECT_ROOT / "src").replace("\\", "/")

# Pre-compiled patterns
_FASTLED_H_INCLUDE = re.compile(r'^\s*#\s*include\s+[<"]FastLED\.h[>"]')
_EXEMPTION_COMMENT = re.compile(r"//\s*ok\s+include", re.IGNORECASE)
_FASTLED_INTERNAL_DEFINE = re.compile(r"^\s*#\s*define\s+FASTLED_INTERNAL")

# Files that are exempt (public headers)
_EXEMPT_FILENAMES = {"FastLED.h", "fastspi.h"}


class FastLEDHeaderUsageChecker(FileContentChecker):
    """Checker for internal headers that should use fl/fastled.h instead of FastLED.h."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process header files in src/ (not examples)."""
        normalized = file_path.replace("\\", "/")
        if not normalized.startswith(SRC_DIR + "/"):
            return False
        if not file_path.endswith((".h", ".hpp")):
            return False
        # Skip public headers
        basename = os.path.basename(file_path)
        if basename in _EXEMPT_FILENAMES:
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check for FastLED.h usage in internal headers."""
        violations: list[tuple[int, str]] = []
        lines = file_content.lines

        for line_number, line in enumerate(lines, 1):
            match = _FASTLED_H_INCLUDE.match(line)
            if not match:
                continue

            # Check for exemption comment on same line
            if _EXEMPTION_COMMENT.search(line):
                continue

            # Check if FASTLED_INTERNAL was defined within the previous 5 lines
            has_internal_define = False
            lookback = min(5, line_number - 1)
            for i in range(1, lookback + 1):
                prev_line = lines[line_number - i - 1]
                if _FASTLED_INTERNAL_DEFINE.match(prev_line):
                    has_internal_define = True
                    break

            if not has_internal_define:
                violations.append(
                    (
                        line_number,
                        f"Use 'fl/fastled.h' instead of 'FastLED.h': {line.strip()}",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []
