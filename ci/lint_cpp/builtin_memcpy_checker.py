#!/usr/bin/env python3
"""Checker for raw __builtin_memcpy usage in C/C++ files.

FastLED wraps __builtin_memcpy in FL_BUILTIN_MEMCPY (defined in
fl/stl/compiler_control.h) so that call sites are portable and
the project can swap implementations in one place if needed.

Banned usage:
- __builtin_memcpy(...) -> FL_BUILTIN_MEMCPY(...)
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Pattern to match __builtin_memcpy(...) calls
BUILTIN_MEMCPY_PATTERN = re.compile(r"\b__builtin_memcpy\s*\(")


class BuiltinMemcpyChecker(FileContentChecker):
    """Checker for raw __builtin_memcpy — use FL_BUILTIN_MEMCPY instead."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        # Skip the file that defines FL_BUILTIN_MEMCPY
        if file_path.endswith("compiler_control.h"):
            return False

        # Skip third-party code
        if "/third_party/" in file_path.replace("\\", "/"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for raw __builtin_memcpy usage."""
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

            if in_multiline_comment:
                continue

            if stripped.startswith("//"):
                continue

            code_part = line.split("//")[0]

            # Skip #define lines that define the macro wrapper itself
            if "#define" in code_part and "FL_BUILTIN_MEMCPY" in code_part:
                continue

            # Fast check before regex
            if "__builtin_memcpy" not in code_part:
                continue

            if BUILTIN_MEMCPY_PATTERN.search(code_part):
                violations.append(
                    (
                        line_number,
                        f"Use FL_BUILTIN_MEMCPY instead of __builtin_memcpy: {stripped}\n"
                        f"      Rationale: FL_BUILTIN_MEMCPY wraps __builtin_memcpy for portability.\n"
                        f"      Include 'fl/stl/compiler_control.h' and replace __builtin_memcpy(...) with FL_BUILTIN_MEMCPY(...)",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run builtin_memcpy checker standalone."""
    import sys
    from pathlib import Path

    from ci.util.check_files import (
        MultiCheckerFileProcessor,
        run_checker_standalone,
    )

    if len(sys.argv) > 1:
        file_path = Path(sys.argv[1]).resolve()
        if not file_path.exists():
            print(f"Error: File not found: {file_path}")
            sys.exit(1)

        checker = BuiltinMemcpyChecker()
        processor = MultiCheckerFileProcessor()
        processor.process_files_with_checkers([str(file_path)], [checker])

        if hasattr(checker, "violations") and checker.violations:
            violations = checker.violations
            print(
                f"❌ Found raw __builtin_memcpy usage — use FL_BUILTIN_MEMCPY instead ({len(violations)} file(s)):\n"
            )
            for file_path_str, violation_list in violations.items():
                rel_path = Path(file_path_str).relative_to(PROJECT_ROOT)
                print(f"{rel_path}:")
                for line_num, message in violation_list:
                    print(f"  Line {line_num}: {message}")
            sys.exit(1)
        else:
            print("✅ No raw __builtin_memcpy usage found.")
            sys.exit(0)
    else:
        checker = BuiltinMemcpyChecker()
        run_checker_standalone(
            checker,
            [
                str(PROJECT_ROOT / "src"),
                str(PROJECT_ROOT / "examples"),
                str(PROJECT_ROOT / "tests"),
            ],
            "Found raw __builtin_memcpy usage — use FL_BUILTIN_MEMCPY instead",
            extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
        )


if __name__ == "__main__":
    main()
