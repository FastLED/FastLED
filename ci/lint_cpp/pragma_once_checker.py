#!/usr/bin/env python3
"""Checker for #pragma once usage in header and source files.

Rules:
- Header files (.h, .hpp) in src/ MUST have #pragma once
- Source files (.cpp) in src/ must NOT have #pragma once
- .cpp.hpp files are excluded (unity build implementation files)
- Excludes third_party/ and platforms/ subdirectories within src/
- Only runs on src/ files (examples/tests excluded by scope)
"""

from ci.util.check_files import FileContent, FileContentChecker


class PragmaOnceChecker(FileContentChecker):
    """Checker for correct #pragma once usage in headers vs source files."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Exclude .cpp.hpp files (unity build implementation files)
        if file_path.endswith(".cpp.hpp"):
            return False
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False

        normalized = file_path.replace("\\", "/")

        # Skip third_party/ and platforms/ subdirectories
        if "/third_party/" in normalized:
            return False
        if "/platforms/" in normalized:
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file for correct #pragma once usage."""
        path = file_content.path
        has_pragma_once = "#pragma once" in file_content.content

        is_header = path.endswith((".h", ".hpp"))
        is_cpp = path.endswith(".cpp")

        if is_header and not has_pragma_once:
            self.violations.setdefault(path, []).append(
                (
                    1,
                    "Missing #pragma once directive. "
                    "Add '#pragma once' at the top of the header file.",
                )
            )
        elif is_cpp and has_pragma_once:
            # Find the line number
            for line_number, line in enumerate(file_content.lines, 1):
                if "#pragma once" in line:
                    self.violations.setdefault(path, []).append(
                        (
                            line_number,
                            "Incorrect #pragma once in .cpp file. "
                            "Remove '#pragma once' from source files.",
                        )
                    )
                    break

        return []
