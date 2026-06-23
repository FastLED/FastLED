#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for global-scope C library functions - should use fl:: variants instead.

Covers both <cctype> functions (isspace, isdigit, etc.) and <cstring> functions
(strlen, memcmp, memcpy, etc.). All have fl:: equivalents defined in
fl/stl/cctype.h and fl/stl/cstring.h respectively.

Suppression: add '// ok ctype' or '// okay ctype' to the line.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker, is_excluded_file


# C standard library character functions that have fl:: equivalents (from <cctype>)
CTYPE_FUNCTIONS = [
    "isspace",
    "isdigit",
    "isalpha",
    "isalnum",
    "isupper",
    "islower",
    "tolower",
    "toupper",
]

# C string/memory functions that have fl:: equivalents (from <cstring>)
# See fl/stl/cstring.h for the full list of wrappers
CSTRING_FUNCTIONS = [
    # String functions
    "strlen",
    "strcmp",
    "strncmp",
    "strcpy",
    "strncpy",
    "strcat",
    "strncat",
    "strstr",
    "strchr",
    "strrchr",
    "strspn",
    "strcspn",
    "strpbrk",
    "strtok",
    "strerror",
    # Memory functions
    "memcpy",
    "memcmp",
    "memmove",
    "memset",
    "memchr",
]

# Combined list of all checked functions
_ALL_FUNCTIONS = CTYPE_FUNCTIONS + CSTRING_FUNCTIONS

# Files that legitimately use bare C string/memory functions (implementation files)
_WHITELISTED_SUFFIXES: tuple[str, ...] = (
    "fl/stl/cctype.h",
    "fl/stl/cstring.h",
    "fl/stl/cstring.cpp.hpp",
)

# Match bare function call or ::function call, but not fl::function
# Uses negative lookbehind to exclude fl:: prefix
# (?<!\w) prevents matching substring endings like "myisspace" or "mystrlen"
# (?<!fl::) prevents matching fl::isspace or fl::strlen
_PATTERN = re.compile(r"(?<!\w)(?::{2})?\b(" + "|".join(_ALL_FUNCTIONS) + r")\s*\(")

# Match fl:: prefixed calls (these are OK)
_FL_PATTERN = re.compile(r"\bfl::(" + "|".join(_ALL_FUNCTIONS) + r")\s*\(")

# Match explicitly global-qualified ::func() calls — these bypass fl:: even inside
# namespace fl and must ALWAYS be flagged (unlike bare calls which resolve via ADL).
_GLOBAL_QUALIFIED_PATTERN = re.compile(
    r"(?<!\w)::(" + "|".join(_ALL_FUNCTIONS) + r")\s*\("
)

# Pre-compiled namespace fl detection (was inline re.search per line)
_NAMESPACE_FL_RE = re.compile(r"\bnamespace\s+fl\s*\{")

# Pre-compiled per-function patterns for the inner violation loop
# (was re.compile() inside the loop, creating new patterns per violation)
_BARE_FUNC_PATTERNS: dict[str, re.Pattern[str]] = {
    func: re.compile(r"(?<!\w)(?:::)?\b" + func + r"\s*\(") for func in _ALL_FUNCTIONS
}
_FL_FUNC_PATTERNS: dict[str, re.Pattern[str]] = {
    func: re.compile(r"\bfl::" + func + r"\s*\(") for func in _ALL_FUNCTIONS
}

# Fast first pass: frozenset of keywords for quick substring check
_FAST_KEYWORDS: frozenset[str] = frozenset(_ALL_FUNCTIONS)

# Map function -> its header for better error messages
_FUNC_HEADER: dict[str, str] = {}
for _f in CTYPE_FUNCTIONS:
    _FUNC_HEADER[_f] = "fl/stl/cctype.h"
for _f in CSTRING_FUNCTIONS:
    _FUNC_HEADER[_f] = "fl/stl/cstring.h"


class CtypeGlobalChecker(FileContentChecker):
    """Checker for global-scope C library function usage (ctype + cstring)."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        if is_excluded_file(file_path):
            return False

        if "third_party" in file_path or "thirdparty" in file_path:
            return False

        # Exclude the definition files themselves
        normalized = file_path.replace("\\", "/")
        if any(normalized.endswith(suffix) for suffix in _WHITELISTED_SUFFIXES):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for global-scope C library function usage."""
        # Fast file-level check: skip if no function keywords appear in file
        if not any(kw in file_content.content for kw in _FAST_KEYWORDS):
            return []

        violations: list[tuple[int, str]] = []
        in_multiline_comment = False
        # Track namespace fl depth — bare calls inside namespace fl { } are fine
        # because they resolve to fl::strlen etc. via unqualified lookup
        fl_namespace_depth = 0
        brace_depth_at_fl_namespace: list[int] = []
        brace_depth = 0

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

            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion
            code_part = line.split("//")[0]

            # Track brace depth and namespace fl
            if "namespace" in code_part and _NAMESPACE_FL_RE.search(code_part):
                fl_namespace_depth += 1
                brace_depth_at_fl_namespace.append(brace_depth)
            for ch in code_part:
                if ch == "{":
                    brace_depth += 1
                elif ch == "}":
                    brace_depth -= 1
                    if (
                        brace_depth_at_fl_namespace
                        and brace_depth == brace_depth_at_fl_namespace[-1]
                    ):
                        fl_namespace_depth -= 1
                        brace_depth_at_fl_namespace.pop()

            # Skip lines with suppression comment
            if "// ok ctype" in line or "// okay ctype" in line:
                continue

            # Fast first pass: skip regex if no function keyword appears in the line
            if not any(kw in code_part for kw in _FAST_KEYWORDS):
                continue

            # Inside namespace fl, bare calls (e.g. memcpy()) resolve to fl::memcpy
            # via unqualified lookup — those are fine. But explicitly global-qualified
            # calls (::memcpy()) bypass fl:: and must still be flagged.
            if fl_namespace_depth > 0:
                global_matches = _GLOBAL_QUALIFIED_PATTERN.findall(code_part)
                for func in set(global_matches):
                    header = _FUNC_HEADER.get(func, "fl/stl/cstring.h")
                    violations.append(
                        (
                            line_number,
                            f"Use fl::{func}() instead of ::{func}() "
                            f"— see {header}: {stripped}",
                        )
                    )
                continue

            # Find all function calls
            all_matches = _PATTERN.findall(code_part)
            if not all_matches:
                continue

            # Subtract fl:: prefixed ones (which are fine)
            fl_matches = _FL_PATTERN.findall(code_part)

            # Count bare/global uses
            bare_count = len(all_matches) - len(fl_matches)
            if bare_count > 0:
                for func in set(all_matches):
                    # Use pre-compiled per-function patterns
                    bare_hits = _BARE_FUNC_PATTERNS[func].findall(code_part)
                    fl_hits = _FL_FUNC_PATTERNS[func].findall(code_part)
                    if len(bare_hits) > len(fl_hits):
                        header = _FUNC_HEADER.get(func, "fl/stl/cstring.h")
                        violations.append(
                            (
                                line_number,
                                f"Use fl::{func}() instead of {func}() or ::{func}() "
                                f"— see {header}: {stripped}",
                            )
                        )

        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run ctype/cstring global checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = CtypeGlobalChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src"), str(PROJECT_ROOT / "tests")],
        "Found global-scope C library function usage (use fl:: variants)",
    )


if __name__ == "__main__":
    main()
