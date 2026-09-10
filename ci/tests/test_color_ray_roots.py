"""The feasible chroma ray is not always one interval (P7, #4041).

`docs/color-gamut-algorithm-selection.md` recorded connectivity as an
empirical claim resting on 1.9 million sampled feasibility evaluations, and
asked for an analytic bound it did not have. Computing the ray exactly instead
of sampling it refutes the claim: on the primaries FastLED ships there is a
hue wedge about 0.14 degrees wide near 264 degrees where the feasible chroma
is two intervals, at every lightness the sweep covers.

These tests pin the witness, pin the two distinct reasons a sampled sweep
walks past it, and pin the parts that would silently invalidate the method.
"""

from __future__ import annotations

import math
import unittest

from ci.color_gamut_study import emitter_matrix, is_feasible
from ci.color_ray_roots import (
    RayCubic,
    evaluate,
    feasible_runs,
    ray_cubics,
    real_roots,
    sweep_disconnected,
)
from ci.color_reference import Matrix3, _invert_3x3, _matvec, _xyz_from_oklab
from ci.color_wide_hull_study import scan_ray


PRIMARIES = ((0.6400, 0.3300), (0.3000, 0.6000), (0.1500, 0.0600))
MAX_CHROMA = 0.5
GUARD_CELLS = 64

# The witness. Found by computing the ray exactly on a 0.1-degree hue grid;
# the wedge runs from about 264.06 to 264.20 degrees and spans every
# lightness from 0.05 up.
WITNESS_LIGHTNESS = 0.5
WITNESS_HUE = 264.06


def _inverse() -> Matrix3:
    return _invert_3x3(emitter_matrix(PRIMARIES))


def _target(lightness: float, hue_degrees: float, chroma: float) -> tuple[float, ...]:
    radians = math.radians(hue_degrees)
    return _xyz_from_oklab(
        (lightness, chroma * math.cos(radians), chroma * math.sin(radians))
    )


class TestTheFactorisation(unittest.TestCase):
    def test_the_cubics_reproduce_the_reference_conversion(
        self: "TestTheFactorisation",
    ) -> None:
        # The whole method rests on the drive being this cubic and nothing
        # else. If the two ever drift, every result here is about a curve the
        # pipeline does not follow.
        inverse = _inverse()
        for lightness in (0.05, 0.4, 0.8, 1.1):
            for hue_degrees in (0.0, 37.0, WITNESS_HUE, 300.0):
                cubics = ray_cubics(inverse, lightness, hue_degrees)
                for chroma in (0.0, 0.05, 0.2, 0.35):
                    reference = _matvec(
                        inverse, _target(lightness, hue_degrees, chroma)
                    )
                    for index in range(3):
                        self.assertAlmostEqual(
                            evaluate(cubics[index], chroma),
                            reference[index],
                            delta=1e-12,
                        )

    def test_roots_land_on_the_polynomial(self: "TestTheFactorisation") -> None:
        # A root the solver reports but the polynomial does not have would cut
        # the partition in a place that is not a boundary, which shows up as a
        # spurious interval rather than as an error.
        inverse = _inverse()
        for cubic in ray_cubics(inverse, WITNESS_LIGHTNESS, WITNESS_HUE):
            for offset in (0.0, 1.0):
                for root in real_roots(cubic, offset):
                    self.assertAlmostEqual(evaluate(cubic, root), offset, delta=1e-10)

    def test_a_degenerate_leading_coefficient_still_solves(
        self: "TestTheFactorisation",
    ) -> None:
        # Cardano divides by the cubic term. A hue where it vanishes is not
        # exotic -- it is wherever the ray's LMS slopes cancel -- and dividing
        # by it there loses the roots that do exist.
        quadratic = RayCubic(0.0, 2.0, -3.0, 1.0)
        roots = sorted(real_roots(quadratic, 0.0))
        self.assertEqual(len(roots), 2)
        self.assertAlmostEqual(roots[0], 0.5, delta=1e-12)
        self.assertAlmostEqual(roots[1], 1.0, delta=1e-12)


class TestTheWitness(unittest.TestCase):
    def test_the_feasible_chroma_is_two_intervals(self: "TestTheWitness") -> None:
        scan = feasible_runs(
            _inverse(), WITNESS_LIGHTNESS, WITNESS_HUE, MAX_CHROMA, GUARD_CELLS
        )
        self.assertFalse(scan.is_connected)
        self.assertEqual(len(scan.intervals), 2)
        near, far = scan.intervals
        self.assertAlmostEqual(near.low, 0.0, delta=1e-12)
        self.assertAlmostEqual(near.high, 0.29443, delta=1e-4)
        self.assertAlmostEqual(far.low, 0.34575, delta=1e-4)
        self.assertAlmostEqual(far.high, 0.34644, delta=1e-4)

    def test_the_far_island_is_confirmed_by_the_studys_own_oracle(
        self: "TestTheWitness",
    ) -> None:
        # The intervals come from polynomial roots; `is_feasible` comes from
        # the matrix path the rest of the study uses. Agreeing here is what
        # separates a real gap from an artefact of the factorisation.
        inverse = _inverse()
        scan = feasible_runs(
            inverse, WITNESS_LIGHTNESS, WITNESS_HUE, MAX_CHROMA, GUARD_CELLS
        )
        near, far = scan.intervals
        inside_near = near.high * 0.5
        inside_gap = (near.high + far.low) / 2.0
        inside_far = (far.low + far.high) / 2.0
        for chroma, expected in (
            (inside_near, True),
            (inside_gap, False),
            (inside_far, True),
        ):
            with self.subTest(chroma=chroma):
                point = _target(WITNESS_LIGHTNESS, WITNESS_HUE, chroma)
                self.assertEqual(is_feasible(inverse, point), expected)

    def test_the_far_island_is_a_colour_the_device_can_make(
        self: "TestTheWitness",
    ) -> None:
        # Drives inside [0, 1] is the definition of feasible, but a target can
        # satisfy it while sitting outside the cone of physical colour. Both
        # halves are checked so the witness cannot be dismissed as a target
        # nothing would ever ask for.
        inverse = _inverse()
        scan = feasible_runs(
            inverse, WITNESS_LIGHTNESS, WITNESS_HUE, MAX_CHROMA, GUARD_CELLS
        )
        far = scan.intervals[1]
        point = _target(WITNESS_LIGHTNESS, WITNESS_HUE, (far.low + far.high) / 2.0)
        lms = _matvec(
            Matrix3(
                (0.8190224432164319, 0.3619062562801221, -0.12887378261216414),
                (0.0329836671980271, 0.9292868468965546, 0.03614466816999844),
                (0.048177199566046255, 0.26423952494422764, 0.6335478258136937),
            ),
            point,
        )
        for response in lms:
            self.assertGreater(response, 0.0)
        for drive in _matvec(inverse, point):
            self.assertGreaterEqual(drive, 0.0)
            self.assertLessEqual(drive, 1.0)

    def test_bisection_gives_up_a_sixth_of_the_reachable_chroma(
        self: "TestTheWitness",
    ) -> None:
        # The cost, stated as a number rather than as "the search may be
        # wrong". A bracket seeded at the neutral axis converges inside the
        # near interval and never reaches the far one.
        scan = feasible_runs(
            _inverse(), WITNESS_LIGHTNESS, WITNESS_HUE, MAX_CHROMA, GUARD_CELLS
        )
        near, far = scan.intervals
        self.assertGreater(far.high - near.high, 0.05)
        self.assertGreater((far.high - near.high) / far.high, 0.15)


class TestWhyTheSweepsMissedIt(unittest.TestCase):
    def test_the_reports_hue_grid_steps_over_the_wedge(
        self: "TestWhyTheSweepsMissedIt",
    ) -> None:
        # The dense sweep behind the report walks hue every 3 degrees. The
        # wedge is about 0.14 degrees wide and contains no multiple of 3, so
        # no amount of extra chroma sampling on that grid could have found it.
        broken = sweep_disconnected(_inverse(), 20, 120, MAX_CHROMA, GUARD_CELLS)
        self.assertEqual(broken, [])

    def test_a_tenth_degree_grid_finds_it(self: "TestWhyTheSweepsMissedIt") -> None:
        # Same device, same chroma range, hue thirty times finer. Kept small
        # in lightness so this stays a unit test; the full sweep is in the
        # report.
        broken = sweep_disconnected(_inverse(), 8, 3600, MAX_CHROMA, GUARD_CELLS)
        self.assertGreater(len(broken), 0)
        for ray in broken:
            with self.subTest(hue=ray.hue_degrees):
                self.assertGreater(ray.gap, 0.0)
                self.assertAlmostEqual(ray.hue_degrees, 264.1, delta=0.2)

    def test_chroma_sampling_misses_it_even_on_the_right_ray(
        self: "TestWhyTheSweepsMissedIt",
    ) -> None:
        # The second failure mode, and the one that matters more: even handed
        # the exact hue, the sampled scan reports a single interval, because
        # the far island is narrower than its 0.00125 chroma step. Density is
        # not the fix -- whatever the step, some ray's island is thinner.
        inverse = _inverse()
        sampled = scan_ray(
            lambda point: is_feasible(inverse, point),
            WITNESS_LIGHTNESS,
            WITNESS_HUE,
            MAX_CHROMA,
            401,
        )
        self.assertTrue(sampled.is_connected)
        exact = feasible_runs(
            inverse, WITNESS_LIGHTNESS, WITNESS_HUE, MAX_CHROMA, GUARD_CELLS
        )
        self.assertFalse(exact.is_connected)
        far = exact.intervals[1]
        self.assertLess(far.high - far.low, MAX_CHROMA / 400.0)


class TestTheOrdinaryRayIsStillConnected(unittest.TestCase):
    def test_hues_away_from_the_wedge_are_one_interval_anchored_at_zero(
        self: "TestTheOrdinaryRayIsStillConnected",
    ) -> None:
        # A refutation is only worth something if the thing it refutes was
        # nearly true. Away from the wedge the report's picture holds exactly,
        # which is why this is a narrow correction and not a rewrite.
        inverse = _inverse()
        for lightness_step in range(1, 10):
            lightness = lightness_step / 10.0
            for hue_step in range(0, 360, 15):
                with self.subTest(lightness=lightness, hue=hue_step):
                    scan = feasible_runs(
                        inverse, lightness, float(hue_step), MAX_CHROMA, GUARD_CELLS
                    )
                    self.assertTrue(scan.is_connected)
                    self.assertTrue(scan.starts_at_zero)


class TestInputValidation(unittest.TestCase):
    def test_degenerate_sweep_arguments_are_refused(
        self: "TestInputValidation",
    ) -> None:
        # Each of these returns an empty result rather than failing, and an
        # empty result here reads as "no disconnected ray exists" -- the
        # claim this module was written to overturn.
        inverse = _inverse()
        with self.assertRaises(ValueError):
            feasible_runs(inverse, 0.5, 0.0, 0.0, GUARD_CELLS)
        with self.assertRaises(ValueError):
            feasible_runs(inverse, 0.5, 0.0, MAX_CHROMA, 0)
        with self.assertRaises(ValueError):
            sweep_disconnected(inverse, 0, 120, MAX_CHROMA, GUARD_CELLS)
        with self.assertRaises(ValueError):
            sweep_disconnected(inverse, 20, 0, MAX_CHROMA, GUARD_CELLS)


if __name__ == "__main__":
    unittest.main()
