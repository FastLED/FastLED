"""Detect transient zccache daemon failures (exit 113) so the build can retry.

zccache wraps every compiler invocation in the meson build. When its daemon
is under load or unhealthy it sometimes returns exit code 113 without ever
running the compiler, and ninja reports it as::

    FAILED: [code=113] ci/meson/native/fastled.so.p/..._third_party+.cpp.o
    "/.../.venv/bin/zccache" /.../ctc-clang++ -I... -c ../../src/fl/build/third_party+.cpp
    zccache[info][Q]: daemon under load: queued, position 1 of 3 queued, 3 in flight

113 is zccache's exit code, not clang's. A real compile error prints a
diagnostic (``file:line:col: error: ...``) and is deterministic; these print
nothing and move between objects on reruns of the same commit.

This module provides:
  * ``ZCCACHE_TRANSIENT_EXIT_CODE`` — the exit code zccache uses.
  * ``find_zccache_transient_failures`` — the objects that failed with 113.
  * ``is_zccache_transient_failure`` — True when the build failed *only*
    because of 113s, i.e. no compiler diagnostic accompanies any failure.
  * ``format_zccache_transient_message`` — a self-identifying explanation.

Related: FastLED issue #4132.
"""

from __future__ import annotations

import re


ZCCACHE_TRANSIENT_EXIT_CODE = 113

# Example line (ninja's failure banner, possibly ANSI-coloured):
#   FAILED: [code=113] ci/meson/native/fastled.so.p/.._.._.._src_fl_build_third_party+.cpp.o
_FAILED_113_PATTERN = re.compile(
    r"FAILED:\s*\[code=113\]\s*(?P<obj>\S+)",
)

# Any real compiler/linker diagnostic. Matches clang-style
# ``path:line:col: error:`` and driver-style ``clang: error:`` / ``ld.lld:
# error:`` lines. zccache's own log lines are tagged ``zccache[...]`` and are
# excluded so its chatter can never be mistaken for a compile error.
_COMPILER_ERROR_PATTERN = re.compile(
    r"(?m)^(?!zccache\[)[^\n]*\b(?:fatal )?error:",
)

# The meson build passes -fdiagnostics-color=always, so clang emits
# ``\x1b[0;1;31merror: \x1b[0m`` — no word boundary before ``error``. Strip
# SGR sequences before matching so coloured diagnostics are still seen.
_ANSI_SGR_PATTERN = re.compile(r"\x1b\[[0-9;]*m")


def _strip_ansi(text: str) -> str:
    return _ANSI_SGR_PATTERN.sub("", text)


def find_zccache_transient_failures(output: str) -> list[str]:
    """Return the object paths that ninja reported as ``FAILED: [code=113]``."""
    if not output:
        return []
    plain = _strip_ansi(output)
    return [m.group("obj") for m in _FAILED_113_PATTERN.finditer(plain)]


def is_zccache_transient_failure(output: str) -> bool:
    """Return True if the build failed only with zccache exit 113 and no diagnostic.

    A 113 that sits next to a genuine ``error:`` line anywhere in the output
    is NOT treated as transient: the retry would just replay a real defect.
    """
    if not find_zccache_transient_failures(output):
        return False
    return _COMPILER_ERROR_PATTERN.search(_strip_ansi(output)) is None


def format_zccache_transient_message(objects: list[str]) -> str:
    """Explain a 113 failure in one read, the way fbuild's daemon errors do."""
    listed = "\n".join(f"    {obj}" for obj in objects)
    return (
        f"zccache returned exit code {ZCCACHE_TRANSIENT_EXIT_CODE} with no "
        f"compiler diagnostic for {len(objects)} object(s):\n{listed}\n"
        f"  This is a zccache daemon problem (overloaded or unhealthy), not a "
        f"defect in the code being built — no compilation was attempted. "
        f"See FastLED #4132."
    )
