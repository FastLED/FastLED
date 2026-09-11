"""The normative Bradford matrix must match ICC in both copies.

FastLED#4034 makes Bradford the normative chromatic adaptation transform,
applied whenever a source white differs from the working D65. Its cone
response matrix is hard-coded twice:

* ``src/fl/gfx/chromatic_adaptation.cpp.hpp`` -- ``kBradford``
* ``ci/color_reference.py`` -- inside ``bradford_adaptation()``

Neither was pinned to the published values.

``test_bradford_inverse_matches_the_general_solver`` pins ``kBradfordInverse``
against ``invert3x3(kBradford, ...)``, which is what lets the inverse be a
constant rather than a bind-time inversion. It is self-referential with
respect to the forward matrix: mistype ``0.8951`` as ``0.8591`` and it still
passes, having inverted the typo perfectly consistently.

The end-to-end budget does not close the gap either. The golden corpus is
generated from the Python copy and ``tests/fl/gfx/pipeline.cpp`` measures the
C++ copy against it, so both copies wrong the same way keeps dE2000 at
0.395096 and fires nothing, while one copy wrong moves the budget but names
the pipeline rather than a mistyped constant.

So both are checked here against the specification rather than against each
other -- matching the copies would pass if someone "corrected" both in the
same wrong direction. Same reasoning as FastLED#4352 for the source
primaries.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CXX = PROJECT_ROOT / "src" / "fl" / "gfx" / "chromatic_adaptation.cpp.hpp"
REFERENCE = PROJECT_ROOT / "ci" / "color_reference.py"

# ICC.1:2022 Annex E, linear Bradford cone response.
ICC_BRADFORD = (
    (0.8951, 0.2664, -0.1614),
    (-0.7502, 1.7135, 0.0367),
    (0.0389, -0.0685, 1.0296),
)

kNumber = re.compile(r"-?\d+\.\d+")


def _rows_after(text: str, anchor: str, count: int) -> tuple[tuple[float, ...], ...]:
    """The next `count` brace/paren rows of three numbers after `anchor`."""

    start = text.index(anchor)
    rows: list[tuple[float, ...]] = []
    for line in text[start:].splitlines():
        found = kNumber.findall(line)
        if len(found) != 3:
            continue
        values: list[float] = []
        for token in found:
            values.append(float(token))
        rows.append(tuple(values))
        if len(rows) == count:
            break
    return tuple(rows)


def _invert3x3(
    m: tuple[tuple[float, ...], ...],
) -> tuple[tuple[float, ...], ...]:
    """Cofactor inverse, written out rather than pulled from a dependency."""

    a, b, c = m[0]
    d, e, f = m[1]
    g, h, i = m[2]
    determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if determinant == 0.0:
        raise ValueError("singular matrix")
    cofactors = (
        (e * i - f * h, c * h - b * i, b * f - c * e),
        (f * g - d * i, a * i - c * g, c * d - a * f),
        (d * h - e * g, b * g - a * h, a * e - b * d),
    )
    rows: list[tuple[float, ...]] = []
    for row in cofactors:
        scaled: list[float] = []
        for value in row:
            scaled.append(value / determinant)
        rows.append(tuple(scaled))
    return tuple(rows)


class TestBradfordMatrixAgrees(unittest.TestCase):
    def test_cxx_matches_icc(self: "TestBradfordMatrixAgrees") -> None:
        source = CXX.read_text(encoding="utf-8")
        rows = _rows_after(source, "kBradford[3][3]", 3)
        self.assertEqual(len(rows), 3, msg="kBradford: expected three rows")
        for index in range(3):
            with self.subTest(row=index):
                self.assertEqual(rows[index], ICC_BRADFORD[index])

    def test_reference_matches_icc(self: "TestBradfordMatrixAgrees") -> None:
        # The copy the golden corpus is generated from.
        source = REFERENCE.read_text(encoding="utf-8")
        rows = _rows_after(source, "bradford = Matrix3(", 3)
        self.assertEqual(len(rows), 3, msg="reference bradford: expected three rows")
        for index in range(3):
            with self.subTest(row=index):
                self.assertEqual(rows[index], ICC_BRADFORD[index])

    def test_the_hard_coded_inverse_is_the_inverse_of_icc(
        self: "TestBradfordMatrixAgrees",
    ) -> None:
        # An independent path to the same constant. The C++ guard pins
        # `kBradfordInverse` against `invert3x3(kBradford, ...)`, which is
        # sound now that `kBradford` is pinned above -- but it still routes
        # through FastLED's own inversion. This inverts the *published*
        # matrix here instead, so a bug in `invert3x3` cannot certify its own
        # output.
        #
        # This replaces two `assertIn` checks for the strings
        # "kBradfordInverse" and "invert3x3", which asserted nothing:
        # `invert3x3` appears in that header only inside a comment, so the
        # test passed on prose.
        source = CXX.read_text(encoding="utf-8")
        rows = _rows_after(source, "kBradfordInverse[3][3]", 3)
        self.assertEqual(len(rows), 3, msg="kBradfordInverse: expected three rows")

        expected = _invert3x3(ICC_BRADFORD)
        # places=7 is 5e-8. The constant is float32, whose ulp near these
        # magnitudes is about 1.5e-8, so the gap between a correctly rounded
        # float32 and this float64 inverse is a few ulps and fits inside that
        # bound -- while a mistyped digit anywhere in the nine transcribed
        # decimals does not. Measured rather than assumed: at places=6 a
        # seventh-decimal change passed, which is how this number was chosen.
        for row in range(3):
            for column in range(3):
                with self.subTest(row=row, column=column):
                    self.assertAlmostEqual(
                        rows[row][column], expected[row][column], places=7
                    )


if __name__ == "__main__":
    unittest.main()
