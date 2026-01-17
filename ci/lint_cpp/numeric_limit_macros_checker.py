#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to ensure standard integer limit macros are not used.

Detects usage of macros like UINT32_MAX, INT16_MAX, etc. and suggests
using fl::numeric_limits<T> instead.
"""

import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


# Common integer limit macros from stdint.h and limits.h
# Pattern captures the macro name for better error messages
NUMERIC_LIMIT_MACROS = [
    # Unsigned integer max values
    "UINT8_MAX",
    "UINT16_MAX",
    "UINT32_MAX",
    "UINT64_MAX",
    "UINTMAX_MAX",
    "UINTPTR_MAX",
    "SIZE_MAX",
    # Signed integer max values
    "INT8_MAX",
    "INT16_MAX",
    "INT32_MAX",
    "INT64_MAX",
    "INTMAX_MAX",
    "INTPTR_MAX",
    # Signed integer min values
    "INT8_MIN",
    "INT16_MIN",
    "INT32_MIN",
    "INT64_MIN",
    "INTMAX_MIN",
    "INTPTR_MIN",
    # Legacy limits.h macros
    "UCHAR_MAX",
    "USHRT_MAX",
    "UINT_MAX",
    "ULONG_MAX",
    "ULLONG_MAX",
    "CHAR_MAX",
    "SHRT_MAX",
    "INT_MAX",
    "LONG_MAX",
    "LLONG_MAX",
    "CHAR_MIN",
    "SHRT_MIN",
    "INT_MIN",
    "LONG_MIN",
    "LLONG_MIN",
]


class NumericLimitMacroChecker(FileContentChecker):
    """Checker class for numeric limit macro usage."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for numeric limit macro usage."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        # Exclude test files that test the macros themselves
        if file_path.endswith(
            (
                "tests/ftl/cstdint.cpp",
                "tests\\ftl\\cstdint.cpp",
                "tests/ftl/stdint.cpp",
                "tests\\ftl\\stdint.cpp",
                "tests/ftl/rbtree.cpp",
                "tests\\ftl\\rbtree.cpp",
            )
        ):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for numeric limit macro usage."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Create a regex pattern to match any of the macros as whole words
        # Use word boundaries to avoid matching UINT32_MAX_VALUE or similar
        macro_pattern = r"\b(" + "|".join(NUMERIC_LIMIT_MACROS) + r")\b"
        macro_regex = re.compile(macro_pattern)

        # Check each line
        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue  # Skip the line with closing */

            # Skip if we're inside a multi-line comment
            if in_multiline_comment:
                continue

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion before checking
            code_part = line.split("//")[0]

            # Check for numeric limit macro usage in code portion
            # Allow suppression with special comment
            if "// okay numeric limit macro" in line:
                continue

            match = macro_regex.search(code_part)
            if match:
                macro_name = match.group(1)
                # Determine the suggested fl::numeric_limits type
                type_suggestion = self._suggest_numeric_limits_type(macro_name)
                violations.append(
                    (line_number, f"{stripped} (use {type_suggestion} instead)")
                )

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list

    def _suggest_numeric_limits_type(self, macro_name: str) -> str:
        """Suggest the appropriate fl::numeric_limits<T> replacement for a macro."""
        # Map common macros to their type equivalents
        type_map = {
            "UINT8_MAX": "fl::numeric_limits<uint8_t>::max()",
            "UINT16_MAX": "fl::numeric_limits<uint16_t>::max()",
            "UINT32_MAX": "fl::numeric_limits<uint32_t>::max()",
            "UINT64_MAX": "fl::numeric_limits<uint64_t>::max()",
            "INT8_MAX": "fl::numeric_limits<int8_t>::max()",
            "INT8_MIN": "fl::numeric_limits<int8_t>::min()",
            "INT16_MAX": "fl::numeric_limits<int16_t>::max()",
            "INT16_MIN": "fl::numeric_limits<int16_t>::min()",
            "INT32_MAX": "fl::numeric_limits<int32_t>::max()",
            "INT32_MIN": "fl::numeric_limits<int32_t>::min()",
            "INT64_MAX": "fl::numeric_limits<int64_t>::max()",
            "INT64_MIN": "fl::numeric_limits<int64_t>::min()",
            "SIZE_MAX": "fl::numeric_limits<size_t>::max()",
            "UINTMAX_MAX": "fl::numeric_limits<uintmax_t>::max()",
            "UINTPTR_MAX": "fl::numeric_limits<uintptr_t>::max()",
            "INTMAX_MAX": "fl::numeric_limits<intmax_t>::max()",
            "INTMAX_MIN": "fl::numeric_limits<intmax_t>::min()",
            "INTPTR_MAX": "fl::numeric_limits<intptr_t>::max()",
            "INTPTR_MIN": "fl::numeric_limits<intptr_t>::min()",
            # Legacy limits.h macros
            "UCHAR_MAX": "fl::numeric_limits<unsigned char>::max()",
            "USHRT_MAX": "fl::numeric_limits<unsigned short>::max()",
            "UINT_MAX": "fl::numeric_limits<unsigned int>::max()",
            "ULONG_MAX": "fl::numeric_limits<unsigned long>::max()",
            "ULLONG_MAX": "fl::numeric_limits<unsigned long long>::max()",
            "CHAR_MAX": "fl::numeric_limits<char>::max()",
            "CHAR_MIN": "fl::numeric_limits<char>::min()",
            "SHRT_MAX": "fl::numeric_limits<short>::max()",
            "SHRT_MIN": "fl::numeric_limits<short>::min()",
            "INT_MAX": "fl::numeric_limits<int>::max()",
            "INT_MIN": "fl::numeric_limits<int>::min()",
            "LONG_MAX": "fl::numeric_limits<long>::max()",
            "LONG_MIN": "fl::numeric_limits<long>::min()",
            "LLONG_MAX": "fl::numeric_limits<long long>::max()",
            "LLONG_MIN": "fl::numeric_limits<long long>::min()",
        }

        return type_map.get(macro_name, "fl::numeric_limits<T>::max/min()")


def main() -> None:
    """Run numeric limit macro checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = NumericLimitMacroChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found numeric limit macros",
        extensions=[".cpp", ".h", ".hpp", ".ino"],
    )


if __name__ == "__main__":
    main()
