"""Tests for fbuild board routing decisions."""

from ci.compiler.fbuild_boards import FBUILD_BOARDS


def test_teensy_4x_stays_on_platformio_until_fbuild_framework_libs_work() -> None:
    """Teensy 4.x needs Teensyduino framework libraries that fbuild misses."""
    assert "teensy40" not in FBUILD_BOARDS
    assert "teensy41" not in FBUILD_BOARDS


def test_known_fbuild_boards_remain_enabled() -> None:
    assert "uno" in FBUILD_BOARDS
    assert "esp32dev" in FBUILD_BOARDS
