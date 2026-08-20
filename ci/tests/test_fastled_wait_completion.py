"""Regression contract for the public FastLED.wait() overloads."""

from pathlib import Path


SOURCE = Path(__file__).resolve().parents[2] / "src" / "FastLED.cpp.hpp"


def test_no_argument_wait_requests_unbounded_completion() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    no_argument_wait = source[
        source.index("void CFastLED::wait()") : source.index(
            "bool CFastLED::wait(fl::u32 timeout_ms)"
        )
    ]

    assert "manager.waitForReady(0);" in no_argument_wait
    assert "manager.waitForReady();" not in no_argument_wait


def test_bounded_wait_forwards_the_callers_timeout() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    bounded_wait = source[
        source.index("bool CFastLED::wait(fl::u32 timeout_ms)") : source.index(
            "// Tiered-wait spin-budget shims"
        )
    ]

    assert "return manager.waitForReady(timeout_ms);" in bounded_wait
