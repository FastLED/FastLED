"""The P7 algorithm-selection study must keep reproducing its conclusion.

`docs/color-gamut-algorithm-selection.md` selects OKLCh chroma compression on
the strength of these numbers. If a later change to the reference or the
harness moved them, the recorded decision would quietly stop being supported
by anything.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from ci.color_gamut_study import (
    CANDIDATES,
    emitter_matrix,
    is_feasible,
    score_candidate,
)
from ci.color_reference import Xyz, _invert_3x3


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
