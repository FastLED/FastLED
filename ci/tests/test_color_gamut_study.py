"""The P7 algorithm-selection study must keep reproducing its conclusion.

`docs/color-gamut-algorithm-selection.md` selects OKLCh chroma compression on
the strength of these numbers. If a later change to the reference or the
harness moved them, the recorded decision would quietly stop being supported
by anything.
"""

from __future__ import annotations

import json
import math
import unittest
from pathlib import Path

from ci.color_gamut_study import (
    CANDIDATES,
    D65_WHITE,
    attainable_lightness,
    cbrt_q16,
    emitter_matrix,
    integer_cube_root,
    is_feasible,
    oklch_q16,
    score_candidate,
)
from ci.color_reference import (
    Xyz,
    _invert_3x3,
    _oklch_from_xyz,
    _xyz_from_oklab,
    delta_e2000,
    xyz_to_lab,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
GOLDEN = PROJECT_ROOT / "ci" / "golden" / "color-reference-v1.json"

# The corpus's `rgb` device: sRGB primaries at unit luminance.
RGB_PRIMARIES = ((0.6400, 0.3300), (0.3000, 0.6000), (0.1500, 0.0600))


def out_of_gamut_cases() -> list[tuple[Xyz, Xyz]]:
    corpus = json.loads(GOLDEN.read_text(encoding="utf-8"))
    cases: list[tuple[Xyz, Xyz]] = []
    for vector in corpus["vectors"]:
        if vector["device_profile"] != "rgb":
            continue
        stages = dict(vector["stages"])
        d65 = stages["d65_xyz"]
        mapped = stages["mapped_xyz"]
        target: Xyz = (d65[0], d65[1], d65[2])
        reference: Xyz = (mapped[0], mapped[1], mapped[2])
        moved = max(abs(a - b) for a, b in zip(target, reference))
        if moved > 1e-12:
            cases.append((target, reference))
    return cases


class TestColorGamutStudy(unittest.TestCase):
    def setUp(self: "TestColorGamutStudy") -> None:
        self.forward = emitter_matrix(RGB_PRIMARIES)
        self.inverse = _invert_3x3(self.forward)
        self.cases = out_of_gamut_cases()

    def test_the_corpus_actually_contains_out_of_gamut_vectors(
        self: "TestColorGamutStudy",
    ) -> None:
        # Without this the whole study would pass vacuously on an empty set.
        self.assertGreaterEqual(len(self.cases), 20)

    def test_oklch_compression_reproduces_the_reference(
        self: "TestColorGamutStudy",
    ) -> None:
        score = score_candidate("oklch-bisect", self.forward, self.inverse, self.cases)
        self.assertEqual(score.vector_count, len(self.cases))
        # The reference searches the zonotope globally; bisection assumes the
        # chroma ray is monotonic. On this corpus they agree exactly.
        self.assertLess(score.worst_delta_e, 0.5)

    def test_cheap_mappers_miss_the_budget_by_a_wide_margin(
        self: "TestColorGamutStudy",
    ) -> None:
        # This is the finding the report rests on: the objective matters, and
        # no amount of clamping substitutes for it.
        for name in ("clip", "max-normalize", "desaturate-to-neutral"):
            with self.subTest(candidate=name):
                score = score_candidate(name, self.forward, self.inverse, self.cases)
                self.assertGreater(score.worst_delta_e, 10.0)

    def test_every_candidate_returns_a_feasible_result(
        self: "TestColorGamutStudy",
    ) -> None:
        # A mapper that returns something still outside the hull has not
        # mapped anything; the solve downstream would produce negative drives.
        for name, mapper in CANDIDATES.items():
            for target, _ in self.cases:
                mapped = mapper(self.forward, self.inverse, target)
                with self.subTest(candidate=name, target=target):
                    self.assertTrue(is_feasible(self.inverse, mapped))

    def test_the_selected_algorithm_meets_the_a1_budget(
        self: "TestColorGamutStudy",
    ) -> None:
        # The report selects eight halvings. If this stops holding, the
        # recorded selection is no longer supported by anything.
        score = score_candidate(
            "oklch-bisect-8", self.forward, self.inverse, self.cases
        )
        self.assertLess(score.worst_delta_e, 0.5)

    def test_bounded_search_beats_a_large_lookup_table(
        self: "TestColorGamutStudy",
    ) -> None:
        # The selection rests on this comparison: eight halvings need no
        # storage and still beat a 16 KB table, because a grid cannot
        # represent the gamut boundary's corners at the primaries.
        LARGEST_LUT_WORST_DELTA_E = 2.434  # 64 x 128 conservative, 16 KB
        eight = score_candidate(
            "oklch-bisect-8", self.forward, self.inverse, self.cases
        )
        self.assertLess(eight.worst_delta_e, LARGEST_LUT_WORST_DELTA_E)

    def test_s16_16_precision_does_not_degrade_the_selected_algorithm(
        self: "TestColorGamutStudy",
    ) -> None:
        """Quantizing to the working domain must not cost accuracy.

        The report selects eight halvings on float64 numbers while the
        embedded path runs in s16.16, so the equivalence is pinned rather
        than assumed.

        The lightness search is quantized too, including its internal branch
        decisions. Rounding only its returned value leaves 30 comparisons in
        float64 and does not exercise the fixed-point path at all -- an
        earlier version of this test did that and reported numbers that did
        not survive.
        """

        def quantize(value: float) -> float:
            return round(value * 65536) / 65536

        def quantized_point(lightness: float, a: float, b: float) -> Xyz:
            point = _xyz_from_oklab((lightness, a, b))
            return (quantize(point[0]), quantize(point[1]), quantize(point[2]))

        def attainable_quantized(lightness: float) -> float:
            if is_feasible(self.inverse, quantized_point(lightness, 0.0, 0.0)):
                return quantize(lightness)
            low, high = 0.0, lightness
            for _ in range(30):
                middle = quantize((low + high) / 2.0)
                if is_feasible(self.inverse, quantized_point(middle, 0.0, 0.0)):
                    low = middle
                else:
                    high = middle
            return low

        worst = 0.0
        for target, reference in self.cases:
            polar = _oklch_from_xyz(target)
            lightness = attainable_quantized(polar.lightness)
            hue = math.radians(polar.hue_degrees)
            cosine, sine = quantize(math.cos(hue)), quantize(math.sin(hue))
            low, high = 0.0, quantize(polar.chroma)
            for _ in range(8):
                chroma = quantize((low + high) / 2.0)
                trial = quantized_point(
                    lightness, quantize(chroma * cosine), quantize(chroma * sine)
                )
                if is_feasible(self.inverse, trial):
                    low = chroma
                else:
                    high = chroma
            mapped = quantized_point(
                lightness, quantize(low * cosine), quantize(low * sine)
            )
            self.assertTrue(is_feasible(self.inverse, mapped))
            worst = max(
                worst,
                delta_e2000(
                    xyz_to_lab(mapped, D65_WHITE), xyz_to_lab(reference, D65_WHITE)
                ),
            )
        # Pin the documented result, not merely the A1 budget. Asserting only
        # `< 0.5` would accept a threefold degradation from 0.152 without
        # noticing, which is the regression this test exists to catch.
        S16_16_BASELINE = 0.152
        self.assertLess(worst, S16_16_BASELINE * 1.05)

    def _map_with_fixed_point_root(
        self: "TestColorGamutStudy", ulp_error: int
    ) -> float:
        """Worst dE2000 of the selected mapper, cube root included."""

        def quantize(value: float) -> float:
            return round(value * 65536) / 65536

        def quantized_point(lightness: float, a: float, b: float) -> Xyz:
            point = _xyz_from_oklab((lightness, a, b))
            return (quantize(point[0]), quantize(point[1]), quantize(point[2]))

        def attainable_quantized(lightness: float) -> float:
            if is_feasible(self.inverse, quantized_point(lightness, 0.0, 0.0)):
                return quantize(lightness)
            low, high = 0.0, lightness
            for _ in range(30):
                middle = quantize((low + high) / 2.0)
                if is_feasible(self.inverse, quantized_point(middle, 0.0, 0.0)):
                    low = middle
                else:
                    high = middle
            return low

        worst = 0.0
        for target, reference in self.cases:
            polar = oklch_q16(target, ulp_error)
            lightness = attainable_quantized(polar.lightness)
            hue = math.radians(polar.hue_degrees)
            cosine, sine = quantize(math.cos(hue)), quantize(math.sin(hue))
            low, high = 0.0, quantize(polar.chroma)
            for _ in range(8):
                trial_chroma = quantize((low + high) / 2.0)
                trial = quantized_point(
                    lightness,
                    quantize(trial_chroma * cosine),
                    quantize(trial_chroma * sine),
                )
                if is_feasible(self.inverse, trial):
                    low = trial_chroma
                else:
                    high = trial_chroma
            mapped = quantized_point(
                lightness, quantize(low * cosine), quantize(low * sine)
            )
            self.assertTrue(is_feasible(self.inverse, mapped))
            worst = max(
                worst,
                delta_e2000(
                    xyz_to_lab(mapped, D65_WHITE), xyz_to_lab(reference, D65_WHITE)
                ),
            )
        return worst

    def test_integer_cube_root_is_exact(self: "TestColorGamutStudy") -> None:
        # The defining property, not a float comparison: this models
        # `fl::icbrt64`, and a model that inherited float's rounding would
        # not be evidence about the integer implementation.
        for value in [0, 1, 7, 8, 9, 26, 27, 28, 10**6, (1 << 63)]:
            root = integer_cube_root(value)
            self.assertLessEqual(root**3, value)
            self.assertGreater((root + 1) ** 3, value)
        self.assertEqual(integer_cube_root((1 << 64) - 1), 2642245)

    def test_cbrt_q16_matches_the_real_cube_root(
        self: "TestColorGamutStudy",
    ) -> None:
        # Truncation toward zero, so the error is in [0, 1) ULP of Q16 --
        # measured against the cube root of the *quantized* input, which is
        # the only thing the fixed-point path is given. Comparing against the
        # root of the real value instead folds in the input's own rounding,
        # and near zero the cube root amplifies that sharply: at v = 0.01 the
        # derivative is about 7, so half a ULP in turns into three and a half
        # out. That is a property of the domain, not of this implementation.
        for numerator in range(1, 400):
            value = numerator / 97.0
            quantized_input = round(value * 65536) / 65536
            got = cbrt_q16(value)
            want = quantized_input ** (1.0 / 3.0)
            self.assertLessEqual(got, want)
            self.assertGreater(got, want - 1.0 / 65536)
        self.assertEqual(cbrt_q16(-8.0), -2.0)
        self.assertEqual(cbrt_q16(27.0), 3.0)

    def test_a_fixed_point_cube_root_does_not_degrade_the_mapper(
        self: "TestColorGamutStudy",
    ) -> None:
        """The claim `src/fl/math/fixed_point/icbrt.h` exists to support.

        The earlier precision test quantized every stage *around* the cube
        root while computing the root itself in float64 -- so it measured the
        stages on either side of it. Driving the forward transform with the
        integer root closes that gap.
        """

        S16_16_BASELINE = 0.152
        self.assertLess(self._map_with_fixed_point_root(0), S16_16_BASELINE * 1.05)

    def test_the_cube_root_accuracy_cliff_is_where_it_is_documented(
        self: "TestColorGamutStudy",
    ) -> None:
        """Pins how much cube-root error the A1 budget can absorb.

        This is what says the exact integer root is not merely sufficient but
        has room to spare, and it keeps the door open for a cheaper
        approximation: anything holding under ~64 ULP would also pass. Without
        it, `icbrt.h`'s recorded table is an unverifiable comment.
        """

        A1_BUDGET = 0.5
        for ulp_error in (64, -64):
            with self.subTest(ulp_error=ulp_error):
                self.assertLess(self._map_with_fixed_point_root(ulp_error), A1_BUDGET)
        # And it is a real cliff, not an asymptote -- a root off by 1024 ULP
        # blows the budget outright.
        for ulp_error in (1024, -1024):
            with self.subTest(ulp_error=ulp_error):
                self.assertGreater(
                    self._map_with_fixed_point_root(ulp_error), A1_BUDGET
                )

    def test_over_bright_targets_are_mapped_into_the_hull(
        self: "TestColorGamutStudy",
    ) -> None:
        # Chroma compression alone cannot rescue a target that is too bright:
        # at zero chroma it is still outside, so a chroma-only mapper returns
        # an infeasible result. The corpus contains no such target, which is
        # why this needed constructing rather than finding.
        white: Xyz = (
            sum(self.forward.row0),
            sum(self.forward.row1),
            sum(self.forward.row2),
        )
        over_bright: Xyz = (white[0] * 1.5, white[1] * 1.5, white[2] * 1.5)
        self.assertFalse(is_feasible(self.inverse, over_bright))
        for name, mapper in CANDIDATES.items():
            with self.subTest(candidate=name):
                mapped = mapper(self.forward, self.inverse, over_bright)
                self.assertTrue(is_feasible(self.inverse, mapped))

    def test_feasible_chroma_is_one_interval_for_this_device(
        self: "TestColorGamutStudy",
    ) -> None:
        """Bisection assumes the feasible chroma ray is connected.

        The reference does not assume that, so the assumption is checked here
        rather than asserted in prose. A coarse sweep on every run; the dense
        1.9M-sample sweep behind the report's claim is recorded there.
        """

        for lightness_step in range(1, 10):
            lightness = lightness_step / 10.0
            for hue_degrees in range(0, 360, 30):
                radians = math.radians(hue_degrees)
                seen_infeasible = False
                for chroma_step in range(0, 61):
                    chroma = chroma_step * 0.5 / 60
                    point = _xyz_from_oklab(
                        (
                            lightness,
                            chroma * math.cos(radians),
                            chroma * math.sin(radians),
                        )
                    )
                    if is_feasible(self.inverse, point):
                        with self.subTest(lightness=lightness, hue=hue_degrees):
                            self.assertFalse(
                                seen_infeasible,
                                "feasible chroma is disconnected here; "
                                "bisection would discard the far interval",
                            )
                    else:
                        seen_infeasible = True

    def test_feasibility_checks_both_bounds(
        self: "TestColorGamutStudy",
    ) -> None:
        # Checking only the lower bound accepts a target needing drives above
        # full scale -- too bright rather than too saturated -- which no
        # device can produce and which the mappers would return unchanged.
        forward = self.forward
        over_bright = (
            forward.row0[0] * 2.0,
            forward.row1[0] * 2.0,
            forward.row2[0] * 2.0,
        )
        self.assertFalse(is_feasible(self.inverse, over_bright))

        at_full_scale = (forward.row0[0], forward.row1[0], forward.row2[0])
        self.assertTrue(is_feasible(self.inverse, at_full_scale))

    def test_scores_cover_every_case_rather_than_a_subset(
        self: "TestColorGamutStudy",
    ) -> None:
        # A candidate scored on fewer cases than another is not comparable to
        # it, so the harness must count them all or raise.
        for name in CANDIDATES:
            with self.subTest(candidate=name):
                score = score_candidate(name, self.forward, self.inverse, self.cases)
                self.assertEqual(score.vector_count, len(self.cases))

    def test_in_gamut_targets_pass_through_untouched(
        self: "TestColorGamutStudy",
    ) -> None:
        # Exact preservation in gamut is an acceptance criterion of #4041.
        in_gamut: Xyz = (0.2, 0.25, 0.22)
        self.assertTrue(is_feasible(self.inverse, in_gamut))
        for name in ("desaturate-to-neutral", "oklch-bisect"):
            with self.subTest(candidate=name):
                mapped = CANDIDATES[name](self.forward, self.inverse, in_gamut)
                for got, want in zip(mapped, in_gamut):
                    self.assertAlmostEqual(got, want, places=12)


if __name__ == "__main__":
    unittest.main()
