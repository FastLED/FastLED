#!/usr/bin/env python3

"""
Check for bare 'using' declarations at file/namespace scope in header files.

In unity builds, all .cpp.hpp files are concatenated into a single translation
unit. Bare 'using X::Y;' or 'using namespace X;' at file or namespace scope
leaks names into other translation units, causing potential conflicts.

Only flags:
  - 'using X::Y;' (using-declarations)
  - 'using namespace X;' (using-directives)
at file scope or inside named namespace blocks.

Does NOT flag:
  - 'using Alias = Type;' (type alias declarations)
  - 'using Base::member;' inside class/struct bodies
  - Any 'using' inside function bodies, class/struct bodies, or anonymous namespaces
  - Lines with '// ok bare using' or '// okay bare using' suppression comments

Suppression: Add '// ok bare using' or '// okay bare using' at end of line.
"""

import re
from pathlib import Path

from ci.util.check_files import (
    FileContent,
    FileContentChecker,
    run_checker_standalone,
)
from ci.util.paths import PROJECT_ROOT


# Pre-compiled regex patterns

# Matches 'using X::Y;' or 'using namespace X;' but NOT 'using X = ...'
# Must check for '=' separately since the declaration form overlaps.
_RE_USING_DECL = re.compile(
    r"^\s*using\s+(?!.*\s*=)"  # 'using' not followed by '='
    r"(?:"
    r"namespace\s+\w+(?:::\w+)*"  # 'using namespace X::Y'
    r"|"
    r"\w+(?:::\w+)+"  # 'using X::Y' (must have at least one ::)
    r")\s*;"
)

# Suppression comment pattern
_RE_SUPPRESS = re.compile(r"//\s*ok(?:ay)?\s+bare\s+using", re.IGNORECASE)

# Named namespace opening: 'namespace X {' or 'namespace X { ...'
# Also matches nested: 'namespace X { namespace Y {'
_RE_NAMED_NAMESPACE = re.compile(r"^\s*namespace\s+\w+")

# Anonymous namespace: 'namespace {'
_RE_ANON_NAMESPACE = re.compile(r"^\s*namespace\s*\{")

# extern "C" { — treat as namespace-level scope (not local)
_RE_EXTERN_C = re.compile(r'^\s*extern\s+"C"')

# Class/struct/enum/union definition opening
_RE_LOCAL_SCOPE_KEYWORD = re.compile(r"^\s*(?:class|struct|enum|union)\b")

# Preprocessor directive
_RE_PREPROCESSOR = re.compile(r"^\s*#")

# Single-line comment
_RE_LINE_COMMENT = re.compile(r"//")

# Excluded files (backward compat or special purpose)
_EXCLUDED_BASENAMES = frozenset({"FastLED.h"})


def _strip_strings_and_comments(line: str) -> str:
    """Remove string literals and single-line comments from a line for analysis."""
    # Remove string literals (simple approach)
    result = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"', '""', line)
    # Remove single-line comments but preserve suppression check on original
    comment_match = _RE_LINE_COMMENT.search(result)
    if comment_match:
        result = result[: comment_match.start()]
    return result


class BareUsingChecker(FileContentChecker):
    """Checks for bare 'using' declarations at file/namespace scope in headers.

    These are dangerous in unity builds because .cpp.hpp files are concatenated
    and bare using declarations leak into other translation units.
    """

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process .h, .hpp, .cpp.hpp files under src/fl/."""
        normalized = file_path.replace("\\", "/")

        # Must be under src/fl/
        if "/src/fl/" not in normalized:
            return False

        # Skip third_party
        if "/third_party/" in normalized:
            return False

        # Skip excluded basenames
        basename = Path(file_path).name
        if basename in _EXCLUDED_BASENAMES:
            return False

        # Only header-like files
        return normalized.endswith((".h", ".hpp", ".cpp.hpp"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file for bare using declarations at namespace scope."""
        violations: list[tuple[int, str]] = []
        lines = file_content.lines

        # Track scope: stack of scope types
        # 'namespace' = file-level or named namespace (flagged)
        # 'local' = class/struct/function/anon-namespace (not flagged)
        scope_stack: list[str] = []
        in_block_comment = False

        for line_num, line in enumerate(lines, 1):
            # Handle block comments
            if in_block_comment:
                if "*/" in line:
                    in_block_comment = False
                continue

            # Check for block comment start, but only OUTSIDE single-line comments
            # Strip the // comment portion first to avoid false positives
            # e.g. "// implementations in viz/*.cpp.hpp" should not trigger
            line_before_line_comment = line
            line_comment_pos = line.find("//")
            if line_comment_pos >= 0:
                line_before_line_comment = line[:line_comment_pos]

            if "/*" in line_before_line_comment:
                start_pos = line_before_line_comment.index("/*")
                rest = line[start_pos + 2 :]
                if "*/" not in rest:
                    in_block_comment = True
                continue  # Skip lines that start/contain block comments

            # Skip preprocessor directives
            if _RE_PREPROCESSOR.match(line):
                continue

            # Strip comments for analysis
            clean = _strip_strings_and_comments(line)

            # Track braces for scope depth
            # Count opens and closes in the clean line
            open_braces = clean.count("{")
            close_braces = clean.count("}")

            # Before processing braces, check if this line opens a scope
            # Determine what kind of scope is being opened
            scope_types_to_push: list[str] = []

            if open_braces > 0:
                # Determine the type of scope being opened
                if _RE_ANON_NAMESPACE.match(clean):
                    # Anonymous namespace — local scope (safe zone)
                    scope_types_to_push.append("local")
                    open_braces -= 1  # consumed one brace
                elif _RE_NAMED_NAMESPACE.match(clean):
                    # Named namespace — still namespace scope (flagged)
                    scope_types_to_push.append("namespace")
                    open_braces -= 1
                elif _RE_EXTERN_C.match(clean):
                    # extern "C" { — namespace-level scope
                    scope_types_to_push.append("namespace")
                    open_braces -= 1
                elif _RE_LOCAL_SCOPE_KEYWORD.match(clean):
                    # class/struct/enum/union — local scope
                    scope_types_to_push.append("local")
                    open_braces -= 1

                # Remaining open braces are function bodies or other blocks
                for _ in range(open_braces):
                    scope_types_to_push.append("local")

            # Push new scopes
            for scope_type in scope_types_to_push:
                scope_stack.append(scope_type)

            # Pop scopes for close braces
            for _ in range(close_braces):
                if scope_stack:
                    scope_stack.pop()

            # Now check if this line has a bare using declaration
            if _RE_USING_DECL.search(clean):
                # Check suppression on original line (not stripped)
                if _RE_SUPPRESS.search(line):
                    continue

                # Determine if we're at namespace scope (flagged)
                # At namespace scope means: no 'local' scope in the stack
                is_namespace_scope = all(s == "namespace" for s in scope_stack)

                if is_namespace_scope:
                    violations.append((line_num, line.strip()))

        if violations:
            self.violations[file_content.path] = violations

        return []  # Violations stored internally


def main() -> None:
    """Run the bare using checker standalone."""
    checker = BareUsingChecker()
    src_fl = str(PROJECT_ROOT / "src" / "fl")
    run_checker_standalone(
        checker,
        [src_fl],
        "Bare using declarations in headers (unity build safety)",
        extensions=[".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
