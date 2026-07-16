#!/usr/bin/env python3
"""Reject Meson ``/`` joins on canonical forward-slash path variables.

``path_norm_*`` values are deliberately composed with ``+ '/' +``. Meson's
``/`` operator uses the host-native separator, so applying it to one of those
values on Windows creates a mixed-separator path and defeats compiler/PCH
caches. A small lexer removes comments and quoted strings before a regex
checks the unambiguous unsafe token sequence; a full Meson parser would add
substantial complexity without improving this focused rule.
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
                        f"Meson's '/' operator injects native separators when "
                        f"joining {variable}. Use string concatenation with a "
                        f"literal forward slash instead: {variable} + '/' + "
                        "'relative/path'.",
                    )
                )
        return []
