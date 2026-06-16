#!/usr/bin/env python3
"""Checker that bans legacy stream-style FastLED logging macros in src/.

FastLED logging call sites should use the printf-style `_F` variants so the
formatter work is centralized in `fl::printf` instead of instantiating a fresh
`fl::sstream` operator chain at every call site.
"""

import re
from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"
LOG_HEADER = (SRC_ROOT / "fl" / "log" / "log.h").resolve()

LEGACY_MACROS = (
    "FL_WARN",
    "FL_WARN_IF",
    "FL_WARN_ONCE",
    "FL_WARN_EVERY",
    "FL_WARN_FMT",
    "FL_WARN_FMT_IF",
    "FL_PRINT",
    "FL_PRINT_EVERY",
    "FL_DBG",
    "FL_DBG_IF",
    "FL_DBG_EVERY",
    "FL_ERROR",
    "FL_ERROR_IF",
    "FL_LOG_SPI",
    "FL_LOG_RMT",
    "FL_LOG_PARLIO",
    "FL_LOG_AUDIO",
    "FL_LOG_INTERRUPT",
    "FL_LOG_FLEXIO",
    "FL_LOG_OBJECTFLED",
    "FL_LOG_ASYNC",
    "FL_LOG_SPI_ASYNC_MAIN",
    "FL_LOG_RMT_ASYNC_MAIN",
    "FL_LOG_PARLIO_ASYNC_MAIN",
    "FL_LOG_AUDIO_ASYNC_MAIN",
    "FL_LOG_INTERRUPT_ASYNC_MAIN",
    "FL_LOG_FLEXIO_ASYNC_MAIN",
    "FL_LOG_OBJECTFLED_ASYNC_MAIN",
)

_LEGACY_PATTERN = re.compile(
    r"\b("
    + "|".join(re.escape(name) for name in sorted(LEGACY_MACROS, key=len, reverse=True))
    + r")\s*\("
)


class LegacyLogMacroChecker(FileContentChecker):
    """Bans legacy stream-style logging macro calls inside src/."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        path = file_path.replace("\\", "/")
        if not file_path.startswith(str(SRC_ROOT)):
            return False
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False
        if "/third_party/" in path:
            return False
        if Path(file_path).resolve() == LOG_HEADER:
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str]] = []
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
            if stripped.startswith("//") or stripped.startswith("#define"):
                continue

            code = line.split("//")[0]
            match = _LEGACY_PATTERN.search(code)
            if not match:
                continue
            macro = match.group(1)
            replacement = _replacement_for(macro)
            violations.append(
                (
                    line_number,
                    f"{line.rstrip()}  [use {replacement} instead of {macro}]",
                )
            )

        if violations:
            self.violations[file_content.path] = violations

        return []


def _replacement_for(macro: str) -> str:
    if macro.endswith("_ASYNC_MAIN"):
        return f"{macro}_F"
    if macro == "FL_LOG_ASYNC":
        return "FL_LOG_ASYNC_F"
    if macro.endswith("_FMT_IF"):
        return macro.replace("_FMT_IF", "_F_IF")
    if macro.endswith("_FMT"):
        return macro.replace("_FMT", "_F")
    if macro.endswith("_IF"):
        return macro.replace("_IF", "_F_IF")
    if macro.endswith("_ONCE"):
        return macro.replace("_ONCE", "_F_ONCE")
    if macro.endswith("_EVERY"):
        return macro.replace("_EVERY", "_F_EVERY")
    return f"{macro}_F"


def main() -> None:
    from ci.util.check_files import run_checker_standalone

    checker = LegacyLogMacroChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT)],
        "Found legacy stream-style FastLED logging macros in src/ "
        "(use FL_*_F printf-style macros)",
        extensions=[".cpp", ".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
