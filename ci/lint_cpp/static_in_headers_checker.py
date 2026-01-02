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

Exception for template functions:
    // In header.h
    template<typename T>
    const T& getSingleton() {
        static T instance;  // ✅ PASS: static in template function is okay
        return instance;
    }

    Template functions are instantiated per template parameter, so each instantiation
    gets its own static variable without __cxa_guard conflicts.
"""

# pyright: reportUnknownMemberType=false
import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


class StaticInHeaderChecker(FileContentChecker):
    """Checker for function-local static variables in header files."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

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
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False
        brace_depth = 0  # Track brace nesting level
        in_function = False  # Are we inside a function implementation?
        in_template_function = False  # Are we inside a template function?

        # Pattern to detect inline function implementations with bodies
        # We specifically look for getAll() pattern which is the problem case
        inline_func_pattern = re.compile(r"\w+\s*\([^)]*\)\s*\{")

        # Pattern to detect template declarations (template<...>)
        template_pattern = re.compile(r"^\s*template\s*<")

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

            # Detect template declaration - next function will be a template function
            if template_pattern.search(code_part):
                in_template_function = True

            # Detect entering a function body (inline implementation)
            if inline_func_pattern.search(code_part) and "{" in code_part:
                in_function = True
                brace_depth = open_braces - close_braces
            elif in_function:
                brace_depth += open_braces - close_braces

            # Exit function when braces balance
            if in_function and brace_depth <= 0:
                in_function = False
                in_template_function = False  # Reset template state when function ends
                brace_depth = 0

            # Look for "static" keyword followed by variable declaration inside functions
            # Specifically targeting patterns like: static vector<Type> var = ...
            # or: static Type var = ...
            # But NOT: static Type func() { ... } (static member functions)
            # EXCEPTION: Allow statics inside template functions (they are instantiated per template instance)
            if in_function and brace_depth > 0 and not in_template_function:
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
                    # e.g., "static void foo() {" or "static fl::size foo() {" should not be flagged
                    # Updated to handle namespace-qualified types like fl::size
                    if not re.search(
                        r"static\s+[\w:]+(?:<[^>]+>)?\s+\w+\s*\([^)]*\)\s*\{", code_part
                    ):
                        # Allow suppression
                        if "// okay static in header" not in line:
                            violations.append((line_number, stripped))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run static in headers checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = StaticInHeaderChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found function-local static variables in headers",
        extensions=[".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
