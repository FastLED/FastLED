"""P9: a linked TINY binary carries no float and no solver symbols (#4043).

`ci/tests/test_no_float_per_pixel.py` checks the source-level precondition --
no floating point in the per-pixel functions -- and is explicit that it does
not check the thing P9 actually promises:

    What this does NOT claim: that a linked TINY binary contains no float
    symbols. That is a link-time property needing `bash bloat` on a real
    target, and it remains P9's open half.

This is that half. It reads the symbol table of an ATtiny85 build -- 512 B of
SRAM, `FL_IS_AVR_ATTINY_TINY_MEMORY`, so `FL_PLATFORM_HAS_TINY_MEMORY` is 1 and
`FL_COLOR_PROFILE_RUNTIME` is 0 -- and asserts the soft-float helpers and the
colour-pipeline solver are absent.

Skipped rather than building, following `test_elf.py`: the build needs the AVR
toolchain and takes ~40 s, which does not belong in a unit suite that runs on
every pull request. Run

    bash compile attiny85 --examples Blink

and this checks what it produced.
"""

from __future__ import annotations

import shutil
import subprocess
import unittest
import warnings
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ELF = (
    PROJECT_ROOT
    / ".build"
    / "pio"
    / "attiny85"
    / ".fbuild"
    / "build"
    / "release"
    / "firmware.elf"
)

# avr-gcc's soft-float helpers. An ATtiny has no FPU, so any float arithmetic
# that survives to link time arrives as a call to one of these -- which is what
# makes their absence a real check rather than a restatement of the source
# rule.
kSoftFloatSymbols = (
    "__addsf3",
    "__subsf3",
    "__mulsf3",
    "__divsf3",
    "__cmpsf2",
    "__floatsisf",
    "__floatunsisf",
    "__fixsfsi",
    "__fixunssfsi",
    "__gesf2",
    "__lesf2",
    "__nesf2",
)

# The pipeline stages B11 keeps off this tier entirely.
kSolverFragments = (
    "nnls",
    "gamut",
    "oklab",
    "bradford",
    "white_allocation",
    "device_solve",
    "colorimetric",
)


def _avr_nm() -> str | None:
    found = shutil.which("avr-nm")
    if found is not None:
        return found
    cache = Path.home() / ".fbuild"
    if not cache.is_dir():
        return None
    for candidate in cache.rglob("avr-nm"):
        if candidate.is_file():
            return str(candidate)
    return None


class TestTinyLinksNoFloat(unittest.TestCase):
    def setUp(self: "TestTinyLinksNoFloat") -> None:
        if not ELF.is_file():
            warnings.warn(
                "Skipping TestTinyLinksNoFloat because "
                f"{ELF.relative_to(PROJECT_ROOT)} does not exist. "
                "Run 'bash compile attiny85 --examples Blink' to generate it."
            )
            self.skipTest("attiny85 build missing")
        nm = _avr_nm()
        if nm is None:
            self.skipTest("avr-nm not found; build attiny85 to fetch the toolchain")
        # `subprocess.run` and not `RunningProcess.run`: the latter merges
        # stderr into stdout, and a symbol table is not something to parse out
        # of a merged stream.
        completed = subprocess.run(  # noqa: SRC001
            [nm, str(ELF)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        self.assertEqual(completed.returncode, 0, msg=completed.stderr)
        self.symbols = completed.stdout

    def test_the_build_actually_contains_fastled(
        self: "TestTinyLinksNoFloat",
    ) -> None:
        """Without this, an empty or failed build satisfies everything below.

        That is the way a link-time absence test goes wrong: nothing is
        present, so nothing forbidden is present either.
        """

        self.assertIn("FastLED", self.symbols)
        self.assertIn("CPixelLEDController", self.symbols)

    def test_no_soft_float_helper_is_linked(self: "TestTinyLinksNoFloat") -> None:
        offenders: list[str] = []
        for symbol in kSoftFloatSymbols:
            if symbol in self.symbols:
                offenders.append(symbol)
        self.assertEqual(
            offenders,
            [],
            msg=(
                "TINY tier linked avr-gcc soft-float helpers: "
                f"{offenders}. B11 puts the fixed-point tier on this target "
                "precisely so these do not appear."
            ),
        )

    def test_no_pipeline_solver_is_linked(self: "TestTinyLinksNoFloat") -> None:
        lowered = self.symbols.lower()
        offenders: list[str] = []
        for fragment in kSolverFragments:
            if fragment in lowered:
                offenders.append(fragment)
        self.assertEqual(
            offenders,
            [],
            msg=(
                f"TINY tier linked colour-pipeline solver symbols: {offenders}. "
                "FL_COLOR_PROFILE_RUNTIME is 0 here, so none of this should "
                "survive --gc-sections."
            ),
        )


if __name__ == "__main__":
    unittest.main()
