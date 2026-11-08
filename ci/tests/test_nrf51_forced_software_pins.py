"""nRF51 must honor FASTLED_FORCE_SOFTWARE_PINS (#5069).

`_FL_DEFPIN` in `platforms/arm/nrf51/fastpin_arm_nrf51.h` used to expand to an
explicit `fl::FastPin<PIN>` specialization, which overrides the forced software
primary template in `fastpin_base.h`. It now always emits the native
`Nrf51HwPin<PIN>` and adds the `FastPin` specialization only outside forced
mode. The clockless and SPI0 drivers use `Nrf51HwPin` directly.

The headers are parsed textually; real nRF51 builds cover selection.
"""

import re
from pathlib import Path


NRF51 = Path(__file__).resolve().parents[2] / "src" / "platforms" / "arm" / "nrf51"
DRIVERS = ("clockless_arm_nrf51.h", "fastspi_arm_nrf51.h")


def _read(name: str) -> str:
    return (NRF51 / name).read_text(encoding="utf-8")


def test_forced_mode_adds_no_fastpin_specialization() -> None:
    match = re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\n(.*?)#else\n(.*?)#endif",
        _read("fastpin_arm_nrf51.h"),
        re.S,
    )
    assert match, "force guard around _FL_NRF51_FASTPIN not found"
    forced, normal = match.group(1), match.group(2)
    assert "#define _FL_NRF51_FASTPIN(PIN)" in forced
    assert "FastPin<PIN>" not in forced
    assert "template<> class FastPin<PIN> : public Nrf51HwPin<PIN>" in normal


def test_defpin_emits_native_pin_and_conditional_fastpin() -> None:
    text = _read("fastpin_arm_nrf51.h")
    assert text.count("#define _FL_DEFPIN(") == 1
    body = re.search(r"#define _FL_DEFPIN\(.*?\n((?:.*\\\n)*.*)", text)
    assert body
    assert "class Nrf51HwPin<PIN>" in body.group(1)
    assert "_FL_NRF51_FASTPIN(PIN)" in body.group(1)
    assert "FastPin" not in body.group(1)


def test_drivers_use_the_native_pin() -> None:
    for name in DRIVERS:
        text = _read(name)
        assert not re.search(r"\bFastPin(BB)?<", text), name
        assert "Nrf51HwPin<" in text, name
