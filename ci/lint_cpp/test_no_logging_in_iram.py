#!/usr/bin/env python3
"""
Lint check: Prevent logging macros in FL_IRAM functions.

FL_IRAM functions execute in IRAM (Instruction RAM) and are typically interrupt service
routines (ISRs). Logging macros cannot be used in ISRs because they:
1. Call functions that may not be in IRAM (causing crashes)
2. Use heap allocation (unsafe in ISR context)
3. Access peripherals that may be in use by main code

Banned macros:
- FL_WARN, FL_DBG, FL_ASSERT
- FL_LOG_SPI, FL_LOG_RMT, FL_LOG_PARLIO, FL_LOG_AUDIO, FL_LOG_INTERRUPT

Example violation:
    void FL_IRAM interruptHandler(void *arg) {
        FL_WARN("Interrupt fired");  // ❌ FAIL: logging in ISR
        // ... handle interrupt
    }

Correct approach:
    void FL_IRAM interruptHandler(void *arg) {
        // ... handle interrupt without logging
        flag = true;  // ✅ PASS: set flag, check later
    }

    void checkInterruptFlag() {
        if (flag) {
            FL_WARN("Interrupt occurred");  // ✅ PASS: log outside ISR
        }
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

# List of banned logging macros that cannot be used in FL_IRAM functions
BANNED_MACROS = [
    "FL_WARN",
    "FL_DBG",
    "FL_ASSERT",
    "FL_LOG_SPI",
    "FL_LOG_RMT",
    "FL_LOG_PARLIO",
    "FL_LOG_AUDIO",
    "FL_LOG_INTERRUPT",
]


def strip_comments(content: str) -> str:
    """
    Strip C/C++ comments from content while preserving line structure.

    Args:
        content: The content to strip comments from

    Returns:
        Content with comments removed, preserving line numbers
    """
    # Remove multi-line comments (/* ... */)
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    # Remove single-line comments (// ...)
    content = re.sub(r"//.*$", "", content, flags=re.MULTILINE)
    return content


class LoggingInIramChecker(FileContentChecker):
    """Checker for logging macros in FL_IRAM functions."""

    def should_process_file(self, file_path: str) -> bool:
        """Only process C++ files in critical directories."""
        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        # Only check .cpp and .h files
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False

        # Only check critical directories
        critical_dirs = ["/platforms/", "/fl/"]
        if not any(
            critical_dir in file_path.replace("\\", "/")
            for critical_dir in critical_dirs
        ):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check for logging macros in FL_IRAM functions."""
        failings: list[str] = []

        # Join lines and strip comments for parsing
        full_content = "\n".join(file_content.lines)

        # Quick first pass: skip files without FL_IRAM
        if "FL_IRAM" not in full_content:
            return []

        cleaned_content = strip_comments(full_content)

        # Find all FL_IRAM function definitions
        # Pattern matches various forms:
        # - void FL_IRAM func()
        # - FL_IRAM void func()
        # - static void FL_IRAM func()
        # - static FL_IRAM void func()
        # - template<T> T FL_IRAM func()
        iram_func_pattern = re.compile(
            r"(?:(?:static|inline|virtual|template\s*<[^>]+>)\s+)*"  # Optional modifiers/template
            r"(?:(\w+(?:\s*<[^>]+>)?)\s+FL_IRAM|FL_IRAM\s+(\w+(?:\s*<[^>]+>)?))"  # FL_IRAM + return type (either order)
            r"\s+(\w+)\s*\([^)]*\)"  # Function name + parameters
            r"(?:\s*(?:const|override|final))?"  # Optional trailing qualifiers
            r"\s*\{",  # Opening brace
            re.MULTILINE,
        )

        # Find all FL_IRAM functions
        for match in iram_func_pattern.finditer(cleaned_content):
            func_name = match.group(3)  # Function name is always in group 3
            func_start = match.end()  # Start after the opening brace

            # Find the function body by tracking brace depth
            brace_depth = 1
            func_body_end = func_start
            for i in range(func_start, len(cleaned_content)):
                if cleaned_content[i] == "{":
                    brace_depth += 1
                elif cleaned_content[i] == "}":
                    brace_depth -= 1
                    if brace_depth == 0:
                        func_body_end = i
                        break

            # Extract function body
            func_body = cleaned_content[func_start:func_body_end]

            # Search for banned logging macros in the function body
            banned_pattern = re.compile(r"\b(" + "|".join(BANNED_MACROS) + r")\s*\(")
            for macro_match in banned_pattern.finditer(func_body):
                macro_name = macro_match.group(1)

                # Calculate line number for error reporting
                # Count newlines from start of file to the match position
                match_pos = func_start + macro_match.start()
                line_number = cleaned_content[:match_pos].count("\n") + 1

                # Get the line content for context (from original content with comments)
                original_line = (
                    file_content.lines[line_number - 1].strip()
                    if line_number <= len(file_content.lines)
                    else ""
                )

                failings.append(
                    f"Found '{macro_name}' in FL_IRAM function '{func_name}' at "
                    f"{file_content.path}:{line_number}\n"
                    f"  Line: {original_line[:100]}\n"
                    f"  Logging macros cannot be used in ISR functions marked with FL_IRAM."
                )

        return failings


def _check_logging_in_iram(test_directories: list[str]) -> list[str]:
    """Check for logging macros in FL_IRAM functions."""
    # Collect files to check
    files_to_check = collect_files_to_check(test_directories)

    # Create processor and checker
    processor = MultiCheckerFileProcessor()
    checker = LoggingInIramChecker()

    # Process files
    results = processor.process_files_with_checkers(files_to_check, [checker])

    # Get results
    all_failings = results.get("LoggingInIramChecker", []) or []

    return all_failings


class TestNoLoggingInIram(unittest.TestCase):
    """Unit tests for logging-in-IRAM linter."""

    def test_pattern_detection(self) -> None:
        """Test that the checker correctly detects various FL_IRAM and logging patterns."""
        checker = LoggingInIramChecker()

        # Test case 1: FL_IRAM function with FL_WARN (should fail)
        test_code_1 = """
void FL_IRAM handler() {
    FL_WARN("Error");
}
"""
        file_content_1 = FileContent("test_file.cpp", test_code_1, [])
        failings_1 = checker.check_file_content(file_content_1)
        self.assertTrue(
            len(failings_1) > 0, "Should detect FL_WARN in FL_IRAM function"
        )
        self.assertIn("FL_WARN", failings_1[0])
        self.assertIn("handler", failings_1[0])

        # Test case 2: FL_IRAM function without logging (should pass)
        test_code_2 = """
void FL_IRAM handler() {
    int x = 5;
}
"""
        file_content_2 = FileContent("test_file.cpp", test_code_2, [])
        failings_2 = checker.check_file_content(file_content_2)
        self.assertEqual(
            len(failings_2), 0, "Should NOT flag FL_IRAM function without logging"
        )

        # Test case 3: Non-IRAM function with logging (should pass)
        test_code_3 = """
void normal_function() {
    FL_WARN("This is fine");
}
"""
        file_content_3 = FileContent("test_file.cpp", test_code_3, [])
        failings_3 = checker.check_file_content(file_content_3)
        self.assertEqual(
            len(failings_3), 0, "Should NOT flag non-IRAM function with logging"
        )

        # Test case 4: Static FL_IRAM function with FL_DBG (should fail)
        test_code_4 = """
static void FL_IRAM interrupt_handler() {
    FL_DBG("Debug");
}
"""
        file_content_4 = FileContent("test_file.cpp", test_code_4, [])
        failings_4 = checker.check_file_content(file_content_4)
        self.assertTrue(
            len(failings_4) > 0, "Should detect FL_DBG in static FL_IRAM function"
        )

        # Test case 5: FL_IRAM with FL_LOG_INTERRUPT (should fail)
        test_code_5 = """
void FL_IRAM isr() {
    FL_LOG_INTERRUPT("Fired");
}
"""
        file_content_5 = FileContent("test_file.cpp", test_code_5, [])
        failings_5 = checker.check_file_content(file_content_5)
        self.assertTrue(
            len(failings_5) > 0, "Should detect FL_LOG_INTERRUPT in FL_IRAM function"
        )

        # Test case 6: Return type before FL_IRAM
        test_code_6 = """
uint32_t FL_IRAM getData() {
    FL_ASSERT(false);
    return 0;
}
"""
        file_content_6 = FileContent("test_file.cpp", test_code_6, [])
        failings_6 = checker.check_file_content(file_content_6)
        self.assertTrue(
            len(failings_6) > 0, "Should detect FL_ASSERT in FL_IRAM function"
        )

        # Test case 7: Multiple violations in same function
        test_code_7 = """
void FL_IRAM multi() {
    FL_WARN("First");
    int x = 5;
    FL_DBG("Second");
}
"""
        file_content_7 = FileContent("test_file.cpp", test_code_7, [])
        failings_7 = checker.check_file_content(file_content_7)
        self.assertEqual(
            len(failings_7), 2, "Should detect multiple violations in same function"
        )

        # Test case 8: Early exit optimization - no FL_IRAM in file
        test_code_8 = """
void normal_function() {
    FL_WARN("Lots of logging");
    FL_DBG("More logging");
    FL_LOG_INTERRUPT("Even more");
}

void another_function() {
    FL_ASSERT(true);
}
"""
        file_content_8 = FileContent("test_file.cpp", test_code_8, [])
        failings_8 = checker.check_file_content(file_content_8)
        self.assertEqual(
            len(failings_8), 0, "Should skip files without FL_IRAM (early exit)"
        )

    def test_no_logging_in_iram_functions(self) -> None:
        """Check critical directories for logging macros in FL_IRAM functions.

        Critical directories where FL_IRAM is used:
        - src/platforms: Platform-specific hardware interfaces and ISR handlers
        - src/fl: FastLED core library namespace
        """
        test_directories = [
            str(SRC_ROOT / "platforms"),
            str(SRC_ROOT / "fl"),
        ]

        failings = _check_logging_in_iram(test_directories)

        if failings:
            banned_list = ", ".join(BANNED_MACROS)
            msg = (
                f"Found {len(failings)} logging macro(s) in FL_IRAM functions:\n\n"
                + "\n".join(failings)
                + "\n\n"
                "REASON: FL_IRAM functions execute in IRAM and are typically interrupt service "
                "routines (ISRs). Logging macros cannot be used in ISRs because:\n"
                "  1. They call functions that may not be in IRAM (causing crashes)\n"
                "  2. They use heap allocation (unsafe in ISR context)\n"
                "  3. They access peripherals that may be in use by main code\n\n"
                f"BANNED MACROS: {banned_list}\n\n"
                "SOLUTION: Remove logging from FL_IRAM functions:\n"
                "  1. Use volatile flags to signal events from ISR\n"
                "  2. Check flags in main code and log there\n"
                "  3. For debugging, use hardware-specific debug mechanisms\n\n"
                "STRICT ENFORCEMENT: No suppression comments allowed - logging in ISRs "
                "is always incorrect and will cause crashes or undefined behavior."
            )
            self.fail(msg)


if __name__ == "__main__":
    unittest.main()
