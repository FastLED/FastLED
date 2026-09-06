"""Anchor tests for the P5 float64 color-reference primitives.

Normative references:
* IEC 61966-2-1:1999, sRGB encoding transfer.
* ICC.1:2022, Annex E, linear Bradford chromatic adaptation.
* ISO/CIE 11664-6:2022, CIEDE2000.
"""

from __future__ import annotations

import unittest

from ci.color_reference import (
    Chromaticity,
    DeviceProfile,
    Emitter,
    RgbPrimaries,
    SourceProfile,
    TransferFunction,
    apply_brightness_then_inverse_response,
    bradford_adaptation,
    decode_rgb8,
    delta_e2000,
    emitted_xyz,
    map_and_solve,
    source_rgb_to_xyz,
    xyz_to_lab,
)


class TestColorReference(unittest.TestCase):
    @staticmethod
    def _rgb_profile(with_white: bool = False) -> DeviceProfile:
        emitters = (
            Emitter("red", Chromaticity(0.6400, 0.3300), 1.0, False),
            Emitter("green", Chromaticity(0.3000, 0.6000), 1.0, False),
            Emitter("blue", Chromaticity(0.1500, 0.0600), 1.0, False),
        )
        if with_white:
            emitters += (Emitter("white", Chromaticity(0.3127, 0.3290), 1.0, True),)
        return DeviceProfile(
            emitters=emitters, rendering_white=Chromaticity(0.3127, 0.3290)
        )

    def test_feasible_primary_is_identity_and_reproduces_exactly(self) -> None:
        profile = self._rgb_profile()
        target = (0.6400 / 0.3300 * 0.5, 0.5, (1.0 - 0.6400 - 0.3300) / 0.3300 * 0.5)

        solution = map_and_solve(target, profile)

        self.assertEqual(solution.mapped_xyz, target)
        self.assertEqual(solution.drives, (0.5, 0.0, 0.0))
        self.assertEqual(emitted_xyz(profile, solution.drives), target)

    def test_rgbw_neutral_axis_uses_white_preferred_deterministic_allocation(
        self,
    ) -> None:
        profile = self._rgb_profile(with_white=True)
        target = (0.3127 / 0.3290 * 0.25, 0.25, (1.0 - 0.3127 - 0.3290) / 0.3290 * 0.25)

        solution = map_and_solve(target, profile)

        self.assertEqual(solution.drives, (0.0, 0.0, 0.0, 0.25))
        self.assertEqual(solution.mapped_xyz, target)

    def test_rgbww_neutral_axis_has_deterministic_white_tie_break(self) -> None:
        profile = DeviceProfile(
            emitters=(
                Emitter("red", Chromaticity(0.6400, 0.3300), 1.0, False),
                Emitter("green", Chromaticity(0.3000, 0.6000), 1.0, False),
                Emitter("blue", Chromaticity(0.1500, 0.0600), 1.0, False),
                Emitter("cool", Chromaticity(0.3127, 0.3290), 1.0, True),
                Emitter("warm", Chromaticity(0.3127, 0.3290), 1.0, True),
            ),
            rendering_white=Chromaticity(0.3127, 0.3290),
        )
        target = (
            0.3127 / 0.3290 * 0.25,
            0.25,
            (1.0 - 0.3127 - 0.3290) / 0.3290 * 0.25,
        )

        solution = map_and_solve(target, profile)

        self.assertEqual(solution.drives, (0.0, 0.0, 0.0, 0.0, 0.25))
        self.assertEqual(solution.mapped_xyz, target)

    def test_rendering_white_override_preserves_feasible_d50_neutral(self) -> None:
        d50 = Chromaticity(0.3457, 0.3585)
        profile = DeviceProfile(
            emitters=(
                Emitter("red", Chromaticity(0.6400, 0.3300), 1.0, False),
                Emitter("green", Chromaticity(0.3000, 0.6000), 1.0, False),
                Emitter("blue", Chromaticity(0.1500, 0.0600), 1.0, False),
                Emitter("white", d50, 1.0, True),
            ),
            rendering_white=d50,
        )
        target = (
            d50.x / d50.y * 0.25,
            0.25,
            (1.0 - d50.x - d50.y) / d50.y * 0.25,
        )

        solution = map_and_solve(target, profile)

        self.assertEqual(solution.mapped_xyz, target)
        self.assertEqual(solution.drives, (0.0, 0.0, 0.0, 0.25))

    def test_above_neutral_capacity_clamps_to_dim_bt709_white(self) -> None:
        # BT.709's relative luminance weights make the full-on device white
        # D65.  Equal-Y primaries would not: their full-on chromaticity is not
        # the rendering white, so they cannot validly expect (1, 1, 1) for a
        # D65 target.
        profile = DeviceProfile(
            emitters=(
                Emitter(
                    "red", Chromaticity(0.6400, 0.3300), 0.3 * 0.212639005871510, False
                ),
                Emitter(
                    "green",
                    Chromaticity(0.3000, 0.6000),
                    0.3 * 0.715168678767756,
                    False,
                ),
                Emitter(
                    "blue", Chromaticity(0.1500, 0.0600), 0.3 * 0.072192315360734, False
                ),
            ),
            rendering_white=Chromaticity(0.3127, 0.3290),
        )
        target = (
            0.3127 / 0.3290,
            1.0,
            (1.0 - 0.3127 - 0.3290) / 0.3290,
        )

        solution = map_and_solve(target, profile)

        for drive in solution.drives:
            self.assertAlmostEqual(drive, 1.0, delta=1e-12)
        expected = (
            0.3127 / 0.3290 * 0.3,
            0.3,
            (1.0 - 0.3127 - 0.3290) / 0.3290 * 0.3,
        )
        for actual, expected_component in zip(solution.mapped_xyz, expected):
            self.assertAlmostEqual(actual, expected_component, delta=1e-12)

    def test_equal_y_primaries_use_their_derived_neutral_limit(self) -> None:
        profile = DeviceProfile(
            emitters=(
                Emitter("red", Chromaticity(0.6400, 0.3300), 0.1, False),
                Emitter("green", Chromaticity(0.3000, 0.6000), 0.1, False),
                Emitter("blue", Chromaticity(0.1500, 0.0600), 0.1, False),
            ),
            rendering_white=Chromaticity(0.3127, 0.3290),
        )
        target = (
            0.3127 / 0.3290,
            1.0,
            (1.0 - 0.3127 - 0.3290) / 0.3290,
        )

        solution = map_and_solve(target, profile)

        # For equal-Y primaries, scale the D65 BT.709 source weights until
        # green (the largest weight) reaches unit drive.  This is calculated
        # from the source matrix, independently of the mapper's allocation.
        red_weight = 0.212639005871510
        green_weight = 0.715168678767756
        blue_weight = 0.072192315360734
        neutral_scale = 0.1 / green_weight
        expected_drives = (
            red_weight / green_weight,
            1.0,
            blue_weight / green_weight,
        )
        expected_xyz = tuple(component * neutral_scale for component in target)
        for actual, expected_drive in zip(solution.drives, expected_drives):
            self.assertAlmostEqual(actual, expected_drive, delta=1e-12)
        for actual, expected_component in zip(solution.mapped_xyz, expected_xyz):
            self.assertAlmostEqual(actual, expected_component, delta=1e-12)

    def test_out_of_gamut_target_maps_to_a_feasible_hue_preserving_solution(
        self,
    ) -> None:
        profile = self._rgb_profile()
        target = (0.15 / 0.80 * 0.5, 0.5, (1.0 - 0.15 - 0.80) / 0.80 * 0.5)

        solution = map_and_solve(target, profile)

        self.assertNotEqual(solution.mapped_xyz, target)
        self.assertTrue(all(0.0 <= drive <= 1.0 for drive in solution.drives))
        self.assertEqual(emitted_xyz(profile, solution.drives), solution.mapped_xyz)
        self.assertAlmostEqual(
            solution.target_oklch.hue_degrees,
            solution.mapped_oklch.hue_degrees,
            places=9,
        )

    def test_brightness_precedes_nonlinear_inverse_response(self) -> None:
        drives = apply_brightness_then_inverse_response(
            solved_light=(1.0, 1.0),
            brightness=0.25,
            response_exponents=(1.0, 2.0),
        )

        self.assertEqual(drives, (0.25, 0.5))

    def test_srgb_decode_includes_piecewise_boundary_and_endpoints(self) -> None:
        profile = SourceProfile.srgb_bt709()

        self.assertEqual(
            decode_rgb8((0, 255, 10), profile), (0.0, 1.0, 10 / 255 / 12.92)
        )
        self.assertAlmostEqual(
            decode_rgb8((11, 0, 0), profile)[0],
            ((11 / 255 + 0.055) / 1.055) ** 2.4,
            places=15,
        )

    def test_bt709_decode_uses_its_own_piecewise_transfer(self) -> None:
        profile = SourceProfile(
            RgbPrimaries.bt709(),
            TransferFunction.BT709,
        )

        self.assertEqual(decode_rgb8((20, 0, 0), profile)[0], 20 / 255 / 4.5)
        self.assertAlmostEqual(
            decode_rgb8((21, 0, 0), profile)[0],
            ((21 / 255 + 0.099) / 1.099) ** (1 / 0.45),
            places=15,
        )

    def test_linear_bt709_source_rgb_converts_to_d65_xyz(self) -> None:
        profile = SourceProfile.linear_srgb()

        red = source_rgb_to_xyz((1.0, 0.0, 0.0), profile)
        white = source_rgb_to_xyz((1.0, 1.0, 1.0), profile)

        self.assertAlmostEqual(red[0], 0.4123907993, places=9)
        self.assertAlmostEqual(red[1], 0.2126390059, places=9)
        self.assertAlmostEqual(red[2], 0.0193308187, places=9)
        self.assertAlmostEqual(white[0], 0.3127 / 0.3290, places=9)
        self.assertAlmostEqual(white[1], 1.0, places=7)
        self.assertAlmostEqual(white[2], (1.0 - 0.3127 - 0.3290) / 0.3290, places=9)

    def test_named_boundary_primaries_and_decoded_srgb_keep_source_metadata(
        self,
    ) -> None:
        d65 = Chromaticity(0.3127, 0.3290)
        display_p3 = SourceProfile(
            RgbPrimaries(
                red=Chromaticity(0.6800, 0.3200),
                green=Chromaticity(0.2650, 0.6900),
                blue=Chromaticity(0.1500, 0.0600),
                white=d65,
            ),
            TransferFunction.SRGB,
        )
        bt2020 = RgbPrimaries(
            red=Chromaticity(0.7080, 0.2920),
            green=Chromaticity(0.1700, 0.7970),
            blue=Chromaticity(0.1310, 0.0460),
            white=d65,
        )

        decoded_white = decode_rgb8((255, 255, 255), display_p3)
        xyz = source_rgb_to_xyz(decoded_white, display_p3)

        self.assertAlmostEqual(xyz[0], d65.x / d65.y, places=12)
        self.assertAlmostEqual(xyz[1], 1.0, places=12)
        self.assertAlmostEqual(
            xyz[2],
            (1.0 - d65.x - d65.y) / d65.y,
            places=12,
        )
        self.assertEqual(bt2020.red, Chromaticity(0.7080, 0.2920))

    def test_source_xyz_preserves_signed_intermediates(self) -> None:
        xyz = source_rgb_to_xyz((-0.5, 0.0, 0.0), SourceProfile.linear_srgb())

        self.assertLess(xyz[0], 0.0)
        self.assertLess(xyz[1], 0.0)
        self.assertLess(xyz[2], 0.0)

    def test_primitives_reject_wrong_component_counts(self) -> None:
        profile = SourceProfile.linear_srgb()
        with self.assertRaisesRegex(ValueError, "exactly three"):
            decode_rgb8((0, 0), profile)
        with self.assertRaisesRegex(ValueError, "exactly three"):
            source_rgb_to_xyz((0.0, 0.0), profile)
        with self.assertRaisesRegex(ValueError, "exactly three"):
            bradford_adaptation(
                (0.0, 0.0), Chromaticity(0.3127, 0.3290), Chromaticity(0.3457, 0.3585)
            )
        with self.assertRaisesRegex(ValueError, "exactly three"):
            xyz_to_lab((0.0, 0.0), (1.0, 1.0, 1.0))
        with self.assertRaisesRegex(ValueError, "exactly three"):
            delta_e2000((0.0, 0.0), (0.0, 0.0, 0.0))

    def test_bradford_adaptation_maps_d65_white_to_d50_white(self) -> None:
        d65 = Chromaticity(0.3127, 0.3290)
        d50 = Chromaticity(0.3457, 0.3585)
        d65_xyz = (d65.x / d65.y, 1.0, (1.0 - d65.x - d65.y) / d65.y)
        adapted = bradford_adaptation(d65_xyz, d65, d50)

        self.assertAlmostEqual(adapted[0], d50.x / d50.y, places=12)
        self.assertAlmostEqual(adapted[1], 1.0, places=12)
        self.assertAlmostEqual(
            adapted[2],
            (1.0 - d50.x - d50.y) / d50.y,
            places=12,
        )

    def test_ciede2000_matches_published_sharma_pairs(self) -> None:
        """Sharma/Wu/Dalal CIEDE2000 reference pairs, published values.

        These span the term-by-term traps the formula is known for: the hue
        arc across 0/360, near-neutral pairs where the chroma product is zero,
        low-lightness pairs, and the blue region where the rotation term is
        largest.
        """

        published = (
            ((50.0, 2.6772, -79.7751), (50.0, 0.0, -82.7485), 2.0425),
            ((50.0, 2.5, 0.0), (50.0, 0.0, -2.5), 4.3065),
            ((22.7233, 20.0904, -46.6940), (23.0331, 14.9730, -42.5619), 2.0373),
            ((36.4612, 47.8580, 18.3852), (36.2715, 50.5065, 21.2231), 1.4146),
            ((60.2574, -34.0099, 36.2677), (60.4626, -34.1751, 39.4387), 1.2644),
            ((35.0831, -44.1164, 3.7933), (35.0232, -40.0716, 1.5901), 1.8645),
            ((63.0109, -31.0961, -5.8663), (62.8187, -29.7946, -4.0864), 1.2630),
            ((61.2901, 3.7196, -5.3901), (61.4292, 2.2480, -4.9620), 1.8731),
            ((90.8027, -2.0831, 1.4410), (91.1528, -1.6435, 0.0447), 1.4441),
            ((2.0776, 0.0795, -1.1350), (0.9033, -0.0636, -0.5514), 0.9082),
        )
        for first, second, expected in published:
            with self.subTest(first=first, second=second):
                self.assertAlmostEqual(delta_e2000(first, second), expected, places=4)

    def test_ciede2000_rotation_uses_the_adjusted_chroma_mean(self) -> None:
        """R_C is defined over C-bar-prime, not the raw chroma mean.

        The published pairs above do not separate the two: at high chroma the
        G adjustment vanishes, and at low chroma R_C vanishes. They disagree
        only in the blue band around C ~ 15-25, where G is still large and
        R_C is turning on. This pair sits in that band, so it fails if the
        rotation magnitude is ever recomputed from the unadjusted mean.
        """

        self.assertAlmostEqual(
            delta_e2000((50.0, 2.0, -19.8), (50.0, 3.0, -13.8)),
            4.299463,
            places=6,
        )

    def test_ciede2000_reference_pair_and_profile_white_normalization(self) -> None:
        self.assertAlmostEqual(
            delta_e2000((50.0, 2.6772, -79.7751), (50.0, 0.0, -82.7485)),
            2.0425,
            places=4,
        )
        self.assertEqual(
            xyz_to_lab((0.96422, 1.0, 0.82521), (0.96422, 1.0, 0.82521)),
            (100.0, 0.0, 0.0),
        )


if __name__ == "__main__":
    unittest.main()
