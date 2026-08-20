"""Regression contract for ESP32 FastPin atomic GPIO toggling."""

from pathlib import Path


SOURCE = (
    Path(__file__).resolve().parents[2]
    / "src"
    / "platforms"
    / "esp"
    / "32"
    / "core"
    / "fastpin_esp32.h"
)


def test_esp32_fastpin_toggle_never_rewrites_unrelated_gpio_bits() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    toggle = source[
        source.index("inline static void toggle()") : source.index(
            "inline static void hi(FASTLED_REGISTER"
        )
    ]

    assert "*port() ^= MASK" not in toggle
    assert "if (isset())" in toggle
    assert "lo();" in toggle
    assert "hi();" in toggle
