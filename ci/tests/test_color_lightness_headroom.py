"""The structure behind #4245, and the mapper shape it implies.

`docs/color-gamut-algorithm-selection.md` argues -- correctly -- that a chroma
bisection bracketed at zero cannot work above the brightest reachable neutral.
The conclusion drawn from that, that lightness must therefore be clamped
first, is what these tests show to be too strong: the feasible chroma above
the cap is still a single interval, it simply does not contain zero.
"""

from __future__ import annotations

import unittest

from ci.color_gamut_study import emitter_matrix, is_feasible
from ci.color_lightness_headroom_study import (
    brightest_reachable,
    feasible_intervals,
    hue_of,
    map_clamp_then_chroma,
    map_interval_clamp,
    neutral_cap,
    target_xyz,
)
from ci.color_reference import _invert_3x3, _oklch_from_xyz


PRIMARIES = ((0.6400, 0.3300), (0.3000, 0.6000), (0.1500, 0.0600))
PROBES = 8
HALVINGS = 12


def _device() -> tuple:
    forward = emitter_matrix(PRIMARIES)
    return forward, _invert_3x3(forward)


class TestTheFeasibleSetIsNotWhatTheClampAssumes(unittest.TestCase):
    def setUp(self: "TestTheFeasibleSetIsNotWhatTheClampAssumes") -> None:
        _forward, self.inverse = _device()
        self.cap = neutral_cap(self.inverse)

    def test_the_cap_is_well_below_what_the_device_can_reach(
        self: "TestTheFeasibleSetIsNotWhatTheClampAssumes",
    ) -> None:
        # Without headroom there is nothing to recover and every assertion
        # below would be about an empty region.
        best = brightest_reachable(self.inverse, 15, 25, 60)
        self.assertGreater(best.lightness, self.cap * 1.2)
        # And it is genuinely off the neutral axis, which is why the cap
        # cannot reach it.
        self.assertGreater(best.chroma, 0.0)

    def test_below_the_cap_the_interval_starts_at_zero(
        self: "TestTheFeasibleSetIsNotWhatTheClampAssumes",
    ) -> None:
        # Which is exactly when a bisection bracketed at zero is valid.
        for hue in (30.0, 120.0, 300.0):
            with self.subTest(hue=hue):
                intervals = feasible_intervals(self.inverse, self.cap * 0.95, hue, 400)
                self.assertEqual(len(intervals), 1)
                self.assertTrue(intervals[0].anchored_at_zero)

    def test_above_the_cap_it_is_one_interval_that_excludes_zero(
        self: "TestTheFeasibleSetIsNotWhatTheClampAssumes",
    ) -> None:
        """The finding. Connected, and simply not anchored at zero.

        Still one interval, so a search is possible; not containing zero, so
        the shipped bracket's invariant is inverted from the first step. Both
        halves matter, and asserting only one of them would mis-state why the
        clamp exists.
        """

        found = 0
        for hue in (30.0, 300.0):
            intervals = feasible_intervals(self.inverse, self.cap * 1.05, hue, 400)
            for interval in intervals:
                with self.subTest(hue=hue):
                    self.assertFalse(interval.anchored_at_zero)
                    self.assertGreater(interval.low, 0.0)
                    self.assertGreater(interval.high, interval.low)
                found += 1
            self.assertLessEqual(len(intervals), 1)
        self.assertGreater(found, 0)


class TestTheIntervalClampRecoversLightness(unittest.TestCase):
    def setUp(self: "TestTheIntervalClampRecoversLightness") -> None:
        _forward, self.inverse = _device()
        self.cap = neutral_cap(self.inverse)

    def _cases(self: "TestTheIntervalClampRecoversLightness") -> list[tuple]:
        cases: list[tuple] = []
        for scale in (1.05, 1.15, 1.25):
            lightness = self.cap * scale
            for hue in (30.0, 120.0, 300.0):
                for chroma in (0.10, 0.30, 0.50):
                    if is_feasible(self.inverse, target_xyz(lightness, hue, chroma)):
                        continue
                    cases.append((lightness, hue, chroma))
        return cases

    def test_there_are_over_bright_cases_to_score(
        self: "TestTheIntervalClampRecoversLightness",
    ) -> None:
        self.assertGreater(len(self._cases()), 10)

    def test_it_is_never_worse_than_the_shipped_path(
        self: "TestTheIntervalClampRecoversLightness",
    ) -> None:
        # The property that makes it a safe replacement rather than a trade.
        for lightness, hue, chroma in self._cases():
            with self.subTest(L=round(lightness, 3), hue=hue, C=chroma):
                shipped_l, _shipped_c = map_clamp_then_chroma(
                    self.inverse, lightness, hue, chroma, HALVINGS
                )
                kept_l, _kept_c = map_interval_clamp(
                    self.inverse, lightness, hue, chroma, PROBES, HALVINGS
                )
                self.assertGreaterEqual(kept_l, shipped_l - 1e-9)

    def test_it_recovers_lightness_on_some_targets(
        self: "TestTheIntervalClampRecoversLightness",
    ) -> None:
        # Not vacuous: "never worse" is satisfied by doing nothing.
        best = 0.0
        for lightness, hue, chroma in self._cases():
            shipped_l, _c = map_clamp_then_chroma(
                self.inverse, lightness, hue, chroma, HALVINGS
            )
            kept_l, _k = map_interval_clamp(
                self.inverse, lightness, hue, chroma, PROBES, HALVINGS
            )
            best = max(best, kept_l - shipped_l)
        self.assertGreater(best, 0.2)

    def test_every_result_is_feasible(
        self: "TestTheIntervalClampRecoversLightness",
    ) -> None:
        for lightness, hue, chroma in self._cases():
            with self.subTest(L=round(lightness, 3), hue=hue, C=chroma):
                kept_l, kept_c = map_interval_clamp(
                    self.inverse, lightness, hue, chroma, PROBES, HALVINGS
                )
                self.assertTrue(
                    is_feasible(self.inverse, target_xyz(kept_l, hue, kept_c))
                )

    def test_hue_is_preserved_exactly(
        self: "TestTheIntervalClampRecoversLightness",
    ) -> None:
        checked = 0
        for lightness, hue, chroma in self._cases():
            kept_l, kept_c = map_interval_clamp(
                self.inverse, lightness, hue, chroma, PROBES, HALVINGS
            )
            if kept_c <= 1e-6:
                continue
            with self.subTest(L=round(lightness, 3), hue=hue, C=chroma):
                self.assertAlmostEqual(
                    hue_of(target_xyz(kept_l, hue, kept_c)), hue, places=3
                )
            checked += 1
        self.assertGreater(checked, 0)

    def test_in_gamut_targets_pass_through_untouched(
        self: "TestTheIntervalClampRecoversLightness",
    ) -> None:
        lightness, hue, chroma = self.cap * 0.5, 30.0, 0.05
        self.assertTrue(is_feasible(self.inverse, target_xyz(lightness, hue, chroma)))
        kept = map_interval_clamp(
            self.inverse, lightness, hue, chroma, PROBES, HALVINGS
        )
        self.assertEqual(kept, (lightness, chroma))


class TestTheRefutedShapes(unittest.TestCase):
    """Both alternatives measured failing, kept so they are not re-attempted."""

    def setUp(self: "TestTheRefutedShapes") -> None:
        _forward, self.inverse = _device()
        self.cap = neutral_cap(self.inverse)

    def test_a_bracket_at_zero_converges_on_an_infeasible_answer(
        self: "TestTheRefutedShapes",
    ) -> None:
        # Compressing chroma at the target's own lightness, above the cap.
        # The low end is infeasible from the first step, so the bisection's
        # invariant is inverted and it walks down to zero -- which is also
        # infeasible there. This is why the clamp exists.
        lightness = self.cap * 1.25
        hue = 300.0
        low, high = 0.0, 0.50
        for _ in range(HALVINGS):
            middle = (low + high) / 2.0
            if is_feasible(self.inverse, target_xyz(lightness, hue, middle)):
                low = middle
            else:
                high = middle
        self.assertLess(low, 1e-3)
        self.assertFalse(is_feasible(self.inverse, target_xyz(lightness, hue, low)))

    def test_a_line_search_from_the_cap_collapses_chroma(
        self: "TestTheRefutedShapes",
    ) -> None:
        # The anchor sits *on* the hull boundary, so the segment towards an
        # exterior target leaves immediately and the largest feasible step is
        # tiny. Worse than the shipped path, which at least keeps chroma.
        lightness, hue, chroma = self.cap * 1.15, 30.0, 0.50
        low, high = 0.0, 1.0
        for _ in range(HALVINGS):
            step = (low + high) / 2.0
            candidate = self.cap + (lightness - self.cap) * step
            if is_feasible(self.inverse, target_xyz(candidate, hue, chroma * step)):
                low = step
            else:
                high = step
        _shipped_l, shipped_c = map_clamp_then_chroma(
            self.inverse, lightness, hue, chroma, HALVINGS
        )
        self.assertLess(chroma * low, shipped_c)


if __name__ == "__main__":
    unittest.main()
