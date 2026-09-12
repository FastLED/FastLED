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

Skipping is the right default and was, on its own, the whole problem: nothing
built an ATtiny85 before this ran, so it skipped everywhere, every time, and
P9's link-time half was asserted by a test that never executed a single
assertion. `check_attiny85.yml` now runs it in the job that has just built
the ELF, with `FL_REQUIRE_TINY_ELF=1` set -- which turns a missing build from
a skip into a failure. Without that, wiring it up would be worth nothing: the
first thing to move the ELF would return this file to passing vacuously, and
nothing would say so.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import unittest
import warnings
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]

# Where an attiny85 build lands. Both roots and a recursive glob, rather than
# one hardcoded path, because `ci/compiled_size.py` documents two fbuild
# layouts -- `<build_dir>/.fbuild/build/release/firmware.elf` and
# `<build_dir>/.fbuild/build/<env>/release/firmware.elf` -- and falls back
# between two build roots. Pinning one of those spellings meant this file
# silently found nothing whenever the other was produced.
kBuildRoots = (
    PROJECT_ROOT / ".build" / "pio" / "attiny85",
    PROJECT_ROOT / ".build" / "attiny85",
)


def _find_elf() -> "Path | None":
    """Newest attiny85 firmware ELF under either build root, or None."""

    newest: Path | None = None
    for root in kBuildRoots:
        for candidate in root.glob(".fbuild/build/**/firmware.elf"):
            if not candidate.is_file():
                continue
            # Newest wins, matching the rule `ci/bloat.py` settled on in
            # FastLED#4386: two layouts can coexist under one board directory
            # and a fixed preference reports on whichever is stale.
            if newest is None or candidate.stat().st_mtime > newest.stat().st_mtime:
                newest = candidate
    return newest


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


# Set by the CI job that has just built the ELF. A skip there means the build
# it was supposed to inspect is not where it expects, which is a failure of
# that job, not a reason to report success.
kRequireElfEnvVar = "FL_REQUIRE_TINY_ELF"


def _elf_is_required() -> bool:
    """Whether a missing build must fail rather than skip."""

    return os.environ.get(kRequireElfEnvVar, "") not in ("", "0")


class TestTinyLinksNoFloat(unittest.TestCase):
    def setUp(self: "TestTinyLinksNoFloat") -> None:
        required = _elf_is_required()
        elf = _find_elf()
        if elf is None:
            searched = ", ".join(
                str(root.relative_to(PROJECT_ROOT)) for root in kBuildRoots
            )
            if required:
                self.fail(
                    f"{kRequireElfEnvVar} is set, so this must inspect a real "
                    f"build, but no firmware.elf was found under {searched}. "
                    "Either the attiny85 build did not run before this step or "
                    "it wrote somewhere else; skipping here would report P9's "
                    "link-time check as satisfied without having read a single "
                    "symbol."
                )
            warnings.warn(
                "Skipping TestTinyLinksNoFloat because no attiny85 "
                f"firmware.elf was found under {searched}. "
                "Run 'bash compile attiny85 --examples Blink' to generate it."
            )
            self.skipTest("attiny85 build missing")
        nm = _avr_nm()
        if nm is None:
            if required:
                self.fail(
                    f"{kRequireElfEnvVar} is set but avr-nm was not found; the "
                    "toolchain that produced the ELF should provide it."
                )
            self.skipTest("avr-nm not found; build attiny85 to fetch the toolchain")
        # `subprocess.run` and not `RunningProcess.run`: the latter merges
        # stderr into stdout, and a symbol table is not something to parse out
        # of a merged stream.
        completed = subprocess.run(  # noqa: SRC001
            [nm, str(elf)],
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
