"""The reference's OKLab must be the space the pipeline quantizes.

OKLCh is A3's normative objective space -- the gamut mapper's objective is
stated in it. Its two matrices exist in three places:

* ``src/fl/gfx/oklab_q16.cpp.hpp`` -- s16.16 integers, with the float values
  they were quantized from written in the comments beside them;
* ``ci/color_reference.py`` -- float64, and what the golden corpus's
  ``mapped_xyz`` targets are computed with;
* ``tests/fl/gfx/oklab_q16.cpp`` -- a float transcription, which is what
  "OKLab Q16 forward agrees with the float definition" scores the integers
  against.

The C++ side has a property test: D65 goes to lightness 1 with no chroma. That
is the right kind of check here, because these are *not* Ottosson's originally
published coefficients -- they are his corrected higher-precision ones, the
ones for which that property holds exactly. Pinning against the published
table would fail a correct implementation.

The reference had no such test, and it is the copy the corpus is generated
from. If it drifted, the mapper's targets and the mapper would be working in
different spaces while each stayed self-consistent -- so every comparison
between them would still agree with itself.

Within the reference alone M1 is hand-copied three times (``_oklab_from_xyz``,
the ``Matrix3`` literal ``_xyz_from_oklab`` inverts, and the sign-flipped
absolute-value copy bounding the zonotope chroma cap) and M2 twice. The
behavioural checks below cover the first two; the bound copies are compared as
text, since a bound is not required to round-trip.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path
from typing import TypeAlias

from ci.color_reference import (
    _D65,
    Lab,
    Xyz,
    _oklab_from_xyz,
    _xy_to_xyz,
    _xyz_from_oklab,
)


PROJECT_ROOT = Path(__file__).resolve().parents[2]
Q16_SOURCE = PROJECT_ROOT / "src" / "fl" / "gfx" / "oklab_q16.cpp.hpp"
REFERENCE = PROJECT_ROOT / "ci" / "color_reference.py"

Matrix: TypeAlias = tuple[Xyz, Xyz, Xyz]

kSignedFloat = re.compile(r"[+-]\d+\.\d+")
kAnyFloat = re.compile(r"\d+\.\d+")

# Off-white as well as neutral: a matrix error that cancels on the achromatic
# axis still has to show up somewhere.
kSamples: tuple[tuple[str, Xyz], ...] = (
    ("d65 white", _xy_to_xyz(_D65)),
    ("dim neutral", (0.19009, 0.2, 0.21781)),
    ("warm", (0.4, 0.2, 0.05)),
    ("cool", (0.08, 0.12, 0.4)),
    ("green", (0.1, 0.3, 0.06)),
)


def _documented(anchor: str) -> Matrix:
    """The float values written beside a quantized matrix in the C++."""

    source = Q16_SOURCE.read_text(encoding="utf-8")
    start = source.index(anchor)
    rows: list[Xyz] = []
    for line in source[start:].splitlines():
        if "//" not in line:
            continue
        found = kSignedFloat.findall(line.split("//", 1)[1])
        if len(found) != 3:
            continue
        rows.append((float(found[0]), float(found[1]), float(found[2])))
        if len(rows) == 3:
            break
    assert len(rows) == 3, f"{anchor}: found {len(rows)} documented rows, want 3"
    return (rows[0], rows[1], rows[2])


def _matvec(matrix: Matrix, vector: Xyz) -> Xyz:
    values: list[float] = []
    for row in matrix:
        total = 0.0
        for index in range(3):
            total += row[index] * vector[index]
        values.append(total)
    return (values[0], values[1], values[2])


M1 = _documented("constexpr i32 kLmsFromXyz")
M2 = _documented("constexpr i32 kOklabFromLmsRoot")


def _oklab_from_documented(xyz: Xyz) -> Lab:
    lms = _matvec(M1, xyz)
    roots: list[float] = []
    for value in lms:
        roots.append(
            abs(value) ** (1.0 / 3.0) if value >= 0.0 else -(abs(value) ** (1.0 / 3.0))
        )
    return _matvec(M2, (roots[0], roots[1], roots[2]))


def _bound_floats(start_anchor: str, end_anchor: str) -> list[float]:
    source = REFERENCE.read_text(encoding="utf-8")
    start = source.index(start_anchor)
    end = source.index(end_anchor, start)
    values: list[float] = []
    for token in kAnyFloat.findall(source[start:end]):
        values.append(float(token))
    return values


class TestOklabReferenceMatrix(unittest.TestCase):
    def test_d65_is_lightness_one_with_no_chroma(
        self: "TestOklabReferenceMatrix",
    ) -> None:
        # OKLab's defining normalization, and the property the corrected
        # coefficients exist to satisfy. The C++ pins this; the reference did
        # not.
        lightness, a_value, b_value = _oklab_from_xyz(_xy_to_xyz(_D65))
        self.assertAlmostEqual(lightness, 1.0, places=7)
        self.assertAlmostEqual(a_value, 0.0, places=7)
        self.assertAlmostEqual(b_value, 0.0, places=7)

    def test_forward_agrees_with_the_quantized_matrices(
        self: "TestOklabReferenceMatrix",
    ) -> None:
        # The corpus's targets are computed here; the device solve runs the
        # s16.16 integers. Both have to mean the same transform, or the mapper
        # is scored in a space it does not work in.
        #
        # The comments carry ten decimals, so a matrix built from them differs
        # from float64 by 2.3e-11 here; places=9 leaves twenty times that as
        # headroom. Measured by mutation: a slip in a coefficient's ninth
        # decimal fails this. A slip in the tenth -- the last digit the
        # comments express at all -- does not, and this does not claim to
        # catch one.
        for name, xyz in kSamples:
            with self.subTest(colour=name):
                expected = _oklab_from_documented(xyz)
                actual = _oklab_from_xyz(xyz)
                for index in range(3):
                    self.assertAlmostEqual(actual[index], expected[index], places=9)

    def test_inverse_agrees_with_the_forward_copy(
        self: "TestOklabReferenceMatrix",
    ) -> None:
        # `_xyz_from_oklab` inverts its own `Matrix3` literal, a separate
        # transcription from the one `_oklab_from_xyz` spells out inline.
        for name, xyz in kSamples:
            with self.subTest(colour=name):
                recovered = _xyz_from_oklab(_oklab_from_xyz(xyz))
                for index in range(3):
                    self.assertAlmostEqual(recovered[index], xyz[index], places=9)

    def test_the_chroma_bound_uses_the_same_magnitudes(
        self: "TestOklabReferenceMatrix",
    ) -> None:
        # The zonotope chroma cap re-copies both matrices with every sign
        # forced positive, so it cannot round-trip and the checks above do not
        # reach it. Its magnitudes still have to be these magnitudes: a cap
        # built from drifted coefficients stops bounding what it claims to.
        expected: list[float] = []
        for row in M1:
            for value in row:
                expected.append(abs(value))
        self._assert_same(
            "lms_bound", _bound_floats("lms_bound = (", "root_bound = "), expected
        )

        # Rows 2 and 3 of M2 -- the cap is on (a, b), not lightness.
        chroma: list[float] = []
        for row in (M2[1], M2[2]):
            for value in row:
                chroma.append(abs(value))
        self._assert_same(
            "a_bound/b_bound", _bound_floats("a_bound = (", "chroma_cap = "), chroma
        )

    def _assert_same(
        self: "TestOklabReferenceMatrix",
        what: str,
        found: list[float],
        expected: list[float],
    ) -> None:
        # The bound spells the coefficients at full float64; the comments stop
        # at ten decimals, which is all they claim.
        self.assertEqual(len(found), len(expected), msg=f"{what}: coefficient count")
        for index in range(len(expected)):
            self.assertAlmostEqual(
                found[index], expected[index], places=10, msg=f"{what}[{index}]"
            )


if __name__ == "__main__":
    unittest.main()
