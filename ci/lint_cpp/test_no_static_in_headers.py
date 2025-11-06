#!/usr/bin/env python3
"""
Lint check: Prevent function-local static variables in header files.

This prevents compilation issues on platforms with older toolchains (e.g., Teensy 3.0)
that have conflicting __cxa_guard function declarations used by C++11 static initialization.

Example violation:
    // In header.h
    static const vector<Foo*>& getAll() {
        static vector<Foo*> instances = create();  // ❌ FAIL: static local in header
        return instances;
    }

Correct approach:
    // In header.h
    static const vector<Foo*>& getAll();  // ✅ PASS: declaration only

    // In source.cpp
    const vector<Foo*>& getAll() {
        static vector<Foo*> instances = create();  // ✅ PASS: static local in cpp
        return instances;
    }
"""

# pyright: reportUnknownMemberType=false
import re
import unittest

from ci.util.check_files import (
    EXCLUDED_FILES,
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"


class StaticInHeaderChecker(FileContentChecker):
    """Checker for function-local static variables in header files."""

    def should_process_file(self, file_path: str) -> bool:
        """Only process header files (.h, .hpp)."""
        # Only check header files
        if not file_path.endswith((".h", ".hpp")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check header file for function-local static variables."""
        failings: list[str] = []
        in_multiline_comment = False
        brace_depth = 0  # Track brace nesting level
        in_function = False  # Are we inside a function implementation?

        # Pattern to detect inline function implementations with bodies
        # We specifically look for getAll() pattern which is the problem case
        inline_func_pattern = re.compile(r"\w+\s*\([^)]*\)\s*\{")

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            # Skip if in multi-line comment
            if in_multiline_comment:
                continue

            # Skip single-line comments
            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion
            code_part = line.split("//")[0]

            # Track brace depth
            open_braces = code_part.count("{")
            close_braces = code_part.count("}")

            # Detect entering a function body (inline implementation)
            if inline_func_pattern.search(code_part) and "{" in code_part:
                in_function = True
                brace_depth = open_braces - close_braces
            elif in_function:
                brace_depth += open_braces - close_braces

            # Exit function when braces balance
            if in_function and brace_depth <= 0:
                in_function = False
                brace_depth = 0

            # Look for "static" keyword followed by variable declaration inside functions
            # Specifically targeting patterns like: static vector<Type> var = ...
            # or: static Type var = ...
            # But NOT: static Type func() { ... } (static member functions)
            if in_function and brace_depth > 0:
                # Match: static <type> <identifier> = or ( or {
                # But exclude lines that are just function calls to static methods
                static_var_match = re.search(
                    r"\bstatic\s+"  # "static" keyword
                    r"(?:const\s+)?"  # optional "const"
                    r"[\w:]+(?:<[^>]+>)?\s+"  # type (with optional template args)
                    r"\w+\s*"  # variable name
                    r"[=({]",  # followed by =, (, or {
                    code_part,
                )

                if static_var_match:
                    # Skip if line contains function definition pattern (to avoid false positives)
                    # e.g., "static void foo() {" should not be flagged
                    if not re.search(r"static\s+\w+\s+\w+\s*\([^)]*\)\s*\{", code_part):
                        # Allow suppression
                        if "// okay static in header" not in line:
                            failings.append(
                                f"Found function-local 'static' variable in header "
                                f"{file_content.path}:{line_number}\n"
                                f"  Line: {stripped[:100]}"
                            )

        return failings


def test_no_static_in_headers(
    test_directories: list[str],
) -> list[str]:
    """Check for function-local static variables in header files."""
    # Collect files to check
    files_to_check = collect_files_to_check(test_directories)

    # Create processor and checker
    processor = MultiCheckerFileProcessor()
    checker = StaticInHeaderChecker()

    # Process files
    results = processor.process_files_with_checkers(files_to_check, [checker])

    # Get results
    all_failings = results.get("StaticInHeaderChecker", []) or []

    return all_failings


class TestNoStaticInHeaders(unittest.TestCase):
    """Unit tests for static-in-header linter."""

    def test_no_static_locals_in_critical_dirs(self) -> None:
        """Check critical directories for function-local statics in headers.

        Critical directories that must compile on all platforms including Teensy 3.0:
        - src/platforms/shared: Platform-agnostic hardware interfaces
        - src/fl: FastLED core library namespace
        - src/fx: Effects library
        """
        test_directories = [
            str(SRC_ROOT / "platforms" / "shared"),
            str(SRC_ROOT / "fl"),
            str(SRC_ROOT / "fx"),
        ]

        failings = test_no_static_in_headers(test_directories)

        if failings:
            msg = (
                f"Found {len(failings)} function-local static variable(s) in critical headers:\n\n"
                + "\n".join(failings)
                + "\n\n"
                "REASON: Function-local static variables in headers cause compilation errors "
                "on Teensy 3.0 and other platforms with older toolchains due to conflicting "
                "__cxa_guard function declarations.\n\n"
                "SOLUTION: Move the static initialization to a .cpp file:\n"
                "  1. In header: Change inline function to declaration only\n"
                "  2. In cpp file: Add the function implementation with the static variable\n\n"
                "EXAMPLE: See spi_hw_1.h / spi_hw_1.cpp for the correct pattern.\n\n"
                "SUPPRESSION: Add '// okay static in header' comment to silence this check "
                "for specific cases (use sparingly!)."
            )
            self.fail(msg)


if __name__ == "__main__":
    unittest.main()
