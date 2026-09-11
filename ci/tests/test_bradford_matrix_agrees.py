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

    def test_the_inverse_is_still_derived_from_this_matrix(
        self: "TestBradfordMatrixAgrees",
    ) -> None:
        # Not a re-test of the C++ inverse guard, which lives in
        # tests/fl/gfx/. This only pins that the header still says the inverse
        # is derived rather than independently typed, so the two constants
        # cannot drift apart silently if that comment stops being true.
        source = CXX.read_text(encoding="utf-8")
        self.assertIn("kBradfordInverse", source)
        self.assertIn("invert3x3", source)


if __name__ == "__main__":
    unittest.main()
