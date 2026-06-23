#!/usr/bin/env python3
"""Checker to detect bare '#if FL_IS_*' usage without defined().

FL_IS_* macros are platform/feature detection macros that follow a
defined/undefined pattern (no values). They must be used with:
  - #ifdef FL_IS_*
  - #ifndef FL_IS_*
  - #if defined(FL_IS_*)
  - #if !defined(FL_IS_*)
  - #elif defined(FL_IS_*)

Using bare '#if FL_IS_*' or '#elif FL_IS_*' is WRONG because the macro
has no value — it evaluates to 0 when defined as empty, which is the
opposite of the intended behavior.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker


# Match bare #if/#elif FL_IS_* that is NOT wrapped in defined().
# This catches:
#   #if FL_IS_ESP32
#   #elif FL_IS_ARM || FL_IS_ESP32
#   #if FL_IS_ESP32 && FL_IS_ARM
# But does NOT match:
#   #ifdef FL_IS_ESP32
#   #ifndef FL_IS_ESP32
#   #if defined(FL_IS_ESP32)
#   #elif defined(FL_IS_ESP32) || defined(FL_IS_ARM)
#
# Strategy: find FL_IS_\w+ on preprocessor conditional lines, then check
# that each occurrence is inside a defined() call.
_PREPROCESSOR_IF_ELIF_RE = re.compile(r"^\s*#\s*(?:if|elif)\b")

# Match FL_IS_<identifier> tokens
_FL_IS_TOKEN_RE = re.compile(r"\bFL_IS_\w+\b")

# Match FL_IS_<identifier> that IS properly wrapped in defined()
# Handles both defined(FL_IS_X) and !defined(FL_IS_X)
_DEFINED_FL_IS_RE = re.compile(r"!?\s*defined\s*\(\s*FL_IS_\w+\s*\)")

# Fast-scan: does the file contain any FL_IS_ token at all?
_FAST_HAS_FL_IS_RE = re.compile(r"FL_IS_\w+")


class FlIsDefinedChecker(FileContentChecker):
    """Checker that ensures FL_IS_* macros are used with defined().

    FL_IS_* macros follow a defined/undefined pattern (no values assigned).
    Using bare '#if FL_IS_*' is incorrect because the macro expands to
    nothing, which evaluates to 0.

    Correct patterns:
      #ifdef FL_IS_ESP32
      #if defined(FL_IS_ESP32)
      #if defined(FL_IS_ESP32) || defined(FL_IS_ARM)

    Incorrect patterns:
      #if FL_IS_ESP32
      #elif FL_IS_ESP32 || FL_IS_ARM
    """

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        return file_path.endswith((".cpp", ".h", ".hpp", ".ino"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for bare #if FL_IS_* usage."""
        # Fast path: skip files without any FL_IS_ tokens
        if not _FAST_HAS_FL_IS_RE.search(file_content.content):
            return []

        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            if in_multiline_comment:
                continue

            stripped = line.strip()

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Only check #if and #elif lines (not #ifdef/#ifndef which are fine)
            if not _PREPROCESSOR_IF_ELIF_RE.match(stripped):
                continue

            # Remove trailing comment before checking
            code_part = line.split("//")[0]

            # Find all FL_IS_* tokens on this line
            all_fl_is_tokens = _FL_IS_TOKEN_RE.findall(code_part)
            if not all_fl_is_tokens:
                continue

            # Remove all properly-wrapped defined(FL_IS_*) from the code
            # to see if any bare FL_IS_* tokens remain
            stripped_code = _DEFINED_FL_IS_RE.sub("", code_part)

            # Check if bare FL_IS_* tokens remain after removing defined() wrappers
            bare_tokens = _FL_IS_TOKEN_RE.findall(stripped_code)
            for token in bare_tokens:
                message = (
                    f"Bare '{token}' in preprocessor conditional. "
                    f"FL_IS_* macros are defined/undefined (no value). "
                    f"Use '#ifdef {token}' or '#if defined({token})' instead."
                )
                violations.append((line_number, message))

        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run FL_IS_* defined checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = FlIsDefinedChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src"), str(PROJECT_ROOT / "examples")],
        "Found bare #if FL_IS_* usage (should use #ifdef or #if defined())",
    )


if __name__ == "__main__":
    main()
