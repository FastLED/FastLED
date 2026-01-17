#!/usr/bin/env python3
"""
Checker for logging macros in FL_IRAM functions.

FL_IRAM functions execute in IRAM (Instruction RAM) and are typically interrupt service
routines (ISRs). Logging macros cannot be used in ISRs because they:
1. Call functions that may not be in IRAM (causing crashes)
2. Use heap allocation (unsafe in ISR context)
3. Access peripherals that may be in use by main code

Banned macros:
- FL_WARN, FL_ERROR, FL_DBG, FL_ASSERT
- FL_LOG_* (all variants except FL_LOG_*ASYNC*)
"""

import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


# List of banned logging macros that cannot be used in FL_IRAM functions
# Note: FL_LOG_* pattern excludes FL_LOG_*ASYNC* variants (those are allowed)
BANNED_MACROS = [
    "FL_WARN",
    "FL_ERROR",
    "FL_DBG",
    "FL_ASSERT",
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

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

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
        violations: list[tuple[int, str]] = []

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
        # - FL_OPTIMIZE_FUNCTION FL_IRAM\nstatic void func() (multi-line)
        # - FL_IRAM static bool __attribute__(...) func() (attributes)
        # Pattern explanation:
        # 1. Match FL_IRAM keyword
        # 2. Handle optional modifiers/attributes/return type after FL_IRAM (may span lines)
        # 3. Extract function name (including ClassName:: prefix for methods)
        # 4. Find opening brace
        iram_func_pattern = re.compile(
            r"FL_IRAM"  # FL_IRAM marker
            r"[\s\n]+"  # Whitespace/newlines after FL_IRAM
            r"(?:static\s+)?"  # Optional static
            r"(?:inline\s+)?"  # Optional inline
            r"(?:virtual\s+)?"  # Optional virtual
            r"(?:const\s+)?"  # Optional const
            r"(?:(\w+(?:\s*<[^>]+>)?)\s+)?"  # Optional return type (group 1)
            r"(?:__attribute__\s*\([^)]*\)\s+)?"  # Optional __attribute__(...)
            r"([\w:]+)\s*\("  # Function name (group 2) + opening paren (includes ClassName::)
            r"[^)]*\)"  # Parameters
            r"(?:\s*(?:const|override|final))?"  # Optional trailing qualifiers
            r"[^{]*\{",  # Anything up to opening brace (handles multi-line params)
            re.MULTILINE,
        )

        # Find all FL_IRAM functions
        for match in iram_func_pattern.finditer(cleaned_content):
            func_name = match.group(2)  # Function name is in group 2
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
            # Pattern 1: Explicit banned macros (FL_WARN, FL_ERROR, FL_DBG, FL_ASSERT)
            banned_pattern = re.compile(r"\b(" + "|".join(BANNED_MACROS) + r")\s*\(")

            # Pattern 2: FL_LOG_* but NOT FL_LOG_*ASYNC* variants
            # This uses a negative lookahead to exclude ASYNC variants
            fl_log_pattern = re.compile(r"\bFL_LOG_(?!.*ASYNC)\w+\s*\(")

            # Check for explicit banned macros
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

                # Store violation as (line_number, description)
                description = (
                    f"Found '{macro_name}' in FL_IRAM function '{func_name}'\n"
                    f"  Line: {original_line[:100]}\n"
                    f"  Logging macros cannot be used in ISR functions marked with FL_IRAM."
                )
                violations.append((line_number, description))

            # Check for FL_LOG_* patterns (excluding ASYNC variants)
            for log_match in fl_log_pattern.finditer(func_body):
                # Extract the full macro name (e.g., FL_LOG_CUSTOM, FL_LOG_SPI)
                macro_call = log_match.group(0)
                macro_name = macro_call.split("(")[0].strip()

                # Calculate line number for error reporting
                match_pos = func_start + log_match.start()
                line_number = cleaned_content[:match_pos].count("\n") + 1

                # Get the line content for context (from original content with comments)
                original_line = (
                    file_content.lines[line_number - 1].strip()
                    if line_number <= len(file_content.lines)
                    else ""
                )

                # Store violation as (line_number, description)
                description = (
                    f"Found '{macro_name}' in FL_IRAM function '{func_name}'\n"
                    f"  Line: {original_line[:100]}\n"
                    f"  Logging macros cannot be used in ISR functions marked with FL_IRAM."
                )
                violations.append((line_number, description))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run logging in IRAM checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = LoggingInIramChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src" / "platforms"), str(PROJECT_ROOT / "src" / "fl")],
        "Found logging macros in FL_IRAM functions",
        extensions=[".cpp", ".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
