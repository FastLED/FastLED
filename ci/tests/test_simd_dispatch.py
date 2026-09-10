"""The SIMD backend dispatch table selects what it claims to.

`src/platforms/simd.h` is a preprocessor chain that picks one of five SIMD
backends from architecture macros. Nothing tested it, which is how
FastLED#4286 happened: `platforms/arm/simd_arm_neon.hpp` -- a complete
66-operation NEON backend -- had no arm selecting it, so every ARMv8-A
target ran the scalar fallback and the only reference to the file outside
itself was a comment.

A missing arm is invisible: the chain still compiles, still picks *a*
backend, and the one it picks still works. Only the performance is wrong,
and nothing in the suite looks at that.

So this evaluates the real chain with the real preprocessor. Each backend
`#include` is rewritten to a marker first, because the backends reference
target intrinsics (`arm_neon.h`, `emmintrin.h`) that the host does not have
for every target being checked -- the chain's *logic* is what is under test,
not the backends' contents.
"""

from __future__ import annotations

import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from typeguard import typechecked


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
DISPATCH_FILE = PROJECT_ROOT / "src" / "platforms" / "simd.h"

kMarker = "FL_TEST_SELECTED_BACKEND"
kFallbackProbe = "FL_TEST_FALLBACK_FLAG"


@typechecked
def _stub_source() -> str:
    """`simd.h` with each backend include replaced by a marker."""

    source = DISPATCH_FILE.read_text()
    stubbed = re.sub(
        r'#include "(platforms/[^"]*\.hpp)"[^\n]*',
        rf"{kMarker} \1",
        source,
    )
    # `is_platform.h` is a real header the chain needs for FL_IS_ESP32, but
    # pulling it in would drag the platform tree into a -nostdinc run. The
    # macro is supplied directly by the cases below instead.
    stubbed = stubbed.replace('#include "platforms/is_platform.h"', "")
    # `#define` lines do not survive preprocessing, so ask for the value.
    return stubbed + f"\n{kFallbackProbe} FL_SIMD_BACKEND_IS_FALLBACK\n"


@typechecked
def _select(defines: list[str]) -> tuple[str, str]:
    """The backend and fallback flag the chain picks for `defines`."""

    compiler = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
    if compiler is None:
        raise unittest.SkipTest("no C preprocessor on PATH")

    with tempfile.NamedTemporaryFile("w", suffix=".h", delete=False) as handle:
        handle.write(_stub_source())
        path = Path(handle.name)
    try:
        # `subprocess.run` and not `RunningProcess.run`: the latter merges
        # stderr into stdout, and the preprocessor emits a "#pragma once in
        # main file" warning on every invocation here -- merging it would put
        # compiler chatter into the stream this parses.
        completed = subprocess.run(  # noqa: SRC001
            [compiler, "-E", "-P", "-nostdinc", "-undef", *defines, str(path)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    finally:
        path.unlink()

    selected = ""
    fallback = ""
    for raw_line in completed.stdout.splitlines():
        # The preprocessor keeps the chain's indentation, so the marker is
        # not at column zero.
        line = raw_line.strip()
        if line.startswith(kMarker):
            selected = line.split()[-1]
        elif line.startswith(kFallbackProbe):
            fallback = line.split()[-1]
    return selected, fallback


class TestSimdDispatch(unittest.TestCase):
    """One case per target family the chain claims to cover."""

    def test_x86_takes_the_sse_backend(self) -> None:
        backend, fallback = _select(["-D__x86_64__=1"])
        self.assertEqual(backend, "platforms/shared/simd_x86.hpp")
        self.assertEqual(fallback, "0")

    def test_esp32_takes_its_own_backend(self) -> None:
        backend, fallback = _select(["-DFL_IS_ESP32=1"])
        self.assertEqual(backend, "platforms/esp/32/simd_esp32.hpp")
        self.assertEqual(fallback, "0")

    def test_cortex_m4_takes_the_dsp_backend(self) -> None:
        """Teensy 3.x/4.x, SAMD51, nRF52: DSP extension, no NEON."""

        backend, fallback = _select(["-D__ARM_FEATURE_DSP=1"])
        self.assertEqual(backend, "platforms/arm/teensy/simd_arm_dsp.hpp")
        self.assertEqual(fallback, "0")

    def test_aarch64_takes_the_neon_backend(self) -> None:
        """The regression FastLED#4286 was about.

        Apple Silicon, Raspberry Pi 3/4/5 in 64-bit, any ARMv8-A board.
        `__ARM_FEATURE_DSP` is a 32-bit-ARM macro these do not define, so
        before the NEON arm existed they fell all the way through to the
        scalar fallback.
        """

        backend, fallback = _select(["-D__ARM_NEON=1", "-D__aarch64__=1"])
        self.assertEqual(backend, "platforms/arm/simd_arm_neon.hpp")
        self.assertEqual(fallback, "0")

    def test_the_older_neon_spelling_is_accepted(self) -> None:
        """GCC spells it `__ARM_NEON__` on some ARM configurations."""

        backend, _ = _select(["-D__ARM_NEON__=1"])
        self.assertEqual(backend, "platforms/arm/simd_arm_neon.hpp")

    def test_armv7a_with_both_macros_keeps_the_dsp_backend(self) -> None:
        """A deliberate ordering choice, pinned so it cannot drift silently.

        A 32-bit ARMv7-A part can define `__ARM_FEATURE_DSP` and `__ARM_NEON`
        together. The NEON arm sits after the DSP arm so those keep the
        backend they already had; moving them is a separate change with its
        own targets to check.
        """

        backend, _ = _select(["-D__ARM_NEON=1", "-D__ARM_FEATURE_DSP=1"])
        self.assertEqual(backend, "platforms/arm/teensy/simd_arm_dsp.hpp")

    def test_an_unmatched_target_takes_the_fallback_and_says_so(self) -> None:
        """AVR, ESP8266, Cortex-M0/M3, WASM."""

        backend, fallback = _select([])
        self.assertEqual(backend, "platforms/shared/simd_noop.hpp")
        self.assertEqual(fallback, "1")

    def test_every_backend_in_the_tree_is_reachable(self) -> None:
        """The check that would have caught FastLED#4286 on the day.

        A backend file nobody dispatches to is dead code that still compiles,
        still looks maintained, and silently costs every target that should
        have been using it.
        """

        backend_files: list[str] = []
        for path in sorted((PROJECT_ROOT / "src" / "platforms").rglob("simd_*.hpp")):
            backend_files.append(str(path.relative_to(PROJECT_ROOT / "src")))

        # Reachability follows one level of indirection: `simd_esp32.hpp` is
        # itself a dispatch, choosing between the Xtensa and RISC-V backends
        # and the scalar fallback. A backend named by any file the top-level
        # chain selects is reached.
        reached_text = DISPATCH_FILE.read_text()
        for backend in backend_files:
            if backend in reached_text:
                sub_dispatch = PROJECT_ROOT / "src" / backend
                reached_text += sub_dispatch.read_text()

        unreachable: list[str] = []
        for backend in backend_files:
            if backend not in reached_text:
                unreachable.append(backend)
        self.assertEqual(unreachable, [], "no arm of the dispatch selects these")


if __name__ == "__main__":
    unittest.main()
