#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker enforcing the `FL_NO_<WORD>` macro-naming convention.

Rationale: FastLED's compiler-attribute / behavior-suppression macros
follow the `FL_NO_<WORD>` shape -- `FL_NO_INLINE`, `FL_NO_EXCEPT`,
`FL_NO_COPY(CLASS)`. A bare `FL_NO<WORD>` (no underscore) breaks
grep-ability and was the original spelling of `FL_NO_EXCEPT` (renamed
in #3283). This checker prevents the misspelling from coming back and
catches any new `FL_NO<word>` macro that violates the convention.

Whitelist: `FL_NOP` and `FL_NOP2` are standard CPU "No-Operation"
abbreviations (per the assembly-language convention), not the
`FL_NO + WORD` pattern. Renaming them to `FL_NO_P` / `FL_NO_P2` would
obscure standard terminology, so they're accepted by exact name.

Suppression: add `// ok no-underscore` at end of line.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

# Exact-match whitelist for tokens that look like `FL_NO<WORD>` but are
# legitimate CPU "no-operation" abbreviations, not the `FL_NO + WORD`
# convention.
WHITELIST = frozenset({"FL_NOP", "FL_NOP2"})

# Prefix-based skip: identifiers starting with `FL_NOT_` are `FL_ + NOT_ + ...`
# (e.g. `FL_NOT_NULL_ASSERT`), a different lexical structure unrelated to
# the `FL_NO + WORD` macro-naming pattern this checker enforces.
SKIP_PREFIXES = ("FL_NOT_",)

# Match any identifier of the form `FL_NO<UPPER><MORE...>` where the
# character following `FL_NO` is NOT an underscore. This catches misspellings
# like `FL_NO` + `EXCEPT` / `INLINE` / `FOO` etc. (the historical
# `FL_NO`-without-underscore form that #3283 removes).
_BANNED_PATTERN = re.compile(r"\bFL_NO[A-Z][A-Z0-9_]*\b")
_SUPPRESS = "// ok no-underscore"


class FlNoUnderscoreChecker(FileContentChecker):
    """Enforces `FL_NO_<WORD>` convention; bans bare `FL_NO<WORD>` (#3283)."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.startswith(str(SRC_ROOT)):
            return False
        if not file_path.endswith((".cpp", ".h", ".hpp", ".cc", ".hh", ".inl", ".ino")):
            return False
        norm = file_path.replace("\\", "/")
        # Upstream third-party code: not our syntax to enforce.
        if "/third_party/" in norm:
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str, str]] = []
        in_block_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            if in_block_comment:
                if "*/" in line:
                    in_block_comment = False
                continue
            if "/*" in line and "*/" not in line:
                in_block_comment = True
                continue

            if stripped.startswith("//"):
                continue
            if _SUPPRESS in line:
                continue

            code = line.split("//")[0]
            for match in _BANNED_PATTERN.finditer(code):
                token = match.group(0)
                if token in WHITELIST:
                    continue
                if any(token.startswith(p) for p in SKIP_PREFIXES):
                    continue
                violations.append((line_number, token, line.rstrip()))

        if violations:
            self.violations[file_content.path] = violations
        return []


def main() -> None:
    from ci.util.check_files import run_checker_standalone

    checker = FlNoUnderscoreChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT)],
        "Found `FL_NO<WORD>` identifier(s) missing the underscore -- "
        "use `FL_NO_<WORD>` for grep-able convention consistency "
        "(FastLED #3283). Whitelisted: FL_NOP, FL_NOP2.",
        extensions=[".cpp", ".h", ".hpp", ".cc", ".hh", ".inl", ".ino"],
    )


if __name__ == "__main__":
    main()
