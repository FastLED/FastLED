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
    Primaries,
    corpus_primaries,
    emitter_matrix_f32,
    f32,
    from_q16,
    invert3x3_f32,
    invert3x3_q16,
    quantize_rows,
    rounded_div,
    to_q16,
)
from ci.color_fixed_profile_study import (
    EmitterLuminances,
    compare_profile_quantization,
    emitter_matrix_f32_lum,
    emitter_matrix_q16,
    kLuminanceSets,
    kQ16One,
    kUnitLuminance,
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
            q16_built = emitter_matrix_q16(primaries, kUnitLuminance)
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
            for luminances in kLuminanceSets:
                measured = compare_profile_quantization(name, primaries, 9, luminances)
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

    def test_luminance_is_priced_too_and_does_not_move_the_answer(self) -> None:
        """The half the study originally left out.

        `EmitterProfile` carries `lum_r`, `lum_g` and `lum_b` and does not
        require them to be 1, but the first version of this study fixed them
        at unity. Quantising a luminance is a second perturbation and it
        lands on the same columns the divisor sensitivity already stresses,
        so leaving it out priced item 2's precondition only halfway.

        Measured across four luminance sets and the four-profile corpus, the
        worst case is still BT.2020 at unit luminance, 0.1085 dE2000. Every
        non-unit set comes in at or below 0.031.
        """

        worst_unit = 0.0
        worst_non_unit = 0.0
        for name, primaries in corpus_primaries():
            for luminances in kLuminanceSets:
                measured = compare_profile_quantization(name, primaries, 9, luminances)
                if luminances.label == "unit":
                    worst_unit = max(worst_unit, measured.delta_e)
                else:
                    worst_non_unit = max(worst_non_unit, measured.delta_e)

        # The published figure is the unit case, and it stays the binding one.
        self.assertGreater(worst_unit, worst_non_unit)
        self.assertLess(worst_non_unit, 0.05)

    def test_the_luminance_sets_actually_perturb_the_matrix(self) -> None:
        """Vacuity guard for the case above.

        A luminance set that produced the same Q16 matrix as unity would make
        that comparison a number against itself. `dim-blue` scales an emitter
        by 0.05, so the columns must differ -- and the point is that they
        differ *without* the colour error following.
        """

        _name, primaries = corpus_primaries()[0]
        unit = emitter_matrix_q16(primaries, kUnitLuminance)
        self.assertIsNotNone(unit)
        assert unit is not None
        for luminances in kLuminanceSets:
            if luminances.label == "unit":
                continue
            scaled = emitter_matrix_q16(primaries, luminances)
            self.assertIsNotNone(scaled, f"{luminances.label} reported degenerate")
            assert scaled is not None
            differing = sum(
                1
                for row in range(3)
                for col in range(3)
                if unit[row][col] != scaled[row][col]
            )
            self.assertGreater(
                differing, 0, f"{luminances.label} matched unit luminance"
            )

    def test_coefficient_ulps_are_not_a_proxy_for_colour_error(self) -> None:
        """Recorded because the two separate, and one is the misleading one.

        `narrow` with `lopsided` luminances moves 1515 coefficient ULPs and
        still lands 0.0135 dE2000, while BT.2020 at unit luminance moves 2
        ULPs and lands 0.1085 -- eight times the colour error for a
        seven-hundredth of the coefficient movement. Conditioning moves the
        coefficients; the small divisor moves the colour.

        So a future change must not conclude from a small ULP count that the
        colour is fine, nor from a large one that it is not.
        """

        by_name = dict(corpus_primaries())
        lopsided = next(item for item in kLuminanceSets if item.label == "lopsided")
        narrow = compare_profile_quantization("narrow", by_name["narrow"], 9, lopsided)
        bt2020 = compare_profile_quantization(
            "bt2020", by_name["bt2020"], 9, kUnitLuminance
        )
        self.assertGreater(narrow.coefficient_ulps, 100 * bt2020.coefficient_ulps)
        self.assertLess(narrow.delta_e, bt2020.delta_e)

    def test_the_float_baseline_keeps_the_shipped_grouping(self) -> None:
        """The baseline models `xyY_to_XYZ`, not the candidate.

        `colorimetric_response::xyY_to_XYZ` is:

            const float inv_y = 1.0f / y;
            out[0] = x * Y * inv_y;

        which C++ groups left to right, so it scales before it divides.
        `emitter_matrix_f32_lum` has to reproduce that even though it is the
        less accurate arrangement -- it is the thing being measured against,
        and a baseline that quietly adopted the candidate's better order
        would understate what quantising the profile costs.

        Review proposed exactly that change, reading the candidate's order as
        the shipped one. It was the wrong way round, and nothing here caught
        it: the swap passed every other case in this file. This is that gap.
        """

        for _name, primaries in corpus_primaries():
            for luminances in kLuminanceSets:
                baseline = emitter_matrix_f32_lum(primaries, luminances)
                scale = luminances.per_emitter()
                for column, ((x, y), luminance) in enumerate(zip(primaries, scale)):
                    inv_y = f32(1.0 / f32(y))
                    # The shipped grouping, spelled out.
                    shipped_x = f32(f32(f32(x) * f32(luminance)) * inv_y)
                    self.assertEqual(baseline[0][column], shipped_x)
                    z = f32(1.0 - f32(x) - f32(y))
                    shipped_z = f32(f32(z * f32(luminance)) * inv_y)
                    self.assertEqual(baseline[2][column], shipped_z)

    def test_the_two_float_groupings_are_distinguishable(self) -> None:
        """Vacuity guard for the case above.

        If float32 rounded both groupings identically, that case would hold
        no matter which the baseline used, and the swap it exists to catch
        would slip through anyway.
        """

        differing = 0
        for _name, primaries in corpus_primaries():
            for luminances in kLuminanceSets:
                for (x, y), luminance in zip(primaries, luminances.per_emitter()):
                    inv_y = f32(1.0 / f32(y))
                    scale_then_divide = f32(f32(f32(x) * f32(luminance)) * inv_y)
                    divide_then_scale = f32(f32(f32(x) * inv_y) * f32(luminance))
                    if scale_then_divide != divide_then_scale:
                        differing += 1
        self.assertGreater(differing, 0)

    def test_the_divide_then_scale_order_is_the_more_accurate_one(self) -> None:
        """Why `emitter_matrix_q16` diverges from the shipped operation order.

        `xyY_to_XYZ` is `out[0] = x * Y * inv_y`, which C++ groups left to
        right, so the float path scales before it divides. The Q16 candidate
        divides first, which is the opposite -- and is deliberate, because in
        fixed point `x * Y` throws away low bits the divide would have used.

        Twice this comment has said the reverse. It first claimed reversing
        the order "would stop modelling the shipped path", and then that
        divide-first *was* the shipped order. Review caught the second. So
        this stops describing the order and measures it, against the float32
        baseline both are trying to reproduce -- including the one case where
        divide-first comes off worse, which a third telling would probably
        have left out.
        """

        def scale_first(
            primaries: Primaries, luminances: EmitterLuminances
        ) -> list[list[int]]:
            columns: list[list[int]] = []
            for (x_float, y_float), luminance_float in zip(
                primaries, luminances.per_emitter()
            ):
                x = to_q16(x_float)
                y = to_q16(y_float)
                luminance = to_q16(luminance_float)
                z = kQ16One - x - y
                columns.append(
                    [
                        rounded_div(rounded_div(x * luminance, kQ16One) * kQ16One, y),
                        luminance,
                        rounded_div(rounded_div(z * luminance, kQ16One) * kQ16One, y),
                    ]
                )
            return [[columns[0][k], columns[1][k], columns[2][k]] for k in range(3)]

        differed = 0
        divide_first_worse = 0
        worst_divide_first = 0
        worst_scale_first = 0
        for _name, primaries in corpus_primaries():
            for luminances in kLuminanceSets:
                baseline = quantize_rows(
                    invert3x3_f32(emitter_matrix_f32_lum(primaries, luminances))
                )
                candidate = emitter_matrix_q16(primaries, luminances)
                self.assertIsNotNone(candidate)
                assert candidate is not None
                inverses = {
                    "divide": invert3x3_q16(candidate),
                    "scale": invert3x3_q16(scale_first(primaries, luminances)),
                }
                errors = {}
                for label, inverse in inverses.items():
                    self.assertIsNotNone(inverse)
                    assert inverse is not None
                    errors[label] = max(
                        abs(baseline[row][col] - inverse[row][col])
                        for row in range(3)
                        for col in range(3)
                    )
                if errors["divide"] != errors["scale"]:
                    differed += 1
                if errors["divide"] > errors["scale"]:
                    divide_first_worse += 1
                worst_divide_first = max(worst_divide_first, errors["divide"])
                worst_scale_first = max(worst_scale_first, errors["scale"])

        # Vacuity guard: if the orders never diverged, everything below would
        # be comparing a number with itself. They agree at unit luminance,
        # where scaling by 1.0 makes them the same expression, so only the
        # non-unit sets can separate them.
        self.assertGreater(differed, 0)

        # Better in 8 of the 16 combinations, tied in 7 -- the unit sets,
        # where the two are the same expression -- and worse in exactly one:
        # `narrow`/`typical`, 113 ULP against 92. So "never worse" is not the
        # claim, and was the claim until this counted it.
        self.assertEqual(divide_first_worse, 1)

        # What carries the choice is the extreme, not the average: 2899 ULP
        # against 7363 on `narrow` with a near-dark blue. The one case
        # divide-first loses, it loses by 21 ULP.
        self.assertLess(worst_divide_first * 2, worst_scale_first)

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
        self.assertIsNone(emitter_matrix_q16(degenerate, kUnitLuminance))

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
