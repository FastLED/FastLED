"""nRF51 must honor FASTLED_FORCE_SOFTWARE_PINS (#5069).

`_FL_DEFPIN` in `platforms/arm/nrf51/fastpin_arm_nrf51.h` used to expand to an
explicit `fl::FastPin<PIN>` specialization, which overrides the forced software
primary template in `fastpin_base.h`. It now always emits the native
`Nrf51HwPin<PIN>` and adds the `FastPin` specialization only outside forced
mode. The clockless and SPI0 drivers use `Nrf51HwPin` directly.

#5188: setInput() updates PIN_CNF with IRQs masked, SPI0 rejects rates below
125 kHz at compile time, and the lint no longer maps board guards
(__RFduino__, __Simblee__) to the FL_IS_NRF51 family macro.

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


def test_pin_writes_never_read_modify_write_the_whole_port() -> None:
    # #5047: `NRF_GPIO->OUT ^= mask` (or |=, &=) writes the whole port back and
    # reverts any pin an interrupt changed in between; toggle() must go
    # through OUTSET/OUTCLR.
    text = _read("fastpin_arm_nrf51.h")
    assert not re.search(r"->\s*OUT\s*[\^|&]=", text)
    assert "OUTCLR = _MASK" in text and "OUTSET = _MASK" in text


def test_set_input_pin_cnf_update_masks_interrupts() -> None:
    # #5188: an ISR changing PIN_CNF between the read and the write must not
    # be undone by a stale write.
    body = re.search(
        r"static void setInput\(\)\s*\{(.*?)\n    \}",
        _read("fastpin_arm_nrf51.h"),
        re.S,
    )
    assert body, "setInput() not found"
    text = body.group(1)
    assert "PIN_CNF[PIN]" in text
    assert "__get_PRIMASK()" in text
    disable = text.index("__disable_irq()")
    restore = text.index("__set_PRIMASK(")
    assert disable < text.index("PIN_CNF[PIN]") < restore


def test_spi_rejects_rates_below_125khz() -> None:
    # #5188: rates below 125 kHz used to round up to 125 kHz silently.
    lines = _read("fastspi_arm_nrf51.h").splitlines()
    assert any("STATIC_ASSERT" in line and "F_CPU / 125000UL" in line for line in lines)


def test_lint_does_not_map_rfduino_board_guards_to_family() -> None:
    # #5188: FL_IS_NRF51 also covers micro:bit, so board guards stay allowed.
    path = (
        Path(__file__).resolve().parents[2]
        / "ci"
        / "lint_cpp_rs"
        / "src"
        / "lint_core"
        / "prelude_constants.rs"
    )
    text = path.read_text(encoding="utf-8")
    assert '("__RFduino__", "FL_IS_NRF51")' not in text
    assert '("__Simblee__", "FL_IS_NRF51")' not in text
