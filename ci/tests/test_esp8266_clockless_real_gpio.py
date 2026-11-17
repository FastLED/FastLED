"""ESP8266 clockless must hand sendPixel the real GPIO, not the logical pin.

Regression for 3.10.9 (#4917): with the D1 or NodeMCU pin order, DATA_PIN is a
board label (NodeMCU D2 is GPIO4), but sendPixel writes GPOS/GPOC/GP16O directly.
"""

import re
from pathlib import Path


ROOT: Path = Path(__file__).resolve().parents[2]
DRIVER: Path = ROOT / "src/platforms/esp/8266/clockless_esp8266.h"
SEND_PIXEL: Path = ROOT / "src/platforms/esp/8266/clockless_esp8266.cpp.hpp"
FASTPIN: Path = ROOT / "src/platforms/esp/8266/fastpin_esp8266.h"


def _send_pixel_call_args() -> str:
    source: str = DRIVER.read_text(encoding="utf-8")
    match = re.search(r"sendPixel\((.*?)\)\)", source, re.DOTALL)
    assert match is not None, "sendPixel call not found in clockless_esp8266.h"
    return " ".join(match.group(1).split())


def test_send_pixel_takes_a_raw_gpio() -> None:
    source: str = SEND_PIXEL.read_text(encoding="utf-8")
    assert "1u << (pin & 0x0F)" in source
    assert "pin >= 16" in source


def test_clockless_passes_the_real_gpio() -> None:
    args: str = _send_pixel_call_args()
    assert "FastPin<DATA_PIN>::kRealPin" in args
    assert not re.search(r"(?<!<)\bDATA_PIN\b(?!>)", args), args


def test_real_pin_exists_on_native_and_forced_providers() -> None:
    source: str = FASTPIN.read_text(encoding="utf-8")
    assert source.count("static constexpr u8 kRealPin = PIN;") == 2
    assert "public _ESPPIN<REAL_PIN," in source
