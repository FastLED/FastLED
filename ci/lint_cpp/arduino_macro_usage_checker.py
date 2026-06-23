#!/usr/bin/env python3
"""Checker for banned Arduino macro usage: INPUT, OUTPUT, DEFAULT.

These macros are defined by Arduino platform headers and pollute the global
namespace. They conflict with Windows headers and other libraries.
Code should not use these macros directly.

Allowed contexts (skipped by this checker):
- Comments (single-line and multi-line)
- Preprocessor directives (#define, #undef, #ifdef, #ifndef, #pragma)
- Platform definition headers (src/platforms/)
- Third-party code (third_party/)
- Scoped enum values (e.g., SomeEnum::DEFAULT)
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Banned Arduino macros
BANNED_MACROS = ["INPUT", "OUTPUT", "DEFAULT"]

# Word-boundary patterns for each macro
MACRO_PATTERNS = {name: re.compile(rf"\b{name}\b") for name in BANNED_MACROS}

# Pattern to detect scoped enum usage like Foo::DEFAULT (not a macro)
SCOPED_ENUM_PATTERN = re.compile(r"::\s*(?:" + "|".join(BANNED_MACROS) + r")\b")

# Pattern to detect enum member definitions like "DEFAULT = 0,"
ENUM_MEMBER_PATTERN = re.compile(
    r"^\s*(?:" + "|".join(BANNED_MACROS) + r")\s*(?:=\s*\w+)?\s*[,}]"
)

# Preprocessor directive pattern
PREPROCESSOR_PATTERN = re.compile(
    r"^\s*#\s*(?:define|undef|ifdef|ifndef|if|elif|pragma|error|include)\b"
)


class ArduinoMacroUsageChecker(FileContentChecker):
    """Checker for banned Arduino macro usage (INPUT, OUTPUT, DEFAULT)."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed - only src/ files."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        normalized = file_path.replace("\\", "/")

        # Only check src/ files (handles both absolute and relative paths)
        if "/src/" not in normalized and not normalized.startswith("src/"):
            return False

        # Skip platform definition headers (they define these macros)
        if "/platforms/" in normalized or normalized.startswith("src/platforms/"):
            return False

        # Skip third-party libraries
        if "/third_party/" in normalized or normalized.startswith("src/third_party/"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for banned Arduino macro usage."""
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

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion before checking
            code_part = line.split("//")[0]

            # Skip preprocessor directives
            if PREPROCESSOR_PATTERN.match(code_part):
                continue

            # Check for each banned macro
            for name, pattern in MACRO_PATTERNS.items():
                if pattern.search(code_part):
                    # Skip scoped enum usage (e.g., RxDeviceType::DEFAULT)
                    if SCOPED_ENUM_PATTERN.search(code_part):
                        continue

                    # Skip enum member definitions (e.g., DEFAULT = 0,)
                    if ENUM_MEMBER_PATTERN.match(code_part):
                        continue

                    violations.append(
                        (
                            line_number,
                            f"Banned Arduino macro '{name}' used: {stripped}\n"
                            f"      These macros pollute the global namespace and "
                            f"conflict with Windows headers.\n"
                            f"      Use platform-specific APIs or define local "
                            f"constants instead.",
                        )
                    )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run Arduino macro usage checker standalone."""
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

        checker = ArduinoMacroUsageChecker()
        processor = MultiCheckerFileProcessor()
        processor.process_files_with_checkers([str(file_path)], [checker])

        if checker.violations:
            print(
                f"❌ Found banned Arduino macro usage ({len(checker.violations)} file(s)):\n"
            )
            for file_path_str, violation_list in checker.violations.items():
                rel_path = Path(file_path_str).relative_to(PROJECT_ROOT)
                print(f"{rel_path}:")
                for line_num, message in violation_list:
                    print(f"  Line {line_num}: {message}")
            sys.exit(1)
        else:
            print("✅ No banned Arduino macro usage found.")
            sys.exit(0)
    else:
        checker = ArduinoMacroUsageChecker()
        run_checker_standalone(
            checker,
            [
                str(PROJECT_ROOT / "src"),
            ],
            "Found banned Arduino macro usage (INPUT, OUTPUT, DEFAULT)",
            extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
        )


if __name__ == "__main__":
    main()
