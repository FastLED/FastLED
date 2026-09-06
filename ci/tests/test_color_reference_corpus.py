"""Acceptance tests for P5's serialized float64 reference evidence."""

from __future__ import annotations

import json
import math
import unittest
from collections.abc import Sequence
from pathlib import Path

from ci.color_reference import (
    SourceProfile,
    _oklab_from_xyz,
    _oklch_from_xyz,
    _xyz_from_oklab,
    decode_rgb8,
    source_rgb_to_xyz,
)
from ci.color_reference_corpus import (
    CORPUS_SCHEMA_VERSION,
    build_color_reference_corpus,
    legacy_independent_gamma_video,
    legacy_rgb8_as_direct_drive,
    legacy_scale8_brightness,
    load_color_reference_golden,
    measure_color_reference_corpus,
    serialize_color_reference_corpus,
    validate_color_reference_corpus,
)


class TestColorReferenceCorpus(unittest.TestCase):
    def test_dense_corpus_is_deterministic_and_has_every_pipeline_stage(self) -> None:
        first = build_color_reference_corpus()
        second = build_color_reference_corpus()
        serialized = serialize_color_reference_corpus(first)

        self.assertEqual(serialized[0], "{")
        self.assertIsInstance(json.loads(serialized), dict)
        artifact_text = (
            Path(__file__).parents[1] / "golden" / "color-reference-v1.json"
        ).read_text(encoding="utf-8")
        self.assertEqual(artifact_text[0], "{")
        self.assertIsInstance(json.loads(artifact_text), dict)

        self.assertEqual(CORPUS_SCHEMA_VERSION, 1)
        self.assertEqual(serialized, serialize_color_reference_corpus(second))
        decoded = json.loads(serialized)
        self.assertEqual(decoded["schema_version"], CORPUS_SCHEMA_VERSION)
        # 3 RGB encodings × 4 emitter layouts × 16 input regimes: low-code,
        # neutral, and saturated vectors are independently represented.
        self.assertGreaterEqual(len(decoded["vectors"]), 192)
        self.assertEqual(
            [vector["id"] for vector in decoded["vectors"]],
            sorted(vector["id"] for vector in decoded["vectors"]),
        )
        self.assertEqual(
            set(decoded["coverage"]),
            {
                "low_code",
                "neutral_axis",
                "saturated_boundary",
                "display_p3",
                "bt2020",
                "rgb",
                "rgbw",
                "rgbww",
                "non_d65_white",
            },
        )
        observed_coverage = {
            label for vector in decoded["vectors"] for label in vector["coverage"]
        }
        self.assertEqual(observed_coverage, set(decoded["coverage"]))
        required_stages = {
            "encoded_rgb8",
            "linear_rgb",
            "source_xyz",
            "d65_xyz",
            "mapped_xyz",
            "emitter_light",
            "dimmed_light",
            "physical_drive",
        }
        for vector in decoded["vectors"]:
            self.assertEqual(set(vector["stages"]), required_stages)
            self.assertIn(
                vector["source_profile"], {"srgb_bt709", "display_p3", "bt2020"}
            )
            self.assertIn(
                vector["device_profile"], {"rgb", "rgbw", "rgbww", "non_d65_white"}
            )
            for value in vector["stages"].values():
                self.assertTrue(all(math.isfinite(component) for component in value))

    def test_serialized_golden_validation_reports_stage_and_delta_e_budgets(
        self,
    ) -> None:
        corpus = load_color_reference_golden()
        report = validate_color_reference_corpus(
            serialize_color_reference_corpus(corpus)
        )

        self.assertEqual(report["vector_count"], len(corpus["vectors"]))
        self.assertLessEqual(report["max_stage_abs_error"], 1e-12)
        self.assertLessEqual(report["max_delta_e2000"], 1e-9)
        self.assertGreater(report["delta_e_inapplicable_count"], 0)
        self.assertEqual(
            report["delta_e_pair_count"] + report["delta_e_inapplicable_count"],
            report["vector_count"],
        )
        corrupted = json.loads(serialize_color_reference_corpus(corpus))
        corrupted["vectors"][0]["stages"]["source_xyz"][0] += 1e-4
        with self.assertRaisesRegex(ValueError, "source_xyz|stage"):
            validate_color_reference_corpus(json.dumps(corrupted))

        perturbed = json.loads(serialize_color_reference_corpus(corpus))
        perturbed["vectors"][0]["stages"]["d65_xyz"][1] += 1e-4
        perturbed_report = measure_color_reference_corpus(json.dumps(perturbed))
        self.assertGreater(perturbed_report["max_delta_e2000"], 0.0)
        with self.assertRaisesRegex(ValueError, "D65|delta"):
            validate_color_reference_corpus(json.dumps(perturbed))

        # The corpus deliberately retains signed wide XYZ.  A negative D65
        # component is not silently clipped for Lab: it is counted as
        # inapplicable, but its exact stage remains an acceptance invariant.
        signed_index = next(
            index
            for index, vector in enumerate(corpus["vectors"])
            if any(component < 0.0 for component in vector["stages"]["d65_xyz"])
        )
        signed_corrupted = json.loads(serialize_color_reference_corpus(corpus))
        signed_corrupted["vectors"][signed_index]["stages"]["d65_xyz"][2] -= 1e-4
        signed_report = measure_color_reference_corpus(json.dumps(signed_corrupted))
        self.assertGreater(signed_report["delta_e_inapplicable_count"], 0)
        with self.assertRaisesRegex(ValueError, "D65|stage"):
            validate_color_reference_corpus(json.dumps(signed_corrupted))

        anchor_corrupted = json.loads(serialize_color_reference_corpus(corpus))
        anchor_corrupted["independent_anchors"]["srgb_128_linear"] = 0.0
        with self.assertRaisesRegex(ValueError, "anchor"):
            validate_color_reference_corpus(json.dumps(anchor_corrupted))

        # These anchors are independently published reference values, not
        # values generated by the corpus mapper in this test run.
        anchors = corpus["independent_anchors"]
        self.assertAlmostEqual(anchors["srgb_128_linear"], 0.2158605001, places=10)
        self.assertAlmostEqual(anchors["bt709_red_xyz_y"], 0.2126390059, places=10)

    def test_legacy_rgb8_counterexamples_are_explicit_red_evidence(self) -> None:
        # This is the deployed CRGB/scale8 legacy model, not a replacement
        # renderer: source codes are direct drives and scale8 uses
        # (value * (scale + 1)) >> 8 (src/lib8tion.h).
        self.assertEqual(legacy_rgb8_as_direct_drive((128, 128, 128)), (128 / 255,) * 3)
        self.assertGreater(legacy_rgb8_as_direct_drive((128, 128, 128))[0], 0.5)
        self.assertLess(
            decode_rgb8((128, 128, 128), SourceProfile.srgb_bt709())[0], 0.22
        )

        # Early scale8 quantization loses a low code entirely; nonlinear
        # green response makes code-domain brightness physically wrong too.
        self.assertEqual(legacy_scale8_brightness((1, 1, 1), 64), (0, 0, 0))
        legacy_green_drive = legacy_scale8_brightness((255, 255, 255), 64)[1] / 255
        self.assertLess(legacy_green_drive**2, 0.07)
        self.assertAlmostEqual(0.5**2, 0.25)

    def test_independent_channel_gamma_is_not_hue_or_neutral_preserving(self) -> None:
        """RED evidence (c): #4032's independent-8-bit-gamma regression.

        `applyGamma_video` transforms each channel on its own in the code
        domain. That operation is not a luminance scale, so it moves colors
        across the two axes the managed pipeline is required to hold: a
        neutral stays neutral, and an in-gamut hue is preserved.
        """

        profile = SourceProfile.srgb_bt709()

        def oklch_of(codes: Sequence[int]):
            return _oklch_from_xyz(
                source_rgb_to_xyz(decode_rgb8(codes, profile), profile)
            )

        # Neutral-axis regression: per-channel gammas that differ at all pull
        # an exactly neutral code off the neutral axis. The 3-argument
        # applyGamma_video overload makes this reachable from the public API.
        neutral = (128, 128, 128)
        self.assertLess(oklch_of(neutral).chroma, 1e-6)

        skewed = legacy_independent_gamma_video(neutral, (2.2, 2.0, 2.4))
        self.assertNotEqual(skewed[0], skewed[1])
        self.assertGreater(oklch_of(skewed).chroma, 1e-3)

        # Hue regression: even a single shared exponent shifts hue, because
        # the power law acts on each channel's code rather than on luminance.
        saturated = (200, 120, 60)
        gammaed = legacy_independent_gamma_video(saturated, (2.2, 2.2, 2.2))
        hue_shift = abs(
            (oklch_of(gammaed).hue_degrees - oklch_of(saturated).hue_degrees + 180.0)
            % 360.0
            - 180.0
        )
        self.assertGreater(hue_shift, 1.0)

    def test_signed_wide_oklab_uses_real_cube_roots_not_complex_powers(self) -> None:
        # A bounded imaginary-source XYZ can have a negative LMS component.
        # It is valid in the signed wide intermediate domain, even though it
        # is later rejected for physical-emitter mapping.
        signed_xyz = (-1.0, 0.10, 0.20)

        lab = _oklab_from_xyz(signed_xyz)
        round_trip = _xyz_from_oklab(lab)

        self.assertTrue(all(math.isfinite(component) for component in lab))
        for actual, expected in zip(round_trip, signed_xyz):
            self.assertAlmostEqual(actual, expected, places=10)
