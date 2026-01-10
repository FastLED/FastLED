#!/usr/bin/env python3
"""
Lint check: Prevent Google-style member variables (trailing underscore).

This project uses m-prefix camelCase for member variables, not Google-style trailing underscore.

Example violations:
    class Foo {
        int state_;           // ❌ FAIL: Google style
        bool is_ready_;       // ❌ FAIL: Google style
        uint8_t* data_ptr_;   // ❌ FAIL: Google style
    };

Correct approach:
    class Foo {
        int mState;           // ✅ PASS: m-prefix camelCase
        bool mIsReady;        // ✅ PASS: m-prefix camelCase
        uint8_t* mDataPtr;    // ✅ PASS: m-prefix camelCase
    };

The linter will suggest the correct m-prefix camelCase conversion for each violation.
"""

import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


def convert_google_to_m_prefix(var_name: str) -> str:
    """
    Convert Google-style variable name (trailing underscore) to m-prefix camelCase.

    Examples:
        state_ -> mState
        is_ready_ -> mIsReady
        variable_state_ -> mVariableState
        my_var_name_ -> mMyVarName
        fileHandle_ -> mFileHandle  (preserves existing camelCase)
        currentFrame_ -> mCurrentFrame
    """
    # Remove trailing underscore
    if not var_name.endswith("_"):
        return var_name

    base_name = var_name[:-1]

    # Check if it's already camelCase (has uppercase letters in the middle)
    # If so, just add m prefix and capitalize first letter
    if any(c.isupper() for c in base_name[1:]):
        # Already camelCase, just add m prefix and ensure first letter is uppercase
        return "m" + base_name[0].upper() + base_name[1:]

    # Otherwise, it's snake_case, so split by underscore and capitalize
    parts = base_name.split("_")

    # Capitalize each part
    capitalized_parts = [part.capitalize() for part in parts if part]

    # Join with m prefix
    return "m" + "".join(capitalized_parts)


class GoogleMemberStyleChecker(FileContentChecker):
    """Checker for Google-style member variables (trailing underscore)."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process C++ source and header files."""
        # Check file extension
        if not file_path.endswith((".h", ".hpp", ".cpp")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        # Exclude third_party directory (contains external code)
        if "third_party" in file_path.replace("\\", "/"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file for Google-style member variables."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Pattern to detect member variable declarations with trailing underscore
        # Matches: type_name var_name_; or type_name var_name_ = ...;
        # Also matches: type_name* var_name_; or type_name& var_name_;
        # Requires at least 2 characters before the trailing underscore to avoid single letters
        member_var_pattern = re.compile(
            r"\b"  # Word boundary
            r"(?:const\s+)?"  # Optional const
            r"(?:static\s+)?"  # Optional static
            r"(?:volatile\s+)?"  # Optional volatile
            r"(?:mutable\s+)?"  # Optional mutable
            r"[\w:]+(?:<[^>]+>)?"  # Type name (with optional template args)
            r"\s+"  # Required whitespace after type
            r"[*&]*"  # Optional pointer/reference symbols
            r"\s*"  # Optional whitespace
            r"(\w{2,}_)"  # Variable name with trailing underscore - at least 2 chars (capture group 1)
            r"\s*"  # Optional whitespace
            r"[;=,)]"  # Followed by semicolon, equals, comma, or closing paren
        )

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

            # Skip empty lines
            if not code_part.strip():
                continue

            # Basic string literal handling (skip lines with string literals containing underscores)
            # This is a simple heuristic - may need refinement
            if '"' in code_part:
                # Remove string literals before checking
                code_part = re.sub(r'"[^"]*"', '""', code_part)

            # Find all matches
            for match in member_var_pattern.finditer(code_part):
                var_name = match.group(1)

                # Skip common false positives
                # Skip if it's a function parameter in a function signature (has preceding '(')
                before_match = code_part[: match.start()]
                if "(" in before_match and before_match.strip().endswith("("):
                    continue

                # Skip preprocessor directives
                if code_part.strip().startswith("#"):
                    continue

                # Skip macro arguments (contains __COUNTER__, __LINE__, etc.)
                if "__" in var_name or "COUNTER" in code_part or "LINE" in code_part:
                    continue

                # Skip if variable name is all uppercase (likely a macro)
                if var_name[:-1].isupper() and len(var_name) > 2:
                    continue

                # Generate suggested name
                suggested_name = convert_google_to_m_prefix(var_name)

                # Store violation as (line_number, content) tuple with metadata
                violation_content = f"{var_name} -> {suggested_name}: {stripped[:80]}"
                violations.append((line_number, violation_content))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run Google-style member variable checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = GoogleMemberStyleChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found Google-style member variables",
        extensions=[".h", ".hpp", ".cpp"],
    )


if __name__ == "__main__":
    main()
