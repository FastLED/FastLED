"""Selection contracts for ESP8266 forced software pins (#5068); see the PR for cross-build evidence."""

import re
from pathlib import Path


ROOT: Path = Path(__file__).resolve().parents[2]
BASE: Path = ROOT / "src/fl/system/fastpin_base.h"
HEADER: Path = ROOT / "src/platforms/esp/8266/fastpin_esp8266.h"


def test_esp8266_forced_pins_select_the_core_api_provider() -> None:
    source: str = BASE.read_text(encoding="utf-8").replace("\\\n", "")
    assert re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\s*&&\s*\([^\n]*"
        r"defined\(FL_IS_ESP8266\)\)\s*\n#define FL_FASTPIN_HAS_NO_PINMAP",
        source,
    )


def test_unspecified_esp8266_software_pins_are_invalid() -> None:
    source: str = BASE.read_text(encoding="utf-8")
    selected: str = source.split("#define FL_FASTPIN_HAS_NO_PINMAP", 1)[1].split(
        "#elif defined(FASTLED_FORCE_SOFTWARE_PINS)", 1
    )[0]
    guard: str = "#if defined(FL_IS_ESP32) || defined(FL_IS_ESP8266)"
    assert guard in selected
    checked: str = selected.split(guard, 1)[1].split("#else", 1)[0]
    assert "return false;" in checked
    assert "FL_STATIC_ASSERT(PIN != PIN" in checked


def test_every_pin_order_specialization_uses_the_portless_provider() -> None:
    source: str = HEADER.read_text(encoding="utf-8")
    software: str = source.split("#ifdef FL_FASTPIN_HAS_NO_PINMAP", 1)[1].split(
        "#else", 1
    )[0]
    # RAW, D1 and NodeMCU orders all specialize through _FL_DEFPIN -> _ESPPIN,
    # so the forced provider must replace _ESPPIN itself, keeping the real pad.
    assert "class _ESPPIN : public NoPinMapPin<PIN>" in software
    assert "kRealPin = PIN;" in software
    assert "public _ESPPIN<REAL_PIN," in source
    assert re.search(
        r"#ifndef FL_FASTPIN_HAS_NO_PINMAP\s*\n#define HAS_HARDWARE_PIN_SUPPORT\s*\n#endif",
        source,
    )


def test_register_level_bitbang_policy_is_native_only() -> None:
    source: str = HEADER.read_text(encoding="utf-8")
    assert (
        "#if FL_ESP8266_ENABLE_FAST_SOFTWARE_SPI && !defined(FL_FASTPIN_HAS_NO_PINMAP)"
        in source
    )


def test_block_clockless_rejects_forced_software_pins() -> None:
    source: str = (ROOT / "src/platforms/esp/8266/clockless_block_esp8266.h").read_text(
        encoding="utf-8"
    )
    assert re.search(
        r"#ifdef FL_FASTPIN_HAS_NO_PINMAP\s*\n[^\n]*\n\s*FL_STATIC_ASSERT\(FIRST_PIN != FIRST_PIN,",
        source,
    )
