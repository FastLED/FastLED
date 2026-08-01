#!/usr/bin/env python3
"""Reject Meson ``/`` joins on canonical forward-slash path variables.

``path_norm_*`` values are deliberately composed with ``+ '/' +``, so that the
spelling of every derived path is fixed by *our* source rather than by the
behaviour of whichever Meson is installed.

That distinction is the whole point, and it is easy to get wrong:

- Older Meson implemented ``/`` as a bare ``os.path.join``, which on Windows
  injects a native separator: ``path_norm_root / 'src'`` produced
  ``C:/Users/.../fastled\\src``. clang passes that ``-I`` through verbatim, the
  PCH canonicalises one spelling and the consuming TU sees another,
  ``#pragma once`` stops deduping, and the build dies with
  ``error: redefinition of ...``. Reproduced and root-caused 2026-06-02.
- Current Meson (1.11.1, ``mesonbuild/interpreter/primitives/string.py``
  ``_op_div``) appends ``.replace('\\\\', '/')``, so ``/`` now normalises and
  that specific hazard is gone.

The rule is kept anyway. A build whose include spelling depends on the Meson
version is a build that breaks on upgrade, and this is the exact failure mode
that keeps costing days to diagnose. Literal concatenation cannot regress.

Do not "modernise" this away on the grounds that ``/`` is safe today --
see ``ci/tests/test_meson_div_separator.py``, which pins the behaviour this
reasoning depends on.

A small lexer removes comments and quoted strings before a regex checks the
unambiguous token sequence; a full Meson parser would add substantial
complexity without improving this focused rule.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker


_PATH_NORM_JOIN_PATTERN = re.compile(r"\b(path_norm_[A-Za-z0-9_]*)\s*/")


def _code_without_strings_and_comments(line: str) -> str:
    """Replace quoted strings and comments so only executable Meson remains."""
    code: list[str] = []
    quote: str | None = None
    escaped = False
    for char in line:
        if quote is not None:
            code.append(" ")
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char in {"'", '"'}:
            quote = char
            code.append(" ")
        elif char == "#":
            break
        else:
            code.append(char)
    return "".join(code)


class PathNormJoinChecker(FileContentChecker):
    """Forbid host-native Meson path joins on ``path_norm_*`` values."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        return file_path.replace("\\", "/").endswith("meson.build")

    def check_file_content(self, file_content: FileContent) -> list[str]:
        for line_number, line in enumerate(file_content.lines, 1):
            code = _code_without_strings_and_comments(line)
            match = _PATH_NORM_JOIN_PATTERN.search(code)
            if match:
                variable = match.group(1)
                self.violations.setdefault(file_content.path, []).append(
                    (
                        line_number,
                        f"Compose {variable} with a literal forward slash "
                        f"instead: {variable} + '/' + 'relative/path'. "
                        "Meson's '/' normalises separators in current "
                        "versions but did not always, and a build whose "
                        "include spelling depends on the Meson version "
                        "breaks on upgrade -- mixed separators defeat "
                        "#pragma once across the PCH boundary.",
                    )
                )
        return []
