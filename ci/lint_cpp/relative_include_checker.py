#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for relative includes (paths containing '..')."""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_DIR = str(PROJECT_ROOT / "src").replace("\\", "/")

# Pre-compiled regex for #include with ".." in the path
_RELATIVE_INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s+[<"]([^>"]*\.\..*)[>"]')

# Files that are allowed to use relative includes (e.g., platform runners that need tests/ headers)
_ALLOWED_RELATIVE_INCLUDE_FILES = {
    "src/platforms/win/run_example.hpp",
    "src/platforms/posix/run_example.hpp",
    "src/platforms/apple/run_example.hpp",
}


class RelativeIncludeChecker(FileContentChecker):
    """Checker for relative includes containing '..' in src/ files."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process C++ files in src/."""
        normalized = file_path.replace("\\", "/")
        if not normalized.startswith(SRC_DIR + "/"):
            return False
        return file_path.endswith((".cpp", ".h", ".hpp", ".cc", ".cxx", ".ino"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check for relative includes containing '..'."""
        # Skip files that are explicitly allowed to use relative includes
        normalized_path = file_content.path.replace("\\", "/")
        if any(
            normalized_path.endswith(allowed_file)
            for allowed_file in _ALLOWED_RELATIVE_INCLUDE_FILES
        ):
            return []

        violations: list[tuple[int, str]] = []

        for line_number, line in enumerate(file_content.lines, 1):
            match = _RELATIVE_INCLUDE_PATTERN.match(line)
            if match:
                # Allow suppression with '// ok relative include' comment
                if "// ok relative include" not in line:
                    violations.append(
                        (line_number, f"Relative include: {line.strip()}")
                    )

        if violations:
            self.violations[file_content.path] = violations

        return []
