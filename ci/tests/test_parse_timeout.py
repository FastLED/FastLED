"""Tests for ci.util.sketch_resolver.parse_timeout (FastLED #3309).

Covers the time-notation parsing surface that autoresearch's --timeout
argument feeds: plain seconds, ms / s / m / h suffixes, error cases.
"""

import pytest

from ci.util.sketch_resolver import parse_timeout


class TestParseTimeout:
    """parse_timeout accepts standard time notation strings."""

    def test_plain_number_is_seconds(self) -> None:
        assert parse_timeout("30") == 30
        assert parse_timeout("60") == 60
        assert parse_timeout("1") == 1

    def test_seconds_suffix(self) -> None:
        assert parse_timeout("30s") == 30
        assert parse_timeout("90s") == 90
        # Case-insensitive
        assert parse_timeout("30S") == 30

    def test_minutes_suffix(self) -> None:
        assert parse_timeout("1m") == 60
        assert parse_timeout("2m") == 120
        assert parse_timeout("10m") == 600

    def test_milliseconds_suffix(self) -> None:
        # 5000ms == 5 s
        assert parse_timeout("5000ms") == 5
        # Sub-second timeouts saturate to 1 (min positive integer)
        assert parse_timeout("500ms") == 1

    def test_hours_suffix_new_in_3309(self) -> None:
        # FastLED #3309: added `h` suffix for hour-scale timeouts so
        # debugger / interactive sessions don't have to type 3600.
        assert parse_timeout("1h") == 3600
        assert parse_timeout("2h") == 7200

    def test_whitespace_tolerated(self) -> None:
        assert parse_timeout("  30  ") == 30
        assert parse_timeout(" 2m") == 120

    def test_decimal_values(self) -> None:
        assert parse_timeout("1.5m") == 90
        assert parse_timeout("0.5h") == 1800

    def test_invalid_format_raises(self) -> None:
        with pytest.raises(ValueError, match="Invalid timeout format"):
            parse_timeout("abc")
        with pytest.raises(ValueError, match="Invalid timeout format"):
            parse_timeout("30sx")
        with pytest.raises(ValueError, match="Invalid timeout format"):
            parse_timeout("30 minutes")

    def test_non_positive_raises(self) -> None:
        with pytest.raises(ValueError, match="must be positive"):
            parse_timeout("0")
        with pytest.raises(ValueError, match="must be positive"):
            parse_timeout("0s")
