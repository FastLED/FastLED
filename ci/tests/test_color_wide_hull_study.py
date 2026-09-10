"""The wide-hull claims in the P7 report must keep reproducing.

`docs/color-gamut-algorithm-selection.md` records two results for devices with
a white emitter: the feasible chroma ray is a single interval reaching the
neutral axis, and map-then-allocate composes without loss. Both are
measurements, so both can rot -- and the first is what makes the shipped
eight-halving bisection a valid search rather than a lucky one.
"""

from __future__ import annotations

import math
import unittest

from ci.color_gamut_study import D65_WHITE
from ci.color_reference import (
    _oklch_from_xyz,
    _xyz_from_oklab,
    delta_e2000,
    xyz_to_lab,
)
from ci.color_reference_corpus import _device_profiles
from ci.color_wide_hull_study import (
    ChromaInterval,
    attainable_lightness,
    corpus_devices,
    disconnected,
    hue_drift_degrees,
    map_oklch,
    scan_ray,
    scan_rays,
)


HALVINGS = 8
MAX_CHROMA = 0.5


def _target(lightness: float, hue_degrees: float, chroma: float) -> tuple:
    radians = math.radians(hue_degrees)
    return _xyz_from_oklab(
        (lightness, chroma * math.cos(radians), chroma * math.sin(radians))
    )


class TestTheDetectorCanFail(unittest.TestCase):
    """Without these the sweeps below would pass on a detector that never fires."""

    def test_a_hole_in_the_ray_is_reported_as_two_intervals(
        self: "TestTheDetectorCanFail",
    ) -> None:
        def holed(target: tuple) -> bool:
            chroma = _oklch_from_xyz(target).chroma
            return chroma <= 0.10 or 0.25 <= chroma <= 0.35

        scan = scan_ray(holed, 0.6, 0.0, MAX_CHROMA, 401)
        self.assertEqual(len(scan.intervals), 2)
        self.assertFalse(scan.is_connected)
        self.assertEqual(len(disconnected([scan])), 1)

    def test_an_interval_off_the_neutral_axis_is_reported_as_such(
        self: "TestTheDetectorCanFail",
    ) -> None:
        # Connected but not anchored at zero defeats the shipped search just
        # as thoroughly, because it seeds its bracket at chroma zero.
        def floating(target: tuple) -> bool:
            return 0.2 <= _oklch_from_xyz(target).chroma <= 0.3

        scan = scan_ray(floating, 0.6, 0.0, MAX_CHROMA, 401)
        self.assertTrue(scan.is_connected)
        self.assertFalse(scan.starts_at_zero)

    def test_a_ray_with_nothing_feasible_is_not_called_disconnected(
        self: "TestTheDetectorCanFail",
    ) -> None:
        scan = scan_ray(lambda target: False, 0.6, 0.0, MAX_CHROMA, 401)
        self.assertEqual(scan.intervals, ())
        self.assertTrue(scan.is_connected)
        self.assertFalse(scan.starts_at_zero)

    def test_interval_bounds_are_the_sampled_extremes(
        self: "TestTheDetectorCanFail",
    ) -> None:
        scan = scan_ray(
            lambda target: _oklch_from_xyz(target).chroma <= 0.2, 0.6, 0.0, 0.4, 400
        )
        self.assertEqual(len(scan.intervals), 1)
        self.assertEqual(scan.intervals[0], ChromaInterval(0.0, scan.intervals[0].high))
        self.assertAlmostEqual(scan.intervals[0].high, 0.2, places=2)


class TestWideHullConnectivity(unittest.TestCase):
    def setUp(self: "TestWideHullConnectivity") -> None:
        self.devices = corpus_devices()

    def test_the_devices_track_the_corpus(
        self: "TestWideHullConnectivity",
    ) -> None:
        # If the corpus gains a white-emitter device this study has to grow
        # with it, or the sweep silently stops covering the shipped set.
        studied = {device.name for device in self.devices}
        corpus_names = set(_device_profiles())
        white_emitter_devices = corpus_names - {"rgb"}
        self.assertEqual(studied, white_emitter_devices)

    def test_feasible_chroma_is_one_interval_on_every_wide_device(
        self: "TestWideHullConnectivity",
    ) -> None:
        """The >=4-emitter case the three-emitter sweep could not speak for.

        A white emitter adds a redundant generator to the zonotope, so
        connectivity does not carry over from the RGB result. Coarse here;
        the dense sweep behind the report's number is recorded there.
        """

        for device in self.devices:
            with self.subTest(device=device.name):
                scans = scan_rays(device.hull, 10, 30, MAX_CHROMA, 61)
                self.assertEqual(disconnected(scans), [])

    def test_every_feasible_ray_reaches_the_neutral_axis(
        self: "TestWideHullConnectivity",
    ) -> None:
        for device in self.devices:
            with self.subTest(device=device.name):
                scans = scan_rays(device.hull, 10, 30, MAX_CHROMA, 61)
                anchored = 0
                for scan in scans:
                    if scan.intervals:
                        self.assertTrue(scan.starts_at_zero)
                        anchored += 1
                # Not vacuous: some ray had to be feasible somewhere.
                self.assertGreater(anchored, 0)


class TestMapThenAllocate(unittest.TestCase):
    def setUp(self: "TestMapThenAllocate") -> None:
        self.devices = corpus_devices()

    def _out_of_gamut(self: "TestMapThenAllocate", device: object) -> list[tuple]:
        targets: list[tuple] = []
        for lightness_step in range(1, 10):
            for hue_degrees in range(0, 360, 45):
                for chroma_step in range(1, 8):
                    point = _target(
                        lightness_step / 10.0, float(hue_degrees), chroma_step * 0.08
                    )
                    if not device.solve(point):
                        targets.append(point)
        return targets

    def test_the_sweep_actually_finds_out_of_gamut_targets(
        self: "TestMapThenAllocate",
    ) -> None:
        # Every assertion below is quantified over this set, so an empty one
        # would make all of them pass without testing anything.
        for device in self.devices:
            with self.subTest(device=device.name):
                self.assertGreater(len(self._out_of_gamut(device)), 100)

    def test_mapping_brings_every_out_of_gamut_target_into_the_hull(
        self: "TestMapThenAllocate",
    ) -> None:
        for device in self.devices:
            with self.subTest(device=device.name):
                for point in self._out_of_gamut(device):
                    self.assertTrue(
                        device.solve(map_oklch(device.solve, point, HALVINGS))
                    )

    def test_allocation_reproduces_what_the_mapper_chose(
        self: "TestMapThenAllocate",
    ) -> None:
        """The composition the report listed under "Not covered".

        Re-rendering the drives rather than trusting them is the point: it is
        what would catch an allocation reporting success while producing
        something else.
        """

        for device in self.devices:
            with self.subTest(device=device.name):
                worst = 0.0
                for point in self._out_of_gamut(device):
                    mapped = map_oklch(device.solve, point, HALVINGS)
                    allocated = device.realize(mapped)
                    self.assertIsNotNone(allocated)
                    realized, _drives = allocated
                    difference = delta_e2000(
                        xyz_to_lab(realized, D65_WHITE), xyz_to_lab(mapped, D65_WHITE)
                    )
                    worst = max(worst, difference)
                self.assertLess(worst, 1e-9)

    def test_allocation_does_not_shift_hue(
        self: "TestMapThenAllocate",
    ) -> None:
        # The mapper preserves hue by construction; whether the allocation
        # behind it does is a separate question, and the one that was unscored.
        for device in self.devices:
            with self.subTest(device=device.name):
                checked = 0
                for point in self._out_of_gamut(device):
                    mapped = map_oklch(device.solve, point, HALVINGS)
                    if _oklch_from_xyz(mapped).chroma <= 0.02:
                        continue
                    realized, _drives = device.realize(mapped)
                    self.assertLess(hue_drift_degrees(mapped, realized), 1e-6)
                    checked += 1
                self.assertGreater(checked, 0)

    def test_drives_stay_inside_the_box(
        self: "TestMapThenAllocate",
    ) -> None:
        for device in self.devices:
            with self.subTest(device=device.name):
                for point in self._out_of_gamut(device):
                    mapped = map_oklch(device.solve, point, HALVINGS)
                    _realized, drives = device.realize(mapped)
                    for drive in drives:
                        self.assertGreaterEqual(drive, -1e-9)
                        self.assertLessEqual(drive, 1.0 + 1e-9)

    def test_over_bright_targets_need_the_lightness_clamp(
        self: "TestMapThenAllocate",
    ) -> None:
        """Chroma compression alone cannot rescue a target that is too bright.

        At zero chroma it is still outside the hull, so a chroma-only search
        converges on an infeasible answer and the fallback hands back four
        zero drives for a colour that is not black.

        None of the sweeps above reach this: every wide device in the corpus
        caps its neutral above L = 1.15 and the grids stop at 0.9. The report
        records the same gap for the three-emitter corpus -- an over-bright
        target has to be constructed, and until one is, a mapper that dropped
        the lightness clamp entirely passes everything else here.
        """

        for device in self.devices:
            with self.subTest(device=device.name):
                cap = attainable_lightness(device.solve, 3.0, 60)
                self.assertGreater(cap, 1.0)
                checked = 0
                for scale in (1.05, 1.3, 2.0):
                    for hue_degrees in (0.0, 120.0, 240.0):
                        for chroma in (0.0, 0.05, 0.2):
                            point = _target(cap * scale, hue_degrees, chroma)
                            # Above the *neutral* cap is not the same as
                            # outside the hull: the hull reaches higher in
                            # lightness off the neutral axis, so a chromatic
                            # target just over the cap can still be exactly
                            # reachable. That gap is #4245; here it only means
                            # such a point is not an over-bright case and has
                            # nothing to say about the clamp.
                            if device.solve(point):
                                continue
                            mapped = map_oklch(device.solve, point, HALVINGS)
                            self.assertTrue(device.solve(mapped))
                            # Not black: the failure this guards returns zero
                            # drives, which would also satisfy "in the hull".
                            self.assertGreater(
                                _oklch_from_xyz(mapped).lightness, cap * 0.5
                            )
                            allocated = device.realize(mapped)
                            self.assertIsNotNone(allocated)
                            checked += 1
                # Most of the grid has to survive the screen above, or this
                # stops covering the case it exists for.
                self.assertGreaterEqual(checked, 20)

    def test_in_gamut_targets_pass_through_untouched(
        self: "TestMapThenAllocate",
    ) -> None:
        # Exact preservation in gamut is an acceptance criterion of #4041, and
        # it has to hold on the wide path too.
        for device in self.devices:
            with self.subTest(device=device.name):
                point = _target(0.5, 30.0, 0.02)
                self.assertTrue(device.solve(point))
                mapped = map_oklch(device.solve, point, HALVINGS)
                for got, want in zip(mapped, point):
                    self.assertAlmostEqual(got, want, places=12)


if __name__ == "__main__":
    unittest.main()
