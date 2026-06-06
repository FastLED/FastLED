#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker banning bare `__attribute__((noinline))` and `__declspec(noinline)` in `src/`.

Rationale: FastLED ships an `FL_NO_INLINE` macro in
`fl/stl/compiler_control.h` that expands to the right syntax for GCC,
Clang, and MSVC. Bare `__attribute__((noinline))` is GCC/Clang-only and
silently no-ops under MSVC, so a host build that flows through the MSVC
toolchain (e.g. CI's WASM/Windows host tests) can quietly lose the
`noinline` semantics — exactly the kind of regression #2773 item 2.1
exists to prevent (the hot/cold split collapses back into the inlined
hot path).

Suppression: add `// ok noinline` at end of line. Five source paths are
exempt as a matter of course:

- `fl/stl/compiler_control.h` — where the `FL_NO_INLINE` macro is
  defined; the bare attribute appears inside the macro body itself.
- `fl/stl/compiler_control.cpp.hpp` — sibling of the above.
- `src/third_party/` — upstream code we don't control.
- the lint checker itself.
- `src/platforms/arm/nrf52/led_sysdefs_arm_nrf52.h` — uses
  `__attribute__((used, noinline))` on a linker keep-alive function;
  the `used` attribute has no FL_NO_INLINE equivalent. The file is
  hard-gated to nrf52 ARM builds (never flows through MSVC), so the
  MSVC-drift risk the lint guards against does not apply here.
- `src/platforms/arm/teensy/coroutine_teensy.impl.hpp` — uses
  `__attribute__((naked, noinline, used))` on the ARM context-switch
  trampoline; `naked` is required for the inline-asm prologue and has
  no portable FL_ equivalent. Same ARM-only gate as the nrf52 case.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

# Whitelisted files inside src/ that legitimately use the bare attribute.
# See the module docstring for rationale on each entry.
EXEMPT_FILES = (
    "fl/stl/compiler_control.h",
    "fl/stl/compiler_control.cpp.hpp",
    "platforms/arm/nrf52/led_sysdefs_arm_nrf52.h",
    "platforms/arm/teensy/coroutine_teensy.impl.hpp",
)

# Match `__attribute__((noinline))`, `__attribute__ ((noinline))`, and
# *compound* attribute lists that include `noinline` as one element —
# e.g. `__attribute__((naked, noinline, used))` or
# `__attribute__((used, noinline))` (#2854). The MSVC-drift risk the
# lint exists to prevent applies identically to the compound forms; the
# only reason they don't bite today is that the call sites happen to be
# on ARM platforms that never flow through MSVC. That's a fragile
# invariant — better to require an explicit `// ok noinline`
# suppression on those sites so the intent is greppable.
#
# Compound matching is implemented with a `noinline` keyword anchored
# inside the outer `__attribute__((...))` parens; `[^)]*` keeps the
# match local to a single attribute list so we don't accidentally span
# multiple lines or sibling attribute groups.
_BANNED_PATTERN = re.compile(
    r"__attribute__\s*\(\s*\([^)]*\bnoinline\b[^)]*\)\s*\)"
    r"|"
    r"__declspec\s*\(\s*noinline\s*\)"
)
_SUPPRESS = "// ok noinline"


class BareNoInlineChecker(FileContentChecker):
    """Bans bare `noinline` compiler attributes in src/ (#2773 item 2.1 follow-up)."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.startswith(str(SRC_ROOT)):
            return False
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False
        norm = file_path.replace("\\", "/")
        for exempt in EXEMPT_FILES:
            if norm.endswith(exempt):
                return False
        # Upstream third-party code: not our syntax to enforce.
        if "/third_party/" in norm:
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

            if stripped.startswith("//"):
                continue
            if _SUPPRESS in line:
                continue

            code = line.split("//")[0]
            if _BANNED_PATTERN.search(code):
                violations.append((line_number, line.rstrip()))

        if violations:
            self.violations[file_content.path] = violations
        return []


def main() -> None:
    from ci.util.check_files import run_checker_standalone

    checker = BareNoInlineChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT)],
        "Found bare `__attribute__((noinline))` / `__declspec(noinline)` "
        "in src/ — use `FL_NO_INLINE` from fl/stl/compiler_control.h "
        "instead (FastLED #2773 item 2.1 follow-up).",
        extensions=[".cpp", ".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
