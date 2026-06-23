#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to detect raw 'noexcept' keyword in src/ files.

FastLED wraps the noexcept specifier behind the FL_NOEXCEPT macro
(defined in src/fl/stl/noexcept.h).  FL_NOEXCEPT is currently a noop
on all platforms due to compatibility issues across AVR, WASM, etc.
Using the macro rather than the bare keyword ensures that enabling
noexcept in the future only requires a change in one place.

Using raw 'noexcept' directly bypasses this portability layer.

Scope: src/** (all .h / .hpp / .cpp / .cpp.hpp files)

Exemptions:
  - src/fl/stl/noexcept.h  (the macro definition itself)
  - Lines entirely inside // or /* */ comments
  - #include directives (e.g. #include "fl/stl/noexcept.h")
  - Lines where 'noexcept' appears only as the RHS of #define FL_NOEXCEPT
  - Lines with suppression comment "// ok noexcept" or "// nolint"
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Match the bare noexcept keyword (not inside a word)
_RAW_NOEXCEPT = re.compile(r"\bnoexcept\b")

# The definition line in noexcept.h: "#define FL_NOEXCEPT noexcept"
# We want to allow this exact pattern (the macro that expands to noexcept)
_DEFINE_FL_NOEXCEPT = re.compile(r"#\s*define\s+FL_NOEXCEPT\b")

# Suppression comments accepted on the same line
_SUPPRESSION = re.compile(r"//\s*(?:ok\s+noexcept|nolint)\b", re.IGNORECASE)


class RawNoexceptChecker(FileContentChecker):
    """Checker that flags raw 'noexcept' keyword — use FL_NOEXCEPT instead."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        normalized = file_path.replace("\\", "/")

        # Only C++ source files inside src/
        # Accept both absolute paths (/path/to/project/src/...) and
        # relative test paths (src/fl/...)
        if "/src/" not in normalized and not normalized.startswith("src/"):
            return False
        if not normalized.endswith((".h", ".hpp", ".cpp", ".cpp.hpp")):
            return False

        # Skip the definition file itself
        if normalized.endswith("fl/stl/noexcept.h"):
            return False

        # Skip third-party code — not under our control
        if "/third_party/" in normalized:
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for raw noexcept keyword usage."""
        # Fast file-level skip when noexcept is completely absent
        if "noexcept" not in file_content.content:
            return []

        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue  # skip the closing */ line

            if in_multiline_comment:
                continue

            # Skip pure single-line comments
            if stripped.startswith("//"):
                continue

            # Strip inline comment portion
            code = stripped.split("//")[0].strip()
            if not code:
                continue

            # Skip preprocessor includes entirely — no raw noexcept there
            if re.match(r"^#\s*include\b", code):
                continue

            # Skip the #define FL_NOEXCEPT noexcept line (legal definition)
            if _DEFINE_FL_NOEXCEPT.search(code):
                continue

            # Only bother checking lines that contain noexcept
            if not _RAW_NOEXCEPT.search(code):
                continue

            # Allow noexcept(expr) — the noexcept operator/conditional specifier.
            # Only flag bare 'noexcept' (specifier without parentheses).
            # After stripping FL_NOEXCEPT occurrences, check if remaining
            # noexcept keywords are all followed by '('.
            temp = code.replace("FL_NOEXCEPT", "")
            remaining = list(_RAW_NOEXCEPT.finditer(temp))
            all_operator_form = all(
                temp[m.end() :].lstrip().startswith("(") for m in remaining
            )
            if remaining and all_operator_form:
                continue

            # Allow suppression comment on the same original line
            if _SUPPRESSION.search(line):
                continue

            violations.append(
                (
                    line_number,
                    f"Raw 'noexcept' keyword — use FL_NOEXCEPT macro instead "
                    f"(defined in fl/stl/noexcept.h, currently a noop everywhere "
                    f"for cross-platform compatibility): {stripped}",
                )
            )

        if violations:
            self.violations[file_content.path] = violations

        return []  # Violations collected internally


def main() -> None:
    """Run raw_noexcept checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = RawNoexceptChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found raw 'noexcept' keyword — use FL_NOEXCEPT macro instead",
        extensions=[".h", ".hpp", ".cpp", ".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
