"""Provider-selection contracts supplement real ESP32 Apa102 cross-builds."""

import re
from pathlib import Path


ROOT: Path = Path(__file__).resolve().parents[2]
BASE: Path = ROOT / "src/fl/system/fastpin_base.h"


def test_esp32_forced_pins_select_the_core_api_provider() -> None:
    source: str = BASE.read_text(encoding="utf-8").replace("\\\n", "")
    assert re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\s*&&\s*"
        r"\(defined\(FASTLED_NO_PINMAP\)\s*\|\|\s*defined\(FL_IS_ESP32\)\)\s*\n"
        r"#define FL_FASTPIN_HAS_NO_PINMAP",
        source,
    )
    assert "class FastPin : public NoPinMapPin<PIN>" in source


def test_esp32_specializations_retain_portless_provider_and_pin_validity() -> None:
    source: str = (ROOT / "src/platforms/esp/32/core/fastpin_esp32.h").read_text(
        encoding="utf-8"
    )
    software: str = source.split("#ifdef FL_FASTPIN_HAS_NO_PINMAP", 1)[1].split(
        "#else", 1
    )[0]
    assert "class _ESPPIN : public NoPinMapPin<PIN>" in software
    assert "return VALIDPIN;" in software
    assert "FL_STATIC_ASSERT(validpin()," in software
    assert "NoPinMapPin<PIN>::setOutput();" in software
    assert re.search(
        r"#ifndef FL_FASTPIN_HAS_NO_PINMAP\s*\n#define HAS_HARDWARE_PIN_SUPPORT\s*\n#endif",
        source,
    )


def test_unspecified_esp32_software_pins_are_invalid() -> None:
    source: str = BASE.read_text(encoding="utf-8")
    selected: str = source.split("#define FL_FASTPIN_HAS_NO_PINMAP", 1)[1].split(
        "#elif defined(FASTLED_FORCE_SOFTWARE_PINS)", 1
    )[0]
    assert "#ifdef FL_IS_ESP32" in selected
    esp32: str = selected.split("#ifdef FL_IS_ESP32", 1)[1].split("#else", 1)[0]
    assert "return false;" in esp32
    assert "FL_STATIC_ASSERT(PIN != PIN" in esp32
    assert "static void setOutput()" in esp32


def test_core_api_provider_is_available_without_the_legacy_no_map_flag() -> None:
    source: str = BASE.read_text(encoding="utf-8")
    assert re.search(
        r"#ifdef FASTLED_FORCE_SOFTWARE_PINS\s*\n"
        r'#include "fl/system/pin.h"[^\n]*\n'
        r'#include "fl/system/fastpin_no_pinmap.h"',
        source,
    )
