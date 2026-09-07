"""A3/B11: iterative solvers must never run on the per-pixel path.

`nnls3` is a 500-iteration projected-gradient solve. It is legitimate at
profile/cache build time and catastrophic per pixel, and #4041 asks for a
structural test rather than a convention. This asserts the streaming stages
never reference it, so a future edit that wires one in fails here rather than
in a frame-rate report on someone's bench.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
GFX = PROJECT_ROOT / "src" / "fl" / "gfx"

# The stages that execute once per pixel inside show().
PER_PIXEL_STAGES = (
    "transfer.cpp.hpp",
    "source_xyz.cpp.hpp",
    "chromatic_adaptation.cpp.hpp",
    "device_solve.cpp.hpp",
    "flux_scalar.cpp.hpp",
)

# Names whose presence on a per-pixel path would mean an unbounded or
# iterative computation per frame.
FORBIDDEN = ("nnls3", "solve_rgb_colorimetric", "build_rgb_colorimetric_cache")


def call_pattern(symbol: str) -> re.Pattern[str]:
    """Match an actual call to `symbol`, not a mention of its name.

    Deliberately not comment-stripping. Stripping with a regex treats `//`
    inside a string literal as a comment start -- `"https://x"; nnls3(...)`
    would lose the call and the assertion would pass while the reference
    stood. Matching the call shape instead needs no stripping: prose in the
    comments here says "nnls3" without a following parenthesis, so it does
    not match, while any real invocation does.
    """

    return re.compile(r"\b" + re.escape(symbol) + r"\s*\(")


class TestNoIterativeSolverPerPixel(unittest.TestCase):
    def test_stage_files_exist(self: "TestNoIterativeSolverPerPixel") -> None:
        # Guards against the list silently going stale if a file is renamed:
        # a missing file would otherwise make this suite vacuously pass.
        for name in PER_PIXEL_STAGES:
            with self.subTest(stage=name):
                self.assertTrue((GFX / name).is_file(), f"{name} not found in {GFX}")

    def test_no_iterative_solver_on_the_per_pixel_path(
        self: "TestNoIterativeSolverPerPixel",
    ) -> None:
        for name in PER_PIXEL_STAGES:
            text = (GFX / name).read_text(encoding="utf-8")
            for symbol in FORBIDDEN:
                with self.subTest(stage=name, symbol=symbol):
                    self.assertIsNone(
                        call_pattern(symbol).search(text),
                        f"{name} calls {symbol} on the per-pixel path; "
                        "iterative solves belong at profile/cache build time "
                        "(A3/B11).",
                    )


if __name__ == "__main__":
    unittest.main()
