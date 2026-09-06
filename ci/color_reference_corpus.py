"""Deterministic P5 float64 corpus and legacy-compatibility evidence.

The committed JSON artifact is intentionally separate from this generator.
Validation recomputes every named vector and compares it to that artifact;
the independent anchors document values not derived from this mapper.
"""

from __future__ import annotations

import json
from collections.abc import Sequence
from pathlib import Path
from typing import Any

from ci.color_reference import (
    _D65,
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
    map_and_solve,
    source_rgb_to_xyz,
    xyz_to_lab,
)


CORPUS_SCHEMA_VERSION = 1
_GOLDEN_PATH = Path(__file__).with_name("golden") / "color-reference-v1.json"
_COVERAGE = (
    "low_code",
    "neutral_axis",
    "saturated_boundary",
    "display_p3",
    "bt2020",
    "rgb",
    "rgbw",
    "rgbww",
    "non_d65_white",
)
_INDEPENDENT_ANCHORS = {
    "srgb_128_linear": 0.2158605001,
    "bt709_red_xyz_y": 0.2126390059,
}


def legacy_rgb8_as_direct_drive(
    encoded: Sequence[int],
) -> tuple[float, float, float]:  # noqa: DCT002
    """Model legacy CRGB's direct-code interpretation, not a managed decoder."""

    if len(encoded) != 3 or any(not 0 <= component <= 255 for component in encoded):
        raise ValueError("legacy RGB8 requires three byte components")
    return (encoded[0] / 255.0, encoded[1] / 255.0, encoded[2] / 255.0)


def legacy_scale8_brightness(
    encoded: Sequence[int], brightness: int
) -> tuple[int, int, int]:  # noqa: DCT002
    """Match FastLED scale8: ``(value * (scale + 1)) >> 8``.

    Provenance: ``src/lib8tion.h`` documents the direct eight-bit legacy
    scaling path. This helper is evidence for P5's managed-mode exclusion; it
    is not a replacement renderer.
    """

    if len(encoded) != 3 or any(not 0 <= component <= 255 for component in encoded):
        raise ValueError("legacy RGB8 requires three byte components")
    if not 0 <= brightness <= 255:
        raise ValueError("legacy brightness requires a byte")
    return (
        (encoded[0] * (brightness + 1)) >> 8,
        (encoded[1] * (brightness + 1)) >> 8,
        (encoded[2] * (brightness + 1)) >> 8,
    )


def legacy_independent_gamma_video(
    encoded: Sequence[int], gamma_rgb: Sequence[float]
) -> tuple[int, int, int]:  # noqa: DCT002
    """Match FastLED ``applyGamma_video(CRGB, gammaR, gammaG, gammaB)``.

    Provenance: ``src/fl/gfx/colorutils.cpp.hpp``. The CRGB overloads call the
    scalar ``applyGamma_video`` once per channel, so each channel is
    transformed independently of the other two, in the eight-bit code domain.
    The scalar path rounds half-up and refuses to take a positive code down to
    zero; both are reproduced here so the counterexample is the deployed
    behaviour rather than an idealised power law.

    This models the legacy path as evidence for P5's RED baseline. It is not a
    replacement renderer.
    """

    if len(encoded) != 3 or any(not 0 <= component <= 255 for component in encoded):
        raise ValueError("legacy RGB8 requires three byte components")
    if len(gamma_rgb) != 3 or any(gamma <= 0.0 for gamma in gamma_rgb):
        raise ValueError("legacy gamma requires three positive exponents")

    adjusted: list[int] = []
    for component, gamma in zip(encoded, gamma_rgb):
        if component == 0:
            adjusted.append(0)
            continue
        raw = (component / 255.0) ** gamma
        scaled = int(raw * 255.0 + 0.5)
        adjusted.append(max(1, min(255, scaled)))
    return (adjusted[0], adjusted[1], adjusted[2])


def _source_profiles() -> dict[str, SourceProfile]:
    d65 = _D65
    return {
        "srgb_bt709": SourceProfile(RgbPrimaries.bt709(), TransferFunction.SRGB),
        "display_p3": SourceProfile(
            RgbPrimaries(
                Chromaticity(0.6800, 0.3200),
                Chromaticity(0.2650, 0.6900),
                Chromaticity(0.1500, 0.0600),
                d65,
            ),
            TransferFunction.SRGB,
        ),
        "bt2020": SourceProfile(
            RgbPrimaries(
                Chromaticity(0.7080, 0.2920),
                Chromaticity(0.1700, 0.7970),
                Chromaticity(0.1310, 0.0460),
                d65,
            ),
            TransferFunction.BT709,
        ),
    }


def _device_profiles() -> dict[str, DeviceProfile]:
    rgb = (
        Emitter("red", Chromaticity(0.6400, 0.3300), 1.0, False),
        Emitter("green", Chromaticity(0.3000, 0.6000), 1.0, False),
        Emitter("blue", Chromaticity(0.1500, 0.0600), 1.0, False),
    )
    d50 = Chromaticity(0.3457, 0.3585)
    return {
        "rgb": DeviceProfile(rgb, _D65),
        "rgbw": DeviceProfile(rgb + (Emitter("white", _D65, 1.0, True),), _D65),
        "rgbww": DeviceProfile(
            rgb
            + (
                Emitter("cool", Chromaticity(0.3127, 0.3290), 1.0, True),
                Emitter("warm", Chromaticity(0.3457, 0.3585), 1.0, True),
            ),
            _D65,
        ),
        "non_d65_white": DeviceProfile(rgb + (Emitter("white", d50, 1.0, True),), d50),
    }


def _scenario_inputs() -> tuple[tuple[str, tuple[int, int, int]], ...]:
    return (
        ("low_code", (1, 1, 1)),
        ("low_code", (1, 0, 0)),
        ("low_code", (0, 1, 0)),
        ("low_code", (0, 0, 1)),
        ("low_code", (2, 4, 8)),
        ("low_code", (8, 32, 96)),
        ("neutral_axis", (128, 128, 128)),
        ("neutral_axis", (16, 16, 16)),
        ("neutral_axis", (240, 240, 240)),
        ("saturated_boundary", (255, 0, 0)),
        ("saturated_boundary", (0, 255, 0)),
        ("saturated_boundary", (0, 0, 255)),
        ("saturated_boundary", (255, 255, 0)),
        ("saturated_boundary", (0, 255, 255)),
        ("saturated_boundary", (255, 0, 255)),
        ("saturated_boundary", (255, 64, 0)),
    )


def _vector(
    vector_id: str,
    scenario: str,
    encoded: tuple[int, int, int],  # noqa: DCT002
    source_name: str,
    device_name: str,
    source: SourceProfile,
    device: DeviceProfile,
) -> dict[str, Any]:
    linear_rgb = decode_rgb8(encoded, source)
    source_xyz = source_rgb_to_xyz(linear_rgb, source)
    d65_xyz = (
        source_xyz
        if source.primaries.white == _D65
        else bradford_adaptation(source_xyz, source.primaries.white, _D65)
    )
    target_xyz = (
        d65_xyz
        if device.rendering_white == _D65
        else bradford_adaptation(d65_xyz, _D65, device.rendering_white)
    )
    solution = map_and_solve(target_xyz, device)
    dimmed = tuple(value * 0.25 for value in solution.drives)
    exponents = tuple(1.0 if index % 2 == 0 else 2.0 for index in range(len(dimmed)))
    physical_drive = apply_brightness_then_inverse_response(
        solution.drives, 0.25, exponents
    )
    return {
        "id": vector_id,
        "scenario": scenario,
        "coverage": [],
        "source_profile": source_name,
        "device_profile": device_name,
        "stages": {
            "encoded_rgb8": [float(value) for value in encoded],
            "linear_rgb": list(linear_rgb),
            "source_xyz": list(source_xyz),
            "d65_xyz": list(d65_xyz),
            "mapped_xyz": list(solution.mapped_xyz),
            "emitter_light": list(solution.drives),
            "dimmed_light": list(dimmed),
            "physical_drive": list(physical_drive),
        },
    }


def build_color_reference_corpus() -> dict[str, Any]:
    """Build the 192-vector reference corpus from named source/device cases."""

    vectors: list[dict[str, Any]] = []
    profiles = _source_profiles()
    devices = _device_profiles()
    for source_name, source in profiles.items():
        source_scenario = (
            "display_p3"
            if source_name == "display_p3"
            else "bt2020"
            if source_name == "bt2020"
            else None
        )
        for device_name, device in devices.items():
            device_scenario = (
                device_name if device_name != "non_d65_white" else "non_d65_white"
            )
            for index, (base_scenario, encoded) in enumerate(_scenario_inputs()):
                scenario = source_scenario or device_scenario or base_scenario
                vector = _vector(
                    f"{source_name}-{device_name}-{index:02d}",
                    scenario,
                    encoded,
                    source_name,
                    device_name,
                    source,
                    device,
                )
                vector["coverage"] = [
                    *([source_scenario] if source_scenario is not None else []),
                    device_scenario,
                    base_scenario,
                ]
                vectors.append(vector)
    vectors.sort(key=lambda vector: str(vector["id"]))
    return {
        "schema_version": CORPUS_SCHEMA_VERSION,
        "coverage": list(_COVERAGE),
        "normalization": "XYZ Y=1 rendering white; dimmed_light uses brightness=0.25",
        "dark_floor": "No black denominator: ΔE uses CIELAB; stage error is absolute.",
        "provenance": {
            "source": "IEC 61966-2-1 sRGB; BT.709/P3/BT.2020 primaries",
            "adaptation": "ICC.1:2022 Annex E Bradford",
            "metric": "ISO/CIE 11664-6:2022 CIEDE2000",
            "legacy": "FastLED src/lib8tion.h scale8 direct code arithmetic",
        },
        "independent_anchors": _INDEPENDENT_ANCHORS,
        "vectors": vectors,
    }


def serialize_color_reference_corpus(corpus: dict[str, Any]) -> str:
    """Canonical JSON serialization used by the checked-in golden artifact."""

    return json.dumps(corpus, allow_nan=False, indent=2, sort_keys=True) + "\n"


def load_color_reference_golden() -> dict[str, Any]:
    """Load the independently stored corpus; never regenerate it implicitly."""

    return json.loads(_GOLDEN_PATH.read_text(encoding="utf-8"))


def measure_color_reference_corpus(serialized: str) -> dict[str, float | int]:
    """Measure artifact/reference divergence without applying acceptance budgets."""

    artifact = json.loads(serialized)
    if artifact.get("schema_version") != CORPUS_SCHEMA_VERSION:
        raise ValueError("unsupported corpus schema version")
    if artifact.get("independent_anchors") != _INDEPENDENT_ANCHORS:
        raise ValueError("independent anchor provenance mismatch")
    expected = build_color_reference_corpus()
    if artifact.get("coverage") != expected["coverage"]:
        raise ValueError("coverage mismatch")
    actual_vectors = artifact.get("vectors")
    expected_vectors = expected["vectors"]
    if not isinstance(actual_vectors, list) or len(actual_vectors) != len(
        expected_vectors
    ):
        raise ValueError("vector count mismatch")
    max_stage_error = 0.0
    max_delta_e = 0.0
    delta_e_pair_count = 0
    delta_e_inapplicable_count = 0
    for actual, reference in zip(actual_vectors, expected_vectors):
        if actual.get("id") != reference["id"]:
            raise ValueError("vector identifier mismatch")
        if actual.get("coverage") != reference["coverage"]:
            raise ValueError("vector coverage mismatch")
        stages = actual.get("stages")
        if not isinstance(stages, dict):
            raise ValueError(f"stage payload missing for {reference['id']}")
        for stage, expected_value in reference["stages"].items():
            actual_value = stages.get(stage)
            if not isinstance(actual_value, list) or len(actual_value) != len(
                expected_value
            ):
                raise ValueError(f"stage {stage} shape mismatch")
            error = max(
                abs(float(left) - float(right))
                for left, right in zip(actual_value, expected_value)
            )
            max_stage_error = max(max_stage_error, error)
            if error > 1e-12 and stage != "d65_xyz":
                raise ValueError(f"stage {stage} differs from reference")
        actual_d65 = stages["d65_xyz"]
        reference_d65 = reference["stages"]["d65_xyz"]
        # CIELAB is undefined for signed wide XYZ.  Preserve those stages
        # exactly for acceptance, but make their absence from the perceptual
        # metric visible instead of clipping or pretending they are black.
        if all(component >= 0.0 for component in actual_d65) and all(
            component >= 0.0 for component in reference_d65
        ):
            white = (0.9504559270516716, 1.0, 1.0890577507598784)
            max_delta_e = max(
                max_delta_e,
                delta_e2000(
                    xyz_to_lab(tuple(actual_d65), white),
                    xyz_to_lab(tuple(reference_d65), white),
                ),
            )
            delta_e_pair_count += 1
        else:
            delta_e_inapplicable_count += 1

    def anchored_vector(encoded: list[float]) -> dict[str, Any]:
        matches = [
            vector
            for vector in actual_vectors
            if vector.get("source_profile") == "srgb_bt709"
            and vector.get("device_profile") == "rgb"
            and vector.get("stages", {}).get("encoded_rgb8") == encoded
        ]
        if len(matches) != 1:
            raise ValueError("independent anchor vector is missing or ambiguous")
        return matches[0]

    srgb_128 = anchored_vector([128.0, 128.0, 128.0])
    bt709_red = anchored_vector([255.0, 0.0, 0.0])
    if (
        abs(
            srgb_128["stages"]["linear_rgb"][0]
            - _INDEPENDENT_ANCHORS["srgb_128_linear"]
        )
        > 1e-10
    ):
        raise ValueError("srgb independent anchor mismatch")
    if (
        abs(
            bt709_red["stages"]["source_xyz"][1]
            - _INDEPENDENT_ANCHORS["bt709_red_xyz_y"]
        )
        > 1e-10
    ):
        raise ValueError("BT.709 independent anchor mismatch")
    return {
        "vector_count": len(expected_vectors),
        "max_stage_abs_error": max_stage_error,
        "max_delta_e2000": max_delta_e,
        "delta_e_pair_count": delta_e_pair_count,
        "delta_e_inapplicable_count": delta_e_inapplicable_count,
    }


def validate_color_reference_corpus(serialized: str) -> dict[str, float | int]:
    """Measure then enforce the artifact's float64 stage-error budgets."""

    report = measure_color_reference_corpus(serialized)
    if report["max_stage_abs_error"] > 1e-12:
        raise ValueError("D65 stage error exceeds 1e-12")
    if report["max_delta_e2000"] > 1e-9:
        raise ValueError("delta E2000 exceeds 1e-9")
    return report
