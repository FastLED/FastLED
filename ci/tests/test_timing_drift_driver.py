"""Tests for the #2994 timing-drift host driver (FastLED#3765).

The RPC round-trip needs a board, but the analysis on top of it does not --
and the analysis is where a wrong number would quietly mislead the
investigation. These pin the arithmetic.
"""

from __future__ import annotations

import pytest

from ci.autoresearch.test_timing_drift import (
    THEORETICAL_MS,
    WS2812_35_LED_WIRE_US,
    _summarize_sequences,
    _summarize_show,
)


def test_theoretical_period_matches_the_reporters_sketch() -> None:
    """225 ms arm + 254 fade steps * 5 ms + 1000 ms delay = 2495 ms.

    This is the number #2994 measures against; 3.10.3 hits it exactly.
    """
    assert THEORETICAL_MS == 2495
    assert THEORETICAL_MS == 225 + 254 * 5 + 1000


def test_ws2812_wire_floor_is_the_35_led_frame_time() -> None:
    """35 LEDs x 24 bits x 1.25 us = 1050 us of unavoidable wire time."""
    assert WS2812_35_LED_WIRE_US == pytest.approx(1050.0)


def test_first_sequence_is_excluded_from_the_histogram(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Index 0 is a partial sequence and must not skew the stats.

    It has no leading 225 ms arm and no trailing 1000 ms delay, so including
    it would understate the mean. Here index 0 is an absurd outlier: if it
    leaked in, min/mean would move sharply.
    """
    _summarize_sequences([1, 2495, 2495, 2495])
    out = capsys.readouterr().out

    assert "sequences        : 3" in out
    assert "min / mean / max : 2495 / 2495.0 / 2495" in out
    # Zero drift when every usable sequence hits theoretical exactly.
    assert "+0 .. +0 ms" in out


def test_drift_is_reported_against_theoretical(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """The reporter's observed range (2563-2752) should read as +68..+257."""
    _summarize_sequences([0, 2563, 2752])
    out = capsys.readouterr().out
    assert "+68 .. +257 ms" in out


def test_single_sequence_is_reported_as_insufficient(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """With only the partial sequence there is nothing usable to summarize."""
    _summarize_sequences([2495])
    out = capsys.readouterr().out
    assert "only one sequence recorded" in out


def test_show_summary_reports_mean_and_wire_ratio(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """show() mean and its ratio to the wire floor are the decisive numbers."""
    _summarize_show(
        {
            "show_count": 100,
            "show_total_us": 210000,  # mean 2100us == 2x the 1050us floor
            "show_min_us": 2000,
            "show_max_us": 2500,
        }
    )
    out = capsys.readouterr().out
    assert "show() calls     : 100" in out
    assert "2000 / 2100.0 / 2500 us" in out
    # 2000 / 1050 = 1.90x
    assert "1.90x" in out


def test_show_summary_handles_missing_timing(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Older firmware without show() timing must not crash the driver."""
    _summarize_show({})
    out = capsys.readouterr().out
    assert "no show() timing" in out
