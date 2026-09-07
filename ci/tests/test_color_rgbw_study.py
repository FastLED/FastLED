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
    allocate_one_white,
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


if __name__ == "__main__":
    unittest.main()
