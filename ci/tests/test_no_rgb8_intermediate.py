"""B3: the streaming path must not round-trip through RGB8.

P6 (#4040) asks for decode to be one semantic conversion -- an RGB8 code
straight to linear light -- with no intermediate RGB8 linear buffer and no
RGB8 to RGB8 correction anywhere on the per-pixel path. That is a structural
property, not something a numeric test can see: a pipeline that quantized to
8 bits somewhere in the middle would still produce plausible colours, just
worse ones, and the error would look like rounding rather than like a
missing requirement.

So this asserts the shape instead. Companion to
test_no_iterative_solver_per_pixel.py, which guards A3/B11 the same way.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
GFX = PROJECT_ROOT / "src" / "fl" / "gfx"

# The composition, and the stages it runs per pixel. `pipeline.cpp.hpp` is
# the one that would grow an intermediate buffer if anyone added one.
PER_PIXEL_STAGES = (
    "pipeline.cpp.hpp",
    "transfer.cpp.hpp",
    "source_xyz.cpp.hpp",
    "chromatic_adaptation.cpp.hpp",
    "device_solve.cpp.hpp",
    "gamut_map.cpp.hpp",
    "white_allocation.cpp.hpp",
    "flux_scalar.cpp.hpp",
)

# A declaration of one of these on the per-pixel path is the RGB8 round trip
# B3 forbids. `CRGB` and `CRGBW` are the 8-bit pixel types; a bare `u8`
# array is the hand-rolled version of the same thing.
FORBIDDEN_TYPES = ("CRGB", "CRGBW")

# `u8 name[` -- an 8-bit buffer. Single `u8` scalars are fine and common
# (a code coming in, a channel count), which is why this looks for the
# array form specifically rather than for the type.
U8_BUFFER = re.compile(r"\bu8\s+\w+\s*\[")


def strip_comments_and_strings(source: str) -> str:
    """Remove block comments, line comments and string literals.

    Without this the check trips on prose: every one of these files
    *discusses* RGB8, because explaining why there is no RGB8 intermediate is
    exactly what their comments are for. The same reasoning as the sibling
    test's -- a regex over C++ is a deliberate trade, and the cost of getting
    it wrong here is a false alarm rather than a silently weakened guard,
    because a stripped-out declaration would still have to appear somewhere
    this test looks.
    """

    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.S)
    source = re.sub(r"//[^\n]*", " ", source)
    source = re.sub(r'"(?:[^"\\]|\\.)*"', '""', source)
    return source


class TestNoRgb8Intermediate(unittest.TestCase):
    def test_the_stage_files_all_exist(self: "TestNoRgb8Intermediate") -> None:
        # Without this the checks below pass vacuously the moment a file is
        # renamed, which is exactly when the guard is most needed.
        for name in PER_PIXEL_STAGES:
            self.assertTrue((GFX / name).is_file(), msg=f"missing stage {name}")

    def test_no_rgb8_pixel_type_on_the_per_pixel_path(
        self: "TestNoRgb8Intermediate",
    ) -> None:
        for name in PER_PIXEL_STAGES:
            with self.subTest(stage=name):
                code = strip_comments_and_strings(
                    (GFX / name).read_text(encoding="utf-8")
                )
                for forbidden in FORBIDDEN_TYPES:
                    self.assertNotRegex(
                        code,
                        rf"\b{forbidden}\b",
                        msg=(
                            f"{name} references {forbidden} outside a comment. "
                            "The streaming path decodes RGB8 to linear light in "
                            "one step (B3); an 8-bit pixel type in the middle "
                            "of it is the round trip #4040 rules out."
                        ),
                    )

    def test_no_u8_buffer_on_the_per_pixel_path(
        self: "TestNoRgb8Intermediate",
    ) -> None:
        for name in PER_PIXEL_STAGES:
            with self.subTest(stage=name):
                code = strip_comments_and_strings(
                    (GFX / name).read_text(encoding="utf-8")
                )
                match = U8_BUFFER.search(code)
                self.assertIsNone(
                    match,
                    msg=(
                        f"{name} declares an 8-bit buffer "
                        f"({match.group(0) if match else ''}). B3 allows a u8 "
                        "code in and a u8 code out, but nothing 8-bit in "
                        "between."
                    ),
                )

    def test_the_check_can_actually_fail(self: "TestNoRgb8Intermediate") -> None:
        # A structural test that cannot fail is worth nothing, and this one is
        # all regexes over text. Feed it the shape it is meant to catch.
        planted = "void f() { u8 scratch[3]; CRGB mid; }"
        self.assertIsNotNone(U8_BUFFER.search(planted))
        self.assertRegex(strip_comments_and_strings(planted), r"\bCRGB\b")
        # And the comment stripping really does hide prose, which is the
        # other half of the trade.
        prose = "// CRGB is deliberately absent here\nvoid f() {}"
        self.assertNotRegex(strip_comments_and_strings(prose), r"\bCRGB\b")


if __name__ == "__main__":
    unittest.main()
