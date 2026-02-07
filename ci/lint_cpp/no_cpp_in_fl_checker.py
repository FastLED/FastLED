#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to enforce unity build: no .cpp files in src/fl/ except _build.cpp.

All implementation files must use .cpp.hpp extension.

Allowed:
    src/fl/_build.cpp             - Master unity build file
    src/fl/**/*.cpp.hpp           - Includable implementation files
    src/fl/**/*.h                 - Header files

Forbidden:
    src/fl/async.cpp              - Should be async.cpp.hpp
    src/fl/channels/channel.cpp   - Should be channel.cpp.hpp
"""

import os

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


FL_DIR = str(PROJECT_ROOT / "src" / "fl").replace("\\", "/")


class NoCppInFlChecker(FileContentChecker):
    """Checker that flags .cpp files in src/fl/ except _build.cpp."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process .cpp files in src/fl/ that aren't _build.cpp."""
        normalized = file_path.replace("\\", "/")
        if not normalized.startswith(FL_DIR + "/"):
            return False
        if not normalized.endswith(".cpp"):
            return False
        # Allow _build.cpp
        if os.path.basename(normalized) == "_build.cpp":
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Flag the file as a violation - it shouldn't be .cpp."""
        rel_path = file_content.path.replace("\\", "/")
        try:
            idx = rel_path.index("src/fl/")
            rel_path = rel_path[idx:]
        except ValueError:
            pass
        self.violations[file_content.path] = [
            (0, f"{rel_path} should use .cpp.hpp extension (unity build)")
        ]
        return []
