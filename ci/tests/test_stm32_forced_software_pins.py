"""The STM32 clockless driver must reject FASTLED_FORCE_SOFTWARE_PINS (#5184).

In forced mode `fastpin_arm_stm32.h` skips the STM32 pin tables, so
`FastPin<PIN>` is the software template from `fl/system/fastpin_base.h`, whose
`port()` returns `nullptr`. The clockless driver writes the GPIO output
register through `FastPin<DATA_PIN>::port()`, so a forced build would fault at
the first `show()`. Both driver class templates (M0/M0+ and M3+) carry a
DATA_PIN-dependent `FL_STATIC_ASSERT`, so the build fails only when a sketch
instantiates the driver; SPI-only forced builds still compile.
"""

import re
from pathlib import Path


SRC = Path(__file__).resolve().parents[2] / "src" / "platforms"


def _read(rel: str) -> str:
    return (SRC / rel).read_text(encoding="utf-8")


def test_stm32_clockless_rejects_forced_software_pins() -> None:
    text = _read("arm/stm32/clockless_arm_stm32.h")
    message = (
        "FASTLED_FORCE_SOFTWARE_PINS is not supported by the STM32 clockless driver"
    )
    # One guard per driver class (M0/M0+ and Cortex-M3/M4/M7).
    assert text.count(message) == 2
    guards = re.findall(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\s*\n(?:\s*//[^\n]*\n)*\s*FL_STATIC_ASSERT\(DATA_PIN != DATA_PIN,\s*\n?\s*\"FASTLED_FORCE_SOFTWARE_PINS is not supported by the STM32 clockless driver",
        text,
    )
    assert len(guards) == 2


def test_no_pinmap_clockless_rejects_forced_software_pins() -> None:
    fastpin: str = (
        (SRC.parent / "fl/system/fastpin_base.h")
        .read_text(encoding="utf-8")
        .replace("\\\n", "")
    )
    # Derive the capability from the existing legacy selection branch, not
    # additional legacy-macro checks in each clockless driver.
    assert re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\s*&&\s*"
        r"\(defined\(FASTLED_NO_PINMAP\)\s*\|\|\s*defined\(FL_IS_ESP32\)\)\s*\n"
        r"#define FL_FASTPIN_HAS_NO_PINMAP\s*\n",
        fastpin,
    )
    assert fastpin.count("#define FL_FASTPIN_HAS_NO_PINMAP") == 1
    assert "template<fl::u8 PIN> class FastPin : public NoPinMapPin<PIN> {};" in fastpin
    for platform in ("d51", "renesas"):
        text = _read(f"arm/{platform}/clockless_arm_{platform}.h")
        assert re.search(
            r"#ifdef FL_FASTPIN_HAS_NO_PINMAP\s*\n\s*FL_STATIC_ASSERT\(DATA_PIN != DATA_PIN,",
            text,
        ), platform
        assert (
            "FASTLED_FORCE_SOFTWARE_PINS is not supported by this clockless driver"
            in text
        )


def test_premise_forced_stm32_pins_have_no_port() -> None:
    # If a hardware pin type for the clockless driver lands, this fails and the
    # guard can be replaced by real forced-software-pin support.
    fastpin = _read("arm/stm32/fastpin_arm_stm32.h")
    assert "#if defined(FASTLED_FORCE_SOFTWARE_PINS)" in fastpin
    assert '#include "fl/system/fastpin_base.h"' in fastpin
    assert "FastPin<DATA_PIN>::port()" in _read("arm/stm32/clockless_arm_stm32_impl.h")
