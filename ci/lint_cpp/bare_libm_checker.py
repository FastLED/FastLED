#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker that bans bare C `::sqrtf` / `::atan2` / `::powf` / etc.

Rationale (FastLED #3002, #3012): on no-FPU MCUs (Cortex-M0/M0+, AVR,
classic ESP32 in `-mfpu=none` builds, etc.), the libm `sqrtf` / `atan2`
/ `ldexpf` / `acosf` / ... family transitively anchors libgcc's full
soft-double helper set (`__aeabi_dadd`, `__aeabi_dmul`, `__aeabi_ddiv`,
`__ledf2`, `__gedf2`, ...). On the LPC845-BRK JSON-RPC bring-up sketch
that was ~6 KB of overhead — more than 10 % of the entire 64 KB flash
budget — for math operations the sketch never actually performed.

The fix (PR #3012) routes `fl::sqrt(x)`, `fl::atan2(y, x)`, etc. through
hand-rolled polynomial / Newton-Raphson approximations on Low-memory
targets, only falling through to libm on Large-memory targets where the
soft-FP weight is irrelevant. This checker enforces the discipline by
banning direct `::sqrtf` / `::atan2` / etc. calls in `src/`, forcing
new code to go through `fl::` and inherit the same gating.

Suppression:
- Add `// ok libm` at end of line to whitelist a specific call.
- `fl/math/math.cpp.hpp`, `fl/math/math.h`, and the checker itself are
  always exempt (they ARE the compat shim).
- Third-party upstream code (`src/third_party/`) is exempt — we don't
  control those sources.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

# Whitelisted files inside src/ that legitimately need the C symbols
# (the fl::math shim itself).
EXEMPT_FILES = (
    "fl/math/math.cpp.hpp",
    "fl/math/math.h",
)

# Whitelisted directory prefixes (under src/) that are pre-existing heavy
# users of bare libm and will be migrated in a follow-up PR. The discipline
# this checker enforces is "no NEW code that imports libm directly"; the
# existing audio/FFT/animation/graphics subsystems already anchor libm in
# their own translation units (audio FFT in particular requires full IEEE
# 754 precision) and will be ported through `fl::` over a series of
# focused refactors. Tracked in #3002 follow-up.
EXEMPT_PREFIXES = (
    # Audio / FFT / detector chain — scientific math, precision-sensitive.
    "fl/audio/",
    # FX 2D / animation / Animartrix port — heavy trig + fixed-point.
    "fl/fx/",
    # Graphics xypath generators — geometric curves through trig.
    "fl/gfx/",
    # fl::math's own fixed_point subsystem — has its OWN compat layer.
    "fl/math/fixed_point",
    # Other fl/math utilities that pre-date the gate — migrate separately.
    "fl/math/line_simplification",
    "fl/math/screenmap",
    "fl/math/transform",
    # ESP32 has FPU + plenty of flash; libm cost is negligible.
    "platforms/esp/",
    # WASM is host-side; libm comes from emscripten.
    "platforms/wasm/",
)

# Libm functions banned from direct use in src/. Each name is paired with
# both its `f`-suffixed (float) and unsuffixed (double) form.
BANNED_FUNCS = (
    # Trigonometry
    "sin",
    "sinf",
    "cos",
    "cosf",
    "tan",
    "tanf",
    "asin",
    "asinf",
    "acos",
    "acosf",
    "atan",
    "atanf",
    "atan2",
    "atan2f",
    # Power / log / exponential
    "sqrt",
    "sqrtf",
    "pow",
    "powf",
    "exp",
    "expf",
    "exp2",
    "exp2f",
    "log",
    "logf",
    "log10",
    "log10f",
    "log2",
    "log2f",
    # IEEE-754 manipulation
    "ldexp",
    "ldexpf",
    "frexp",
    "frexpf",
    "scalbn",
    "scalbnf",
    # Modulo / hypot / fabs / round
    "fmod",
    "fmodf",
    "hypot",
    "hypotf",
    "fabs",
    "fabsf",
    "lround",
    "lroundf",
    "round",
    "roundf",
)

# Match `<func>(` and require it to be an actual call site:
#   - Preceded by start-of-line, whitespace, or `(`/`,`/`!`/`=`/`?`/`:`/`+`/`-`/`*`/`/`/`>`/`<`/`&`/`|`/`~`/`^`/`%`/`{`/`}`/`;`
#   - NOT preceded by `fl::`, `T::`, `.`, `->`, or an identifier character
#
# Examples that MATCH (banned):
#   sqrtf(x)
#   ::sqrtf(x)        — explicit global qualification, still libm-anchored
#   return atan2(y, x);
#   ldexpf(value, 5)
#
# Examples that DO NOT MATCH (allowed):
#   fl::sqrtf(x)
#   fl::math::sqrtf(x)
#   obj.sqrtf(x)
#   ptr->sqrtf(x)
#   my_sqrtf(x)
#   pow_impl_float(x, y)   — fl::math's own impl wrappers (substring match
#                            avoided by the identifier-boundary rule)
_BANNED_PATTERN = re.compile(
    # Disallow leading identifier-char (so `my_sin(` doesn't match)
    # and disallow leading `:` (so `fl::sin(` is okay — colon before
    # `fl::` is captured by `[A-Za-z0-9_:]`).
    r"(?<![A-Za-z0-9_:])"
    # Allow `::` prefix (explicit global) — those ARE banned.
    r"(?:::)?(" + "|".join(BANNED_FUNCS) + r")\s*\("
)
_SUPPRESS = "// ok libm"


class BareLibmChecker(FileContentChecker):
    """Bans bare C libm calls inside src/ (PR #3012 / issue #3002).

    Forces new code to go through `fl::sqrt(x)`, `fl::atan2(y, x)`, etc.,
    which on Low-memory targets route to hand-rolled polynomial /
    Newton-Raphson approximations rather than dragging libm + libgcc
    soft-FP into the link.
    """

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.startswith(str(SRC_ROOT)):
            return False
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False
        norm = file_path.replace("\\", "/")
        # The fl::math compat shim is exempt — it IS the wrapper.
        for exempt in EXEMPT_FILES:
            if norm.endswith(exempt):
                return False
        # Third-party upstream code (stb_vorbis, cq_kernel, etc.) is not
        # under our authorship; the checker would generate noise.
        if "/third_party/" in norm:
            return False
        # Subsystems still on the migration list (see EXEMPT_PREFIXES). New
        # code in these directories should still prefer fl::, but the
        # checker stays silent for now so this PR doesn't block on a 34-file
        # refactor.
        for prefix in EXEMPT_PREFIXES:
            # Match `src/<prefix>` anywhere in the normalized path.
            if "/src/" + prefix in norm or norm.startswith(prefix):
                return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str]] = []
        in_block_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track /* ... */ blocks. Single-line `/* ... */` is handled
            # by the trailing-comment strip below.
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

            # Strip trailing // comment so the regex doesn't match inside it.
            code = line.split("//")[0]
            if _BANNED_PATTERN.search(code):
                violations.append((line_number, line.rstrip()))

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    from ci.util.check_files import run_checker_standalone

    checker = BareLibmChecker()
    run_checker_standalone(
        checker,
        [str(SRC_ROOT)],
        "Found bare C libm call (e.g. ::sqrtf, atan2, ldexpf) in src/. "
        "Use fl::sqrt / fl::atan2 / fl::ldexp instead — see PR #3012, "
        "issue #3002. Add `// ok libm` to whitelist a specific call.",
        extensions=[".cpp", ".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
