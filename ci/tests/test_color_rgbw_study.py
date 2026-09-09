"""The white-preferred allocation must keep agreeing with the P5 reference.

`docs/color-gamut-algorithm-selection.md` records that one white emitter
needs no vertex enumeration and that two do. Both halves are measured here,
so neither can quietly stop being true.
"""

from __future__ import annotations

import json
import random
import unittest
from dataclasses import dataclass
from pathlib import Path

from ci.color_reference import Xyz, _invert_3x3, _matvec
from ci.color_rgbw_study import (
    ENUMERATION_TOLERANCE,
    allocate_one_white,
    allocate_two_white,
    best_single_white,
    emitter_column,
    most_white_two,
    rgb_matrix,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
GOLDEN = PROJECT_ROOT / "ci" / "golden" / "color-reference-v1.json"

RGB_COLUMNS = [
    emitter_column(0.6400, 0.3300),
    emitter_column(0.3000, 0.6000),
    emitter_column(0.1500, 0.0600),
]
D65_WHITE_COLUMN = emitter_column(0.3127, 0.3290)
D50_WHITE_COLUMN = emitter_column(0.3457, 0.3585)


@dataclass(frozen=True, slots=True)
class CorpusVector:
    """One reference vector: the target, and the drives the reference chose."""

    target: Xyz
    emitter_light: list[float]

    def __post_init__(self: "CorpusVector") -> None:
        """Check the shape the tests index into.

        These fields come straight out of `json.loads`, so nothing has
        checked them. `@typechecked` would not help: typeguard decorates the
        class before `@dataclass` generates `__init__`, so the generated
        initializer is never instrumented and the decorator validates
        nothing here.

        The tests read `emitter_light[3]` and `[4]` directly. Without this a
        corpus whose shape changed would surface as an IndexError inside an
        assertion loop rather than as a statement about the corpus.
        """

        if len(self.target) != 3:
            raise ValueError(f"target must be XYZ, got {self.target!r}")
        if len(self.emitter_light) < 3:
            raise ValueError(
                f"expected at least three emitter drives, got {self.emitter_light!r}"
            )


def corpus_vectors(profile: str) -> list[CorpusVector]:
    corpus = json.loads(GOLDEN.read_text(encoding="utf-8"))
    out: list[CorpusVector] = []
    for vector in corpus["vectors"]:
        if vector["device_profile"] != profile:
            continue
        stages = vector["stages"]
        mapped = stages["mapped_xyz"]
        out.append(
            CorpusVector((mapped[0], mapped[1], mapped[2]), stages["emitter_light"])
        )
    return out


class TestOneWhiteIsClosedForm(unittest.TestCase):
    def setUp(self: "TestOneWhiteIsClosedForm") -> None:
        self.inverse = _invert_3x3(rgb_matrix(RGB_COLUMNS))

    def test_the_corpus_has_rgbw_vectors(self: "TestOneWhiteIsClosedForm") -> None:
        # Without this the agreement tests below could pass on an empty set.
        self.assertGreaterEqual(len(corpus_vectors("rgbw")), 40)
        self.assertGreaterEqual(len(corpus_vectors("non_d65_white")), 40)

    def test_it_reproduces_the_reference_on_rgbw(
        self: "TestOneWhiteIsClosedForm",
    ) -> None:
        """The claim the C++ allocation will rest on.

        The reference reaches these drives by enumerating vertices. If the
        closed form agrees to float64 rounding across the corpus, the
        per-pixel path does not need the enumeration.
        """

        worst = 0.0
        for vector in corpus_vectors("rgbw"):
            drives = allocate_one_white(self.inverse, vector.target, D65_WHITE_COLUMN)
            self.assertIsNotNone(drives, msg=f"no allocation for {vector.target}")
            assert drives is not None
            got = (drives.red, drives.green, drives.blue, drives.white)
            for actual, want in zip(got, vector.emitter_light):
                worst = max(worst, abs(actual - want))
        self.assertLess(worst, 1e-12)

    def test_it_reproduces_the_reference_with_an_off_axis_white(
        self: "TestOneWhiteIsClosedForm",
    ) -> None:
        # `non_d65_white` puts the white emitter at D50 and renders to D50.
        # Nothing in the derivation assumed the white sat on the neutral
        # axis, and this is what says so.
        worst = 0.0
        for vector in corpus_vectors("non_d65_white"):
            drives = allocate_one_white(self.inverse, vector.target, D50_WHITE_COLUMN)
            self.assertIsNotNone(drives)
            assert drives is not None
            got = (drives.red, drives.green, drives.blue, drives.white)
            for actual, want in zip(got, vector.emitter_light):
                worst = max(worst, abs(actual - want))
        self.assertLess(worst, 1e-12)

    def test_the_white_level_is_maximal(self: "TestOneWhiteIsClosedForm") -> None:
        # White-*preferred* is the policy, so pushing the white any higher
        # has to break the RGB drives. Checked directly rather than trusted.
        per_white = _matvec(self.inverse, D65_WHITE_COLUMN)
        at_least_one_capped = 0
        for vector in corpus_vectors("rgbw"):
            drives = allocate_one_white(self.inverse, vector.target, D65_WHITE_COLUMN)
            assert drives is not None
            if drives.white >= 1.0 - 1e-12:
                at_least_one_capped += 1
                continue
            nudged = drives.white + 1e-6
            at_zero = _matvec(self.inverse, vector.target)
            escaped = False
            for index in range(3):
                drive = at_zero[index] - nudged * per_white[index]
                if drive < -1e-9 or drive > 1.0 + 1e-9:
                    escaped = True
            self.assertTrue(
                escaped, msg=f"white could have gone higher for {vector.target}"
            )
        # And the sweep must not have been all saturated emitters.
        self.assertLess(at_least_one_capped, len(corpus_vectors("rgbw")))


class TestTwoWhitesNeedMoreThanOne(unittest.TestCase):
    def setUp(self: "TestTwoWhitesNeedMoreThanOne") -> None:
        self.inverse = _invert_3x3(rgb_matrix(RGB_COLUMNS))

    def test_the_corpus_never_lights_both_whites(
        self: "TestTwoWhitesNeedMoreThanOne",
    ) -> None:
        """Why the corpus cannot settle the two-white question.

        Every `rgbww` vector the reference solves uses at most one of the two
        white emitters. Agreement with a one-white-at-a-time reduction over
        this corpus therefore says nothing at all, which is the trap the test
        below exists to keep open.
        """

        both = 0
        for vector in corpus_vectors("rgbww"):
            light = vector.emitter_light
            if light[3] > 1e-12 and light[4] > 1e-12:
                both += 1
        self.assertEqual(both, 0)

    def test_using_only_one_white_loses_badly(
        self: "TestTwoWhitesNeedMoreThanOne",
    ) -> None:
        """The reduction that looks right, measured being wrong.

        Running the one-white closed form for each white and keeping the
        better answer agrees with the reference on every corpus vector -- and
        is wrong on most reachable targets. The reason is not subtle once
        seen: a single emitter's drive is capped at 1, so any target needing
        more white than one emitter can supply must use both.
        """

        random.seed(11)
        columns = RGB_COLUMNS + [D65_WHITE_COLUMN, D50_WHITE_COLUMN]
        checked = 0
        mixing_wins = 0
        worst_gap = 0.0
        for _ in range(2000):
            drives: list[float] = []
            for _emitter in range(5):
                drives.append(random.random())
            target = tuple(
                sum(columns[e][i] * drives[e] for e in range(5)) for i in range(3)
            )
            single = best_single_white(
                self.inverse, target, D65_WHITE_COLUMN, D50_WHITE_COLUMN
            )
            pair = most_white_two(
                self.inverse, target, D65_WHITE_COLUMN, D50_WHITE_COLUMN
            )
            if single is None or pair is None:
                continue
            checked += 1
            gap = pair.total - single
            if gap > 1e-7:
                mixing_wins += 1
                worst_gap = max(worst_gap, gap)
        self.assertGreater(checked, 1000)
        # Not a rare corner: it is the common case away from the corpus.
        self.assertGreater(mixing_wins, checked // 2)
        # And the shortfall reaches a full emitter's worth of light.
        self.assertGreater(worst_gap, 0.9)


@dataclass(frozen=True, slots=True)
class SweepTally:
    """What one comparison sweep found."""

    solved: int
    disagreed: int
    boundary: int
    rejected_by_both: int
    worst_total_delta: float


class TestTwoWhitesAreAlsoClosedForm(unittest.TestCase):
    """The closed form must match the enumeration it replaces (#4198).

    Nothing is skipped here. The first attempt at this allocation reported
    "0 disagreements over 10,285 targets" while its harness had quietly
    dropped roughly 30,000 of 40,000 -- and the dropped ones were precisely
    the bright targets it could not handle. So every sample is classified by
    both methods and any disagreement about *feasibility* is counted as
    loudly as a disagreement about the answer.
    """

    def setUp(self: "TestTwoWhitesAreAlsoClosedForm") -> None:
        self.inverse = _invert_3x3(rgb_matrix(RGB_COLUMNS))
        self.columns = [*RGB_COLUMNS, D65_WHITE_COLUMN, D50_WHITE_COLUMN]

    def _target(self: "TestTwoWhitesAreAlsoClosedForm", drives: list[float]) -> Xyz:
        return tuple(
            sum(self.columns[e][i] * drives[e] for e in range(5)) for i in range(3)
        )

    def _worst_violation(
        self: "TestTwoWhitesAreAlsoClosedForm", target: Xyz, levels: "object"
    ) -> float:
        """How far outside [0, 1] the drives implied by `levels` reach."""

        at_zero = _matvec(self.inverse, target)
        from_first = _matvec(self.inverse, D65_WHITE_COLUMN)
        from_second = _matvec(self.inverse, D50_WHITE_COLUMN)
        first = levels.first  # type: ignore[attr-defined]
        second = levels.second  # type: ignore[attr-defined]
        worst = 0.0
        for value in (
            first,
            second,
            *(
                at_zero[i] - first * from_first[i] - second * from_second[i]
                for i in range(3)
            ),
        ):
            worst = max(worst, -value, value - 1.0)
        return worst

    def _sweep(
        self: "TestTwoWhitesAreAlsoClosedForm",
        make_drives: "object",
        count: int,
    ) -> SweepTally:
        """Compare both methods over `count` targets, returning the tallies.

        A "boundary" is a target the two classify differently *and* whose
        answer sits within the enumeration's own constraint tolerance of the
        hull. Those are not agreement, and they are not swept under the rug
        either: they are counted and asserted to stay rare, because the two
        methods draw the hull's edge with different arithmetic and a target
        landing exactly on it can fall either side. Anything further out is a
        real disagreement.
        """

        random.seed(4198)
        solved = disagreed = boundary = rejected_by_both = 0
        worst = 0.0
        for _ in range(count):
            target = self._target(make_drives())  # type: ignore[operator]
            closed = allocate_two_white(
                self.inverse, target, D65_WHITE_COLUMN, D50_WHITE_COLUMN
            )
            exact = most_white_two(
                self.inverse, target, D65_WHITE_COLUMN, D50_WHITE_COLUMN
            )
            if closed is None and exact is None:
                rejected_by_both += 1
                continue
            if (closed is None) != (exact is None):
                answered = closed if closed is not None else exact
                if self._worst_violation(target, answered) <= ENUMERATION_TOLERANCE:
                    boundary += 1
                else:
                    disagreed += 1
                continue
            assert closed is not None and exact is not None
            solved += 1
            worst = max(worst, abs(closed.total - exact.total))
        return SweepTally(solved, disagreed, boundary, rejected_by_both, worst)

    def test_it_matches_the_enumeration_on_reachable_targets(
        self: "TestTwoWhitesAreAlsoClosedForm",
    ) -> None:
        tally = self._sweep(lambda: [random.random() for _ in range(5)], 8000)
        self.assertEqual(tally.disagreed, 0)
        self.assertEqual(tally.boundary, 0)
        # Sampling drives rather than XYZ makes every target reachable by
        # construction, so a rejection from either side is a defect and this
        # count is the whole sample.
        self.assertEqual(tally.solved, 8000)
        self.assertLess(tally.worst_total_delta, 1e-9)

    def test_it_matches_on_targets_needing_both_whites(
        self: "TestTwoWhitesAreAlsoClosedForm",
    ) -> None:
        # The half the first attempt failed: totals whose feasible interval
        # does not start at zero.
        tally = self._sweep(
            lambda: [
                random.random() * 0.2,
                random.random() * 0.2,
                random.random() * 0.2,
                0.8 + random.random() * 0.2,
                0.8 + random.random() * 0.2,
            ],
            4000,
        )
        self.assertEqual(tally.disagreed, 0)
        self.assertEqual(tally.boundary, 0)
        self.assertEqual(tally.solved, 4000)
        self.assertLess(tally.worst_total_delta, 1e-9)

    def test_it_agrees_when_the_two_whites_are_the_same_colour(
        self: "TestTwoWhitesAreAlsoClosedForm",
    ) -> None:
        """The branch where the split moves no RGB drive at all.

        Two strings of the same white LED is an ordinary device, and it is
        the only shape that reaches the zero-difference branch: the split
        cannot move any drive, so those constraints fall on the *total*
        instead. An earlier revision encoded them as constant `w1` bounds --
        dimensionally wrong, a total is not a split -- and disagreed with the
        enumeration on 2206 of 3000 targets, by as much as 1.87 in total
        white. Every other device in this file has a non-zero difference in
        all three channels, so nothing else here goes near it.
        """

        random.seed(4198)
        columns = [*RGB_COLUMNS, D65_WHITE_COLUMN, D65_WHITE_COLUMN]
        solved = disagreed = 0
        worst = 0.0
        for _ in range(3000):
            drives = [random.random() for _ in range(5)]
            target = tuple(
                sum(columns[e][i] * drives[e] for e in range(5)) for i in range(3)
            )
            closed = allocate_two_white(
                self.inverse, target, D65_WHITE_COLUMN, D65_WHITE_COLUMN
            )
            exact = most_white_two(
                self.inverse, target, D65_WHITE_COLUMN, D65_WHITE_COLUMN
            )
            if (closed is None) != (exact is None):
                disagreed += 1
                continue
            if closed is None:
                continue
            assert exact is not None
            solved += 1
            worst = max(worst, abs(closed.total - exact.total))
        self.assertEqual(disagreed, 0)
        self.assertEqual(solved, 3000)
        self.assertLess(worst, 1e-9)

    def test_it_agrees_about_targets_outside_the_hull(
        self: "TestTwoWhitesAreAlsoClosedForm",
    ) -> None:
        # Half again as much light as every emitter at full drive can make.
        # Some of these land inside the hull and some do not; both methods
        # must say the same thing about each.
        tally = self._sweep(lambda: [random.random() * 1.35 for _ in range(5)], 4000)
        self.assertEqual(tally.disagreed, 0)
        self.assertGreater(tally.rejected_by_both, 0)
        self.assertGreater(tally.solved, 0)
        self.assertLess(tally.worst_total_delta, 1e-9)
        # Measured at 1 in 4000, and recorded rather than tolerated silently:
        # these are targets lying on the hull to within 1e-7, which the
        # enumeration's per-constraint slack admits and the closed form's
        # exact interval does not.
        self.assertLessEqual(tally.boundary, 4)

    def test_it_reproduces_the_reference_where_the_corpus_reaches(
        self: "TestTwoWhitesAreAlsoClosedForm",
    ) -> None:
        # The corpus is the P5 reference's own output, so this is the
        # strongest available check. Every one of these is reachable with one
        # white or none; `TestTwoWhiteCorpusVectors` covers the ones that are
        # not.
        vectors = corpus_vectors("rgbww")
        self.assertGreater(len(vectors), 0)
        for vector in vectors:
            closed = allocate_two_white(
                self.inverse, vector.target, D65_WHITE_COLUMN, D50_WHITE_COLUMN
            )
            self.assertIsNotNone(closed)
            assert closed is not None
            reference_total = vector.emitter_light[3] + vector.emitter_light[4]
            self.assertLess(abs(closed.total - reference_total), 1e-9)


class TestTwoWhiteCorpusVectors(unittest.TestCase):
    """The corpus now reaches targets that need both whites (#4198).

    Every `rgbww` vector is reachable with one white or none, so agreement
    over that device says nothing about the two-white case -- the same gap
    that let the "run the one-white form twice" reduction look correct while
    being wrong on 85% of random targets. `rgbww_two_white` scales its
    emitters so the strip can only just exceed its own rendering white, and
    bright neutrals then need both.
    """

    def setUp(self: "TestTwoWhiteCorpusVectors") -> None:
        columns = [
            emitter_column(0.6400, 0.3300, 0.22),
            emitter_column(0.3000, 0.6000, 0.60),
            emitter_column(0.1500, 0.0600, 0.08),
        ]
        self.inverse = _invert_3x3(rgb_matrix(columns))
        self.first = emitter_column(0.3127, 0.3290, 0.35)
        self.second = emitter_column(0.3457, 0.3585, 0.35)

    def test_the_corpus_lights_both_whites(
        self: "TestTwoWhiteCorpusVectors",
    ) -> None:
        vectors = corpus_vectors("rgbww_two_white")
        self.assertGreaterEqual(len(vectors), 50)
        both = [
            vector
            for vector in vectors
            if vector.emitter_light[3] > 1e-9 and vector.emitter_light[4] > 1e-9
        ]
        self.assertGreaterEqual(len(both), 12)

        # Both saturated: the corner of the feasible totals, and the case an
        # allocation that assumes those totals start at zero gets wrong.
        saturated = [
            vector
            for vector in both
            if vector.emitter_light[3] > 1.0 - 1e-9
            and vector.emitter_light[4] > 1.0 - 1e-9
        ]
        self.assertGreaterEqual(len(saturated), 6)

        # And both orderings, so an allocation that always fills the first
        # white first cannot pass by luck.
        self.assertTrue(
            any(
                vector.emitter_light[3] > 1.0 - 1e-9
                and vector.emitter_light[4] < 1.0 - 1e-9
                for vector in both
            )
        )
        self.assertTrue(
            any(
                vector.emitter_light[4] > 1.0 - 1e-9
                and vector.emitter_light[3] < 1.0 - 1e-9
                for vector in both
            )
        )

    def test_the_closed_form_reproduces_them(
        self: "TestTwoWhiteCorpusVectors",
    ) -> None:
        # Against the reference's recorded output this time, not against the
        # enumeration -- the check #4198 said could not be written until the
        # corpus reached these targets.
        for vector in corpus_vectors("rgbww_two_white"):
            closed = allocate_two_white(
                self.inverse, vector.target, self.first, self.second
            )
            self.assertIsNotNone(closed, msg=f"refused {vector.target}")
            assert closed is not None
            self.assertLess(abs(closed.first - vector.emitter_light[3]), 1e-9)
            self.assertLess(abs(closed.second - vector.emitter_light[4]), 1e-9)

    def test_one_white_alone_cannot_reach_the_bright_ones(
        self: "TestTwoWhiteCorpusVectors",
    ) -> None:
        # The premise, confirmed rather than asserted: where the reference
        # lights both whites past a total of one, the better single white
        # falls short.
        checked = 0
        for vector in corpus_vectors("rgbww_two_white"):
            total = vector.emitter_light[3] + vector.emitter_light[4]
            if total <= 1.0 + 1e-9:
                continue
            single = best_single_white(
                self.inverse, vector.target, self.first, self.second
            )
            self.assertTrue(
                single is None or single < total - 1e-6,
                msg=f"one white reached {single} of {total}",
            )
            checked += 1
        self.assertGreaterEqual(checked, 6)


if __name__ == "__main__":
    unittest.main()
