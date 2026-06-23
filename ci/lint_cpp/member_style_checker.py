#!/usr/bin/env python3
"""
Lint check: Prevent non-standard member variable naming styles.

This project uses m-prefix camelCase for member variables.

Detected violations:
  1. Google-style trailing underscore:
    class Foo {
        int state_;           // FAIL: Google style
        bool is_ready_;       // FAIL: Google style
    };

  2. m-underscore prefix (m_name):
    class Foo {
        int m_hybridNumDecim; // FAIL: m_ prefix
        bool m_isReady;       // FAIL: m_ prefix
        int m_some_var;       // FAIL: m_ prefix with snake_case
    };

Correct approach:
    class Foo {
        int mState;           // PASS: m-prefix camelCase
        bool mIsReady;        // PASS: m-prefix camelCase
        int mHybridNumDecim;  // PASS: m-prefix camelCase
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


def convert_m_underscore_to_m_prefix(var_name: str) -> str:
    """
    Convert m_underscore-style variable name to m-prefix camelCase.

    Examples:
        m_hybridNumDecim -> mHybridNumDecim
        m_isReady -> mIsReady
        m_some_var -> mSomeVar
        m_x -> mX
        m_dataPtr -> mDataPtr
    """
    if not var_name.startswith("m_"):
        return var_name

    remainder = var_name[2:]  # Strip "m_"

    if not remainder:
        return var_name  # Just "m_" alone, don't transform

    # Check if remainder contains underscores (snake_case after m_)
    if "_" in remainder:
        # Convert snake_case to camelCase
        parts = remainder.split("_")
        capitalized_parts = [part.capitalize() for part in parts if part]
        return "m" + "".join(capitalized_parts)

    # remainder is already camelCase or a single word - capitalize first letter
    return "m" + remainder[0].upper() + remainder[1:]


class MemberStyleChecker(FileContentChecker):
    """Checker for non-standard member variable naming styles.

    Detects:
    - Google-style trailing underscore (state_, is_ready_)
    - m-underscore prefix (m_hybridNumDecim, m_isReady, m_some_var)
    """

    # Pre-compiled regex for member variable declarations with trailing underscore.
    # Matches: type_name var_name_; or type_name var_name_ = ...;
    # Also matches: type_name* var_name_; or type_name& var_name_;
    # Requires at least 2 characters before the trailing underscore.
    _TRAILING_UNDERSCORE_PATTERN = re.compile(
        r"\b"  # Word boundary
        r"(?:const\s+)?"  # Optional const
        r"(?:static\s+)?"  # Optional static
        r"(?:volatile\s+)?"  # Optional volatile
        r"(?:mutable\s+)?"  # Optional mutable
        r"[\w:]+(?:<[^>]+>)?"  # Type name (with optional template args)
        r"[\s*&]+"  # Required separator (whitespace and/or pointer/reference)
        r"\s*"  # Optional trailing whitespace
        r"(\w{2,}_)"  # Variable name with trailing underscore (capture group 1)
        r"\s*"  # Optional whitespace
        r"[;=,)]"  # Followed by semicolon, equals, comma, or closing paren
    )

    # Pre-compiled regex for member variable declarations with m_ prefix.
    # Matches: type_name m_varName; or type_name m_varName = ...;
    _M_UNDERSCORE_PATTERN = re.compile(
        r"\b"  # Word boundary
        r"(?:const\s+)?"  # Optional const
        r"(?:static\s+)?"  # Optional static
        r"(?:volatile\s+)?"  # Optional volatile
        r"(?:mutable\s+)?"  # Optional mutable
        r"[\w:]+(?:<[^>]+>)?"  # Type name (with optional template args)
        r"[\s*&]+"  # Required separator (whitespace and/or pointer/reference)
        r"\s*"  # Optional trailing whitespace
        r"(m_\w+)"  # Variable name with m_ prefix (capture group 1)
        r"\s*"  # Optional whitespace
        r"[;=,)]"  # Followed by semicolon, equals, comma, or closing paren
    )

    # Pre-compiled regex for removing string literals before checking
    _STRING_LITERAL_PATTERN = re.compile(r'"[^"]*"')

    # Pre-compiled set of excluded files for O(1) suffix lookups
    _EXCLUDED_SET = frozenset(EXCLUDED_FILES)

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process C++ source and header files."""
        # Check file extension
        if not file_path.endswith((".h", ".hpp", ".cpp")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in self._EXCLUDED_SET):
            return False

        # Exclude third_party directory (contains external code)
        if "third_party" in file_path.replace("\\", "/"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file for non-standard member variable naming."""
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

            # Fast first pass: skip complex regex if line doesn't contain
            # a trailing underscore terminator OR m_ prefix
            has_trailing_underscore = (
                "_;" in code_part
                or "_=" in code_part
                or "_," in code_part
                or "_)" in code_part
                or "_ " in code_part
            )
            has_m_underscore = "m_" in code_part

            if not has_trailing_underscore and not has_m_underscore:
                continue

            # Basic string literal handling
            if '"' in code_part:
                code_part = self._STRING_LITERAL_PATTERN.sub('""', code_part)

            # Check for trailing underscore (Google style)
            if has_trailing_underscore:
                self._check_trailing_underscore(
                    code_part, stripped, line_number, violations
                )

            # Check for m_ prefix style
            if has_m_underscore:
                self._check_m_underscore(code_part, stripped, line_number, violations)

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list

    @staticmethod
    def _is_inside_parens(code_part: str, pos: int) -> bool:
        """Check if position is inside parentheses (i.e., a function parameter)."""
        depth = 0
        for c in code_part[:pos]:
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
        return depth > 0

    def _check_trailing_underscore(
        self,
        code_part: str,
        stripped: str,
        line_number: int,
        violations: list[tuple[int, str]],
    ) -> None:
        """Check for Google-style trailing underscore member variables."""
        for match in self._TRAILING_UNDERSCORE_PATTERN.finditer(code_part):
            var_name = match.group(1)

            # Skip function/constructor parameters (inside parentheses)
            if self._is_inside_parens(code_part, match.start()):
                continue

            # Skip preprocessor directives
            if code_part.strip().startswith("#"):
                continue

            # Skip macro arguments
            if "__" in var_name or "COUNTER" in code_part or "LINE" in code_part:
                continue

            # Skip all-uppercase names (likely macros)
            if var_name[:-1].isupper() and len(var_name) > 2:
                continue

            suggested_name = convert_google_to_m_prefix(var_name)
            violation_content = f"{var_name} -> {suggested_name}: {stripped[:80]}"
            violations.append((line_number, violation_content))

    def _check_m_underscore(
        self,
        code_part: str,
        stripped: str,
        line_number: int,
        violations: list[tuple[int, str]],
    ) -> None:
        """Check for m_underscore prefix member variables."""
        for match in self._M_UNDERSCORE_PATTERN.finditer(code_part):
            var_name = match.group(1)

            # Skip function/constructor parameters (inside parentheses)
            if self._is_inside_parens(code_part, match.start()):
                continue

            # Skip preprocessor directives
            if code_part.strip().startswith("#"):
                continue

            # Skip macro arguments
            if "__" in var_name or "COUNTER" in code_part or "LINE" in code_part:
                continue

            suggested_name = convert_m_underscore_to_m_prefix(var_name)
            violation_content = f"{var_name} -> {suggested_name}: {stripped[:80]}"
            violations.append((line_number, violation_content))


# Backwards compatibility alias
GoogleMemberStyleChecker = MemberStyleChecker


def main() -> None:
    """Run member style checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = MemberStyleChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found non-standard member variable naming",
        extensions=[".h", ".hpp", ".cpp"],
    )


if __name__ == "__main__":
    main()
