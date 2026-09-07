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

# An 8-bit buffer. Single `u8` scalars are fine and common -- a code coming
# in, a channel count -- so this looks for the array form rather than the
# type.
#
# The scan runs from `u8` to the first `[`, refusing to cross `;{}()` or `=`.
# Crossing `;` would let one statement's `u8` match the next statement's
# subscript; refusing `=` is what keeps `u8 value = table[i];` from reading
# as a declaration, since there the bracket is a subscript on the right-hand
# side. What it does catch, and the simpler `u8 \w+ \[` did not, is the
# second declarator in `u8 code, intermediate[3];`.
U8_BUFFER = re.compile(r"\bu8\s+[^;{}()=]*\[")


# One pass over the four lexical forms that can contain each other's
# delimiters, so precedence is decided by which starts first rather than by
# the order of separate substitutions. Stripping comments before strings
# would treat the `//` in a URL literal as a comment and eat the rest of the
# line; stripping strings before comments would treat a quote inside a
# comment as opening one.
_LEXICAL = re.compile(
    r'"(?:[^"\\\n]|\\.)*"'
    r"|'(?:[^'\\\n]|\\.)*'"
    r"|/\*.*?\*/"
    r"|//[^\n]*",
    re.S,
)


def strip_comments_and_strings(source: str) -> str:
    """Remove block comments, line comments, and string and char literals.

    Without this the check trips on prose: every one of these files
    *discusses* RGB8, because explaining why there is no RGB8 intermediate is
    exactly what their comments are for. The same reasoning as the sibling
    test's -- a regex over C++ is a deliberate trade, and the cost of getting
    it wrong here is a false alarm rather than a silently weakened guard,
    because a stripped-out declaration would still have to appear somewhere
    this test looks.
    """

    def replace(match: "re.Match[str]") -> str:
        text = match.group(0)
        if text.startswith('"'):
            return '""'
        if text.startswith("'"):
            return "''"
        return " "

    return _LEXICAL.sub(replace, source)


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
        # all regexes over text. Feed it the shapes it is meant to catch.
        self.assertIsNotNone(U8_BUFFER.search("void f() { u8 scratch[3]; }"))
        self.assertRegex(
            strip_comments_and_strings("void f() { CRGB mid; }"), r"\bCRGB\b"
        )

        # The second declarator, which an earlier `u8 \w+ \[` pattern missed
        # entirely: `code` is a legitimate scalar and `intermediate` is the
        # buffer B3 forbids, in one statement.
        self.assertIsNotNone(U8_BUFFER.search("u8 code, intermediate[3];"))

    def test_the_check_does_not_fire_on_legitimate_code(
        self: "TestNoRgb8Intermediate",
    ) -> None:
        # The other half. A guard that flags ordinary code gets disabled, so
        # these are the forms it must leave alone.
        for allowed in (
            "u8 code = 0;",  # a scalar
            "u8 value = table[index];",  # a subscript, not a buffer
            "void f(u8 r, u8 g, u8 b) {}",  # parameters
            "const u16 table[256] = {};",  # 16-bit is the working type
        ):
            with self.subTest(source=allowed):
                self.assertIsNone(U8_BUFFER.search(allowed))

    def test_prose_and_literals_are_stripped_correctly(
        self: "TestNoRgb8Intermediate",
    ) -> None:
        # Comments discussing RGB8 are exactly what these files are full of,
        # so the stripping has to hide them.
        self.assertNotRegex(
            strip_comments_and_strings(
                "// CRGB is deliberately absent here\nvoid f() {}"
            ),
            r"\bCRGB\b",
        )

        # And the precedence has to be decided by which form starts first.
        # Stripping comments before strings ate the rest of a line after a
        # URL literal, which would have hidden real code from the check.
        survived = strip_comments_and_strings(
            'const char* url = "http://example.com"; CRGB mid;'
        )
        self.assertRegex(survived, r"\bCRGB\b")

        # The mirror case: a quote inside a comment must not open a string.
        survived = strip_comments_and_strings("// it's fine\nCRGB mid;")
        self.assertRegex(survived, r"\bCRGB\b")


if __name__ == "__main__":
    unittest.main()
