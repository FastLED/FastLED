"""MGM240 must honor FASTLED_FORCE_SOFTWARE_PINS (#5072).

`_FL_DEFPIN` in `platforms/arm/mgm240/fastpin_arm_mgm240.h` used to expand to
an explicit `fl::FastPin<PIN>` specialization, which overrides the forced
software primary template in `fastpin_base.h`. It now always emits the native
`Mgm240HwPin<PIN>` and adds the `FastPin` specialization only outside forced
mode. The timing-critical clockless and ezWS2812 drivers use `Mgm240HwPin`
directly.

The headers are parsed textually; real-SDK builds cover selection.
"""

import re
from pathlib import Path


MGM240 = Path(__file__).resolve().parents[2] / "src" / "platforms" / "arm" / "mgm240"
DRIVERS = (
    "clockless_arm_mgm240.h",
    "clockless_ezws2812_39mhz.h",
    "clockless_ezws2812_78mhz.h",
)


def _read(name: str) -> str:
    return (MGM240 / name).read_text(encoding="utf-8")


def _fastpin_branches() -> list[str]:
    match = re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\n(#define _FL_MGM240_FASTPIN.*?)#else\n(.*?)#endif",
        _read("fastpin_arm_mgm240.h"),
        re.S,
    )
    assert match, "force guard around _FL_MGM240_FASTPIN not found"
    return [match.group(1), match.group(2)]


def test_forced_mode_adds_no_fastpin_specialization() -> None:
    forced, normal = _fastpin_branches()
    assert "FastPin" not in forced
    assert "template<> class FastPin<PIN> : public Mgm240HwPin<PIN>" in normal


def test_defpin_emits_native_pin_and_conditional_fastpin() -> None:
    text = _read("fastpin_arm_mgm240.h")
    assert text.count("#define _FL_DEFPIN(") == 1
    body = re.search(r"#define _FL_DEFPIN\(.*?\n((?:.*\\\n)*.*)", text)
    assert body
    assert "class Mgm240HwPin<PIN>" in body.group(1)
    assert "_FL_MGM240_FASTPIN(PIN)" in body.group(1)
    assert "FastPin" not in body.group(1)


def test_timing_critical_drivers_use_the_native_pin() -> None:
    for name in DRIVERS:
        text = _read(name)
        assert "FastPin<DATA_PIN>" not in text, name
        assert "Mgm240HwPin<DATA_PIN>" in text, name


def test_forced_mode_uses_the_core_pin_api() -> None:
    # The Silabs core has no portOutputRegister; the register-based software
    # pin would use placeholder addresses, so forced mode must be NoPinMapPin.
    text = _read("led_sysdefs_arm_mgm240.h")
    assert re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\) && !defined\(FASTLED_NO_PINMAP\)\n#define FASTLED_NO_PINMAP\n#endif",
        text,
    )
