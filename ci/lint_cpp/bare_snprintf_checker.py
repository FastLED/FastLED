#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker that bans bare C `::snprintf` / `::printf` / `::sprintf` / etc.

Rationale (FastLED #2773 item 1.1 / 1.5): newlib's `snprintf` pulls
`_svfprintf_r` (~11 KB) into the binary. The fl::snprintf template in
`fl/stl/stdio.h` formats integers/strings through `fl::sstream`, which
the linker can dead-strip and which doesn't transitively pull the full
C `printf` engine.

`libFastLED.a` is the half of the binary we control, so this checker
enforces the discipline only there. ESP-IDF / mbed / mbedtls etc. can
still use the C printf engine; this rule just stops FastLED from
*adding* to their pull-in count.

Suppression:
- Add `// ok snprintf` at end of line to whitelist a specific call.
- `fl/stl/stdio.h`, `fl/stl/cstdio.h`, and the lint checker itself are
  always exempt.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

# Whitelisted files inside src/ that legitimately need the C symbols
# (the fl:: shim itself; the cstdio compat header).
EXEMPT_FILES = (
    "fl/stl/stdio.h",
    "fl/stl/stdio.cpp.hpp",
    "fl/stl/cstdio.h",
    "fl/stl/cstdio.cpp.hpp",
)

BANNED_FUNCS = (
    "snprintf",
    "sprintf",
    "vsnprintf",
    "vsprintf",
    "fprintf",
    "vfprintf",
    "printf",
    "vprintf",
)

# Match `<func>(` not preceded by `fl::`, `::`, `.`, or an identifier
# character. Examples that match:
#   snprintf(buf, ...)
# Examples that DO NOT match:
#   fl::snprintf(...)
#   ::snprintf(...)        (explicit global qualification, allowed)
#   obj.snprintf(...)
#   my_snprintf(...)
_BANNED_PATTERN = re.compile(
    r"(?<![A-Za-z0-9_:.\b])(" + "|".join(BANNED_FUNCS) + r")\s*\("
)
_SUPPRESS = "// ok snprintf"


class BareSnprintfChecker(FileContentChecker):
    """Bans bare C printf-family calls inside src/ (FastLED #2773 item 1.5)."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        # Only src/.
        if not file_path.startswith(str(SRC_ROOT)):
            return False
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False
        norm = file_path.replace("\\", "/")
        # Skip the shim itself.
        for exempt in EXEMPT_FILES:
            if norm.endswith(exempt):
                return False
        # Skip third-party upstream code — we don't control these sources.
        if "/third_party/" in norm:
            return False
        # Skip host-only unit-test watchdog scaffolding. These files are HOST
        # test infrastructure (apple/posix/win) and never link into ESP32
        # firmware, so the binary-size argument doesn't apply.
        if norm.endswith("/run_unit_test.hpp"):
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str]] = []
        in_block_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track /* ... */ blocks (single-line case handled by the slice below).
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

            # Strip trailing // comment so we don't match inside a comment.
            code = line.split("//")[0]
            if _BANNED_PATTERN.search(code):
                violations.append((line_number, line.rstrip()))

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    from ci.util.check_files import run_checker_standalone

    checker = BareSnprintfChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT)],
        "Found bare C ::snprintf/::printf family call in src/ "
        "(use fl::snprintf — see FastLED #2773 item 1.1)",
        extensions=[".cpp", ".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
