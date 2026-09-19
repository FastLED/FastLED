"""The bloat gate names colour-pipeline code that a profile-free Blink links.

Blink binds no colour profile, so it must link none of the pipeline: the
pipeline is reached only through the hooks `setColorProfile` installs
(FastLED#4455).
"""

from __future__ import annotations

from typing import Any

from typeguard import typechecked

from tests.test_esp32s3_bloat_regression import linked_pipeline_symbols


@typechecked
def _symbol(name: str) -> dict[str, Any]:
    return {"demangled": name, "size": 100, "region": "flash"}


@typechecked
def test_blink_symbols_are_not_pipeline_symbols() -> None:
    """What an unbound build does link must not trip the check.

    These are real Blink symbols with pipeline-sounding names: the profile
    API, its event lists and the empty hook table all exist in every build.
    """

    report = {
        "symbols": [
            _symbol("fl::colorPipelineHooks()::hooks"),
            _symbol("fl::Channel::colorPipeline() const"),
            _symbol("fl::CLEDController::staticEmitterProfile() const"),
            _symbol("fl::shared_ptr<fl::StreamingPipelineQ16>::reset()"),
            _symbol("fl::FluxScalar::unity()"),
            _symbol(
                "fl::rgb_2_rgbw_colorimetric(unsigned short, unsigned char, "
                "unsigned char, unsigned char)"
            ),
        ]
    }
    assert linked_pipeline_symbols(report) == []


@typechecked
def test_pipeline_entry_points_are_named() -> None:
    report = {
        "symbols": [
            _symbol("fl::installColorPipelineHooks()"),
            _symbol(
                "fl::processPixelQ16(fl::StreamingPipelineQ16 const&, "
                "unsigned char, unsigned char, unsigned char, long (&) [3])"
            ),
            _symbol(
                "fl::ColorManagedPixelSource::loadAndScaleRGB(unsigned char*, "
                "unsigned char*, unsigned char*)"
            ),
            _symbol("fl::Channel::colorPipeline() const"),
        ]
    }
    assert linked_pipeline_symbols(report) == [
        "fl::ColorManagedPixelSource::loadAndScaleRGB(unsigned char*, "
        "unsigned char*, unsigned char*)",
        "fl::installColorPipelineHooks()",
        "fl::processPixelQ16(fl::StreamingPipelineQ16 const&, unsigned char, "
        "unsigned char, unsigned char, long (&) [3])",
    ]


@typechecked
def test_malformed_reports_yield_nothing() -> None:
    assert linked_pipeline_symbols({}) == []
    assert linked_pipeline_symbols({"symbols": "not a list"}) == []
    assert linked_pipeline_symbols({"symbols": ["x", {"size": 3}]}) == []
