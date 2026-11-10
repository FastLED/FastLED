"""AVR clockless rejects mapped software pins without compile-time ports."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_avr_clockless_rejects_mapped_software_pins() -> None:
    pin = (ROOT / "src/fl/system/fastpin_base.h").read_text(encoding="utf-8")
    clockless = (ROOT / "src/platforms/avr/clockless_avr.h").read_text(encoding="utf-8")
    assert "#define FL_FASTPIN_HAS_RUNTIME_PORT" in pin
    assert "#ifdef FL_FASTPIN_HAS_RUNTIME_PORT" in clockless
    assert (
        "FASTLED_FORCE_SOFTWARE_PINS is not supported by the AVR clockless driver"
        in clockless
    )
    assert "FL_STATIC_ASSERT(DATA_PIN != DATA_PIN" in clockless
    assert "#else // FL_FASTPIN_HAS_RUNTIME_PORT" in clockless
