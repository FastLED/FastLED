"""The quantised-profile study measures what it claims to.

`ci/color_fixed_profile_study.py` prices FastLED#4043 (P9) item 2's
precondition: whether the emitter matrix can be *built* in fixed point from
Q16 chromaticities, not merely inverted in fixed point from a float-built
one. These cases guard the parts of that claim a reader would otherwise have
to take on trust -- above all that the candidate really differs from the
baseline, since a study whose two arms have silently converged reports a
budget it never tested.
"""

from __future__ import annotations

import unittest

from ci.color_fixed_inverse_study import (
    corpus_primaries,
    emitter_matrix_f32,
    from_q16,
    quantize_rows,
    to_q16,
)
from ci.color_fixed_profile_study import (
    compare_profile_quantization,
    emitter_matrix_q16,
    main,
    quantization_residual,
)


class TestQuantizedProfileStudy(unittest.TestCase):
    """The measurement, and the reasons to believe it."""

    def test_the_two_arms_actually_differ(self) -> None:
        """Vacuity guard, and the most important case here.

        If quantising the chromaticities first produced the same matrix the
        float path builds, every dE2000 in the table would be zero and the
        study would be reporting "inside budget" without having measured
        anything.
        """

        differing = 0
        for _name, primaries in corpus_primaries():
            float_built = quantize_rows(emitter_matrix_f32(primaries))
            q16_built = emitter_matrix_q16(primaries)
            self.assertIsNotNone(q16_built)
            assert q16_built is not None
            for row in range(3):
                for col in range(3):
                    if float_built[row][col] != q16_built[row][col]:
                        differing += 1
        self.assertGreater(differing, 0, "the Q16 build matched float exactly")

    def test_the_headline_figure_is_inside_a1(self) -> None:
        """The result the issue comment reports, pinned.

        Measured worst is 0.1085 dE2000 on BT.2020 against A1's 0.5, of
        which the derivation has roughly 0.35 once the gamut mapper's 0.15
        is accounted for.
        """

        worst = 0.0
        for name, primaries in corpus_primaries():
            measured = compare_profile_quantization(name, primaries, 9)
            if measured.delta_e > worst:
                worst = measured.delta_e
        self.assertLess(worst, 0.35)
        # And a band around the measured 0.1085, tight enough that the
        # figure the issue comment reports is the figure this checks. A
        # single `assertLess(worst, 0.15)` admitted 0.149 -- materially worse
        # than what was reported, and passing.
        #
        # The band is +/- 5%, which is far outside anything the arithmetic
        # can drift by (both arms are deterministic integer paths over a
        # fixed corpus and a fixed sweep) and far inside the 38% headroom to
        # the budget. A change that moves this at all should say so.
        self.assertGreater(worst, 0.1031)
        self.assertLess(worst, 0.1139)

    def test_bt2020_is_the_worst_and_its_smallest_y_is_why(self) -> None:
        """The mechanism the study claims, checked rather than asserted.

        `xyY_to_XYZ` divides by `y`, so the profile with the smallest `y`
        takes the largest relative perturbation from a Q16 grid. BT.2020's
        blue sits at y = 0.046, the smallest number in the corpus.
        """

        by_name = dict(corpus_primaries())
        residuals = {}
        for name, primaries in by_name.items():
            residuals[name] = quantization_residual(primaries)
        shipped = ("srgb", "bt2020", "display_p3")
        worst_shipped = max(shipped, key=lambda name: residuals[name])
        self.assertEqual(worst_shipped, "bt2020")

        smallest_y = min(y for _x, y in by_name["bt2020"])
        self.assertLess(smallest_y, 0.05)

    def test_a_chromaticity_that_quantises_to_zero_y_is_refused(self) -> None:
        """The degenerate case the shipped guard already rejects.

        `isUsableSolveChromaticity` refuses a non-positive `y`; a fixed-point
        build has a second way to reach it, since a `y` below half a Q16 step
        quantises to zero and would divide by it.
        """

        degenerate = ((0.64, 0.33), (0.30, 0.60), (0.15, 1.0 / 200000.0))
        self.assertEqual(to_q16(1.0 / 200000.0), 0)
        self.assertIsNone(emitter_matrix_q16(degenerate))

    def test_q16_round_trip_is_within_one_step(self) -> None:
        """The residual the table reports is a quantisation residual."""

        for _name, primaries in corpus_primaries():
            for x, y in primaries:
                for value in (x, y):
                    self.assertLessEqual(
                        abs(from_q16(to_q16(value)) - value), 1.0 / 65536.0
                    )

    def test_the_entry_point_reports_success(self) -> None:
        """`main` returns zero while the corpus stays inside the budget."""

        self.assertEqual(main(["--steps", "5"]), 0)


if __name__ == "__main__":
    unittest.main()
