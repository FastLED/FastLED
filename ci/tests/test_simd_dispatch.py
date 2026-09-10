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


@typechecked
def _arm_cross_compiler() -> str | None:
    """An `arm-none-eabi-gcc`, if this machine has one."""

    found = shutil.which("arm-none-eabi-gcc")
    if found is not None:
        return found
    # PlatformIO and fbuild both keep one under the user's cache.
    for root in (Path.home() / ".platformio" / "packages", Path.home() / ".fbuild"):
        if not root.is_dir():
            continue
        for candidate in root.rglob("bin/arm-none-eabi-gcc"):
            return str(candidate)
    return None


@typechecked
def _predefined(compiler: str, flags: list[str], macro: str) -> str:
    """`macro`'s value in `compiler`'s predefined set under `flags`, or ""."""

    completed = subprocess.run(  # noqa: SRC001
        [compiler, *flags, "-dM", "-E", "-x", "c", "-"],
        input="",
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    for line in completed.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0] == "#define" and parts[1] == macro:
            return parts[2]
    return ""


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

    def test_cortex_m33_with_the_dsp_extension_takes_the_dsp_backend(self) -> None:
        """RP2350, and why FastLED#4216 was filed against the wrong file.

        That issue reported a 15x slowdown in the packed `s16x16x4` type on an
        RP2350W and attributed it to `simd_noop.hpp`, on the reasoning that
        "RP2350 is Cortex-M33 without `__ARM_FEATURE_DSP`". The Cortex-M33
        does not have to carry the DSP extension, but the RP2350's does, and
        the Arduino-Pico core compiles for it: its `rp2350` branch passes
        `-mcpu=cortex-m33 -march=armv8-m.main+fp+dsp`, and GCC then defines
        `__ARM_FEATURE_DSP` to 1. So the arm below claims the target and
        `simd_noop.hpp` is never reached.

        Two of that issue's experiments -- dropping `FL_ALIGNAS(16)`, and
        unrolling the trip-4 lane loops -- were made in `simd_noop.hpp` and
        measured no change, which was then read as evidence against both
        ideas. Neither edit was in the build. The unroll in particular works:
        it is what `FL_SIMD_LANE4` now does.

        The arm this lands on is written for "Cortex-M4 / M4F / M7", which is
        where the misreading starts; ARMv8-M parts reach it too.
        """

        compiler = _arm_cross_compiler()
        if compiler is None:
            raise unittest.SkipTest("no arm-none-eabi-gcc on this machine")

        # Verbatim from the Arduino-Pico core's `rp2350` branch,
        # framework-arduinopico/tools/platformio-build.py.
        rp2350_flags = [
            "-mcpu=cortex-m33",
            "-mthumb",
            "-march=armv8-m.main+fp+dsp",
            "-mfloat-abi=softfp",
        ]
        dsp = _predefined(compiler, rp2350_flags, "__ARM_FEATURE_DSP")
        self.assertEqual(dsp, "1", "an RP2350 build defines __ARM_FEATURE_DSP")

        backend, fallback = _select([f"-D__ARM_FEATURE_DSP={dsp}"])
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

    def test_msvc_arm64_is_not_covered_and_that_is_recorded(self) -> None:
        """MSVC on ARM64 keeps the fallback, deliberately.

        MSVC advertises `_M_ARM64` rather than `__ARM_NEON`, and spells the
        NEON header `arm64_neon.h`. So the arm added for FastLED#4286 does
        not catch it, and the `_M_ARM64` branches already inside
        `simd_arm_neon.hpp` remain unreachable there.

        That is the status quo rather than a regression -- the target took
        the fallback before the arm existed too -- and switching a whole
        backend on for a toolchain that nothing here can compile for, let
        alone run, is not a thing to do on inference. Pinned so it reads as
        a decision, and so that whoever adds MSVC ARM64 support has to
        change this case on purpose.
        """

        backend, fallback = _select(["-D_M_ARM64=1"])
        self.assertEqual(backend, "platforms/shared/simd_noop.hpp")
        self.assertEqual(fallback, "1")

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
