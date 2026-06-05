#!/usr/bin/env python3
"""Checker for raw `<cstdio>` printf-family calls in FastLED sources.

Calling C's `snprintf` / `printf` / `fprintf` / `vfprintf` / `vsnprintf`
family from anywhere inside `libFastLED.a` pulls newlib's `_svfprintf_r`
(~11 KB on xtensa-esp-elf) and its integer-only fallback `_svfiprintf_r`
(~7 KB) into the binary. On ESP32-S3 builds that's ~36 KB of printf
machinery across the float and integer variants — see
https://github.com/FastLED/FastLED/issues/2473 and the audit in
https://github.com/FastLED/FastLED/issues/2473#issuecomment-4628287075.

FastLED ships its own template-based formatter at
`src/fl/stl/stdio.h:659` (`fl::snprintf`, `fl::printf`, etc.) that
routes through `fl::sstream`. The `fl::` variants are faster than
newlib for the integer/string cases we actually use, don't pull the
libc printf engine, and are the canonical FastLED API.

Banned in `src/`:
  - bare `snprintf(...)`, `sprintf(...)`, `printf(...)`, `fprintf(...)`,
    `vfprintf(...)`, `vsnprintf(...)`, `vprintf(...)`, `vsprintf(...)`,
    `vfiprintf(...)`, `vsnprintf_s(...)`, `snprintf_s(...)`
  - global-scope explicit `::snprintf(...)` etc.

Allowed:
  - `fl::snprintf(...)`, `fl::printf(...)`, etc. (the FastLED wrappers)
  - any `Object.snprintf(...)` / `pointer->snprintf(...)` member call
    (none such exist in standard libraries; included for completeness)

Excluded paths:
  - `src/fl/stl/stdio.{h,hpp}` and `src/fl/stl/cstdio.*` — that's where
    the wrappers are defined; they have to reference the C API at the
    implementation boundary.
  - `src/third_party/**` — third-party code defines its own printf
    conventions.

Opt-out:
  - Append `// ok cstdio - <reason>` on the same line for genuine
    boundary cases (e.g., a platform-specific implementation that has
    to call into a vendor-supplied formatter).
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Pattern to find candidate C printf-family call sites.
# Acceptance/rejection of each match is then decided by inspecting the
# preceding text via `_is_allowed_prefix`.
PRINTF_FAMILY = r"snprintf|sprintf|printf|fprintf|vfprintf|vsnprintf|vprintf|vsprintf|vfiprintf|vsnprintf_s|snprintf_s"
PRINTF_CALL_PATTERN = re.compile(r"\b(" + PRINTF_FAMILY + r")\s*\(")


_BANNED_NAMESPACES = ("std::",)  # plus root `::` handled separately
_NAME_PREFIX_RE = re.compile(r"([A-Za-z_][A-Za-z_0-9]*)::$")


def _is_allowed_prefix(line: str, match_start: int) -> bool:
    """Return True if the printf-family call at `match_start` is one we
    explicitly allow.

    Allowed prefixes:
      - `fl::printf(...)` — the FastLED template wrapper.
      - `SerialPort::printf(...)` / `MyClass::snprintf(...)` — any
        user-defined class method definition or qualified call. Member
        functions never drag in newlib regardless of name.
      - `obj.printf(...)` / `ptr->printf(...)` — member access.
      - identifier-prefixed names like `my_snprintf(...)` — defensive
        guard, since `\\b` already handles sub-word boundaries.

    Banned prefixes:
      - bare `printf(...)`, no prefix.
      - `::printf(...)` — explicit global C namespace.
      - `std::snprintf(...)` — explicit C++ std namespace (still routes
        through newlib on xtensa-esp-elf).
    """
    prefix = line[:match_start]
    rstripped = prefix.rstrip()

    # Member access — always allowed.
    if rstripped.endswith(".") or rstripped.endswith("->"):
        return True

    # Identifier-prefixed (defensive — `\b` should already cover this).
    if rstripped and (rstripped[-1].isalnum() or rstripped[-1] == "_"):
        return True

    # Banned: explicit `std::` namespace.
    for ns in _BANNED_NAMESPACES:
        if rstripped.endswith(ns):
            return False

    # Banned: explicit root `::` namespace, with no name before it.
    if rstripped.endswith("::"):
        # Find the identifier before `::`, if any.
        m = _NAME_PREFIX_RE.search(rstripped)
        if m:
            # Some identifier owns this `printf` — user-defined class
            # method, allowed.
            return True
        # No identifier — bare `::printf`, root-namespace C call.
        return False

    # Otherwise (e.g., bare `printf(`) — banned.
    return False


# Opt-out marker on the same line. Lower-cased before match.
OPT_OUT_MARKER = "ok cstdio"


class CstdioPrintfChecker(FileContentChecker):
    """Reject raw C printf-family calls — use the `fl::` template wrappers."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        normalized = file_path.replace("\\", "/")

        # Skip third-party code (defines its own conventions).
        if "/third_party/" in normalized:
            return False

        # Skip the FastLED wrapper TUs themselves — they have to bridge
        # at the C-API boundary.
        if "/fl/stl/stdio.h" in normalized or "/fl/stl/stdio.hpp" in normalized:
            return False
        if "/fl/stl/cstdio." in normalized:
            return False

        # Skip host-only platforms — they don't link newlib, so the printf
        # engine isn't an embedded-bloat concern there.
        host_only_paths = (
            "/platforms/wasm/",  # Emscripten libc, console-write
            "/platforms/posix/",  # Host test runner
            "/platforms/apple/",  # Host test runner
            "/platforms/win/",  # Host test runner
            "/platforms/stub/",  # Host test stub
        )
        for path in host_only_paths:
            if path in normalized:
                return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for raw C printf-family calls."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state.
            if "/*" in line and "*/" not in line:
                in_multiline_comment = True
                continue
            if in_multiline_comment:
                if "*/" in line:
                    in_multiline_comment = False
                continue

            # Skip single-line comments.
            if stripped.startswith("//"):
                continue
            # Skip preprocessor includes — `#include <cstdio>` etc.
            if stripped.startswith("#"):
                continue

            code_part = line.split("//")[0]
            comment_part = line[len(code_part) :].lower()

            # Allow opt-out: `// ok cstdio - <reason>` on the same line.
            if OPT_OUT_MARKER in comment_part:
                continue

            for match in PRINTF_CALL_PATTERN.finditer(code_part):
                if _is_allowed_prefix(code_part, match.start()):
                    continue
                func = match.group(1)
                violations.append(
                    (
                        line_number,
                        f"Avoid C `{func}` — use `fl::{func}` (from "
                        f"`fl/stl/stdio.h`) to avoid newlib's `_svfprintf_r` "
                        f"(~11 KB on ESP32 builds). See #2473 / #2773.\n"
                        f"      Offending line: {stripped}\n"
                        f"      If you genuinely need the C API at a platform "
                        f"boundary, suppress with `// ok cstdio - <reason>` "
                        f"on the same line.",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run cstdio printf-family checker standalone."""
    import sys
    from pathlib import Path

    from ci.util.check_files import (
        MultiCheckerFileProcessor,
        run_checker_standalone,
    )

    if len(sys.argv) > 1:
        file_path = Path(sys.argv[1]).resolve()
        if not file_path.exists():
            print(f"Error: File not found: {file_path}")
            sys.exit(1)

        checker = CstdioPrintfChecker()
        processor = MultiCheckerFileProcessor()
        processor.process_files_with_checkers([str(file_path)], [checker])

        if hasattr(checker, "violations") and checker.violations:
            violations = checker.violations
            print(
                f"❌ Found raw C printf-family usage — use the `fl::` "
                f"wrappers from `fl/stl/stdio.h` instead "
                f"({len(violations)} file(s)):\n"
            )
            for file_path_str, violation_list in violations.items():
                rel_path = Path(file_path_str).relative_to(PROJECT_ROOT)
                print(f"{rel_path}:")
                for line_num, message in violation_list:
                    print(f"  Line {line_num}: {message}")
            sys.exit(1)
        else:
            print("✅ No raw C printf-family usage found.")
            sys.exit(0)
    else:
        checker = CstdioPrintfChecker()
        run_checker_standalone(
            checker,
            [str(PROJECT_ROOT / "src")],
            "Found raw C printf-family usage — use `fl::snprintf` / "
            "`fl::printf` (from `fl/stl/stdio.h`) instead",
            extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
        )


if __name__ == "__main__":
    main()
