#!/usr/bin/env python3
"""Checker to ensure every *.cpp.hpp in src/fl/ has a corresponding *.h header.

Every implementation file (*.cpp.hpp) that is not a _build file should have a
corresponding public header (*.h) in the same directory. For example:
    src/fl/async.cpp.hpp  ->  src/fl/async.h
    src/fl/gfx/blur.cpp.hpp  ->  src/fl/gfx/blur.h

This catches orphaned implementation files that lost their header during moves
or refactors.
"""

from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


FL_ROOT = PROJECT_ROOT / "src" / "fl"


class CppHppHeaderPairChecker(FileContentChecker):
    """Checks that *.cpp.hpp files in src/fl/ have a corresponding *.h file."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process *.cpp.hpp files in src/fl/ (excluding _build files)."""
        if not file_path.endswith(".cpp.hpp"):
            return False

        path = Path(file_path)

        # Must be under src/fl/
        try:
            path.relative_to(FL_ROOT)
        except ValueError:
            return False

        # Skip _build.cpp.hpp files
        if path.name.startswith("_build"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check if a corresponding .h file exists."""
        # Check for suppression comment in first 5 lines
        for line in file_content.lines[:5]:
            if "// ok no header" in line:
                return []

        cpp_hpp_path = Path(file_content.path)

        # foo.cpp.hpp -> foo.h (strip .cpp.hpp, add .h)
        stem = cpp_hpp_path.name.removesuffix(".cpp.hpp")
        expected_header = cpp_hpp_path.parent / f"{stem}.h"

        if not expected_header.exists():
            expected_rel = expected_header.relative_to(PROJECT_ROOT)
            self.violations[file_content.path] = [
                (
                    1,
                    f"No corresponding header found: expected {expected_rel}",
                )
            ]

        return []


def main() -> None:
    """Run checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = CppHppHeaderPairChecker()
    run_checker_standalone(
        checker,
        [str(FL_ROOT)],
        "Found *.cpp.hpp files without corresponding *.h headers",
        extensions=[".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
