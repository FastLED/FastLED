"""Checks the nRF52 variant pin table and the macros that expand it (#5070).

Two bugs lived here unnoticed because only hardware could show them:

* `_FL_DEFPIN` took the port number from `BOARD_PIN / 32` and ignored
  `BOARD_PORT`, so the short form `_FL_DEFPIN(2, 10, 1)` (P1.10) gave the SPIM
  and PWM drivers PSEL P0.10. With the port fixed, a wrong `BOARD_PIN` stops
  being masked, so every entry is checked against the `Pp.nn` its comment
  names (the Metro's D36 said P1.02 but encoded P1.00).
* Every macro specialized `fl::FastPin` unconditionally, so
  `FASTLED_FORCE_SOFTWARE_PINS` could never select the software template.
* In forced-software mode an invalid pin's `Nrf52HwPin` still resolves to
  P0.00, so the PWM and SPIM drivers must reject it at compile time and its
  `FastPin` must fall to the software template (#5190).

The table is parsed textually. Entries without a `Pp.nn` comment are not
checked; the test asserts that most entries have one, so the check cannot
silently become empty.
"""

import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
VARIANTS = (
    REPO_ROOT / "src" / "platforms" / "arm" / "nrf52" / "fastpin_arm_nrf52_variants.h"
)

_ENTRY = re.compile(
    r"^\s*_FL_DEF(?:PIN|PIN_NFC|PIN_USR_LED|_INVALID_PIN)\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)\s*;"
    r"\s*//.*?\bP([01])\.(\d+)"
)


def _text() -> str:
    return VARIANTS.read_text(encoding="utf-8")


def _macro(name: str) -> str:
    text = _text()
    match = re.search(r"#define " + name + r"\(.*?\n((?:.*\\\n)*.*)", text)
    assert match, name
    return match.group(1)


def test_every_entry_matches_its_commented_port_pin() -> None:
    bad = []
    checked = 0
    for number, line in enumerate(_text().splitlines(), 1):
        match = _ENTRY.match(line)
        if not match:
            continue
        checked += 1
        _arduino, board_pin, board_port, port, pin = map(int, match.groups())
        # Comments use either P1.04 or the combined number P1.36.
        want_pin = pin & 31 if pin >= 32 else pin
        if pin >= 32 and pin // 32 != port:
            bad.append((number, line.strip()))
        elif (board_port, board_pin & 31) != (port, want_pin):
            bad.append((number, line.strip()))
        elif board_pin >= 32 and board_pin // 32 != board_port:
            bad.append((number, line.strip()))
    assert checked > 400, checked
    assert not bad, bad


def test_defpin_takes_port_number_from_board_port() -> None:
    body = _macro("_FL_DEFPIN")
    assert "__generated_struct_NRF_P ## BOARD_PORT" in body
    args = re.search(r"fl::_ARMPIN<(.*?)>", body, re.S)
    assert args
    fields = [f.strip(" \\\n") for f in args.group(1).split(",")]
    assert fields[2] == "BOARD_PORT", fields


def test_valid_pins_specialize_fastpin_only_without_forced_software_pins() -> None:
    text = _text()
    forced = re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\n(.*?)#else\n(.*?)#endif",
        text,
        re.S,
    )
    assert forced
    forced_branch, normal_branch = forced.groups()
    valid = re.search(r"#define _FL_NRF52_FASTPIN\(ARDUINO_PIN\)(.*)", forced_branch)
    assert valid
    assert "fl::FastPin" not in valid.group(1)
    assert "fl::FastPin<ARDUINO_PIN>" in normal_branch
    for name in ("_FL_DEFPIN", "_DEFPIN_ARM_IDENTITY_P0", "_DEFPIN_ARM_IDENTITY_P1"):
        body = _macro(name)
        assert "fl::FastPin" not in body, name
        assert "_FL_NRF52_FASTPIN(ARDUINO_PIN)" in body, name
        assert "fl::Nrf52HwPin<ARDUINO_PIN>" in body, name


def _forced_branches() -> tuple[str, str]:
    forced = re.search(
        r"#if defined\(FASTLED_FORCE_SOFTWARE_PINS\)\n(.*?)#else\n(.*?)#endif",
        _text(),
        re.S,
    )
    assert forced
    return forced.group(1), forced.group(2)


def test_invalid_pin_macro_defers_fastpin_to_mode_macro() -> None:
    body = _macro("_FL_DEF_INVALID_PIN")
    assert "fl::_INVALID_ARMPIN" in body
    assert "_FL_NRF52_INVALID_FASTPIN(ARDUINO_PIN)" in body
    assert "fl::FastPin" not in body


def test_invalid_pins_specialize_fastpin_only_without_forced_software_pins() -> None:
    forced_branch, normal_branch = _forced_branches()
    assert "#define _FL_NRF52_INVALID_FASTPIN(ARDUINO_PIN)" in forced_branch
    assert "fl::Nrf52HwPin" not in forced_branch
    normal = re.search(
        r"#define _FL_NRF52_INVALID_FASTPIN\(ARDUINO_PIN\)((?:.*\\\n)*.*)",
        normal_branch,
    )
    assert normal
    assert (
        "fl::FastPin<ARDUINO_PIN> : public fl::Nrf52HwPin<ARDUINO_PIN>"
        in normal.group(1)
    )


def test_drivers_reject_invalid_pins_in_every_mode() -> None:
    nrf52 = REPO_ROOT / "src" / "platforms" / "arm" / "nrf52"
    clockless = (nrf52 / "clockless_arm_nrf52.h").read_text(encoding="utf-8")
    spi = (nrf52 / "fastspi_arm_nrf52.h").read_text(encoding="utf-8")
    assert "FL_STATIC_ASSERT(nrf52_pin_routable<_DATA_PIN>()" in clockless
    assert "FL_STATIC_ASSERT(nrf52_pin_routable<_DATA_PIN>()" in spi
    assert "FL_STATIC_ASSERT(nrf52_pin_routable<_CLOCK_PIN>()" in spi
    for text in (clockless, spi):
        assert "FASTLED_FORCE_SOFTWARE_PINS" not in text
    fastpin = (nrf52 / "fastpin_arm_nrf52.h").read_text(encoding="utf-8")
    assert "constexpr bool nrf52_pin_routable()" in fastpin


def test_forced_mode_leaves_invalid_fastpin_incomplete() -> None:
    forced_branch, _ = _forced_branches()
    line = re.search(
        r"#define _FL_NRF52_INVALID_FASTPIN\(ARDUINO_PIN\)(.*)", forced_branch
    )
    assert line
    assert line.group(1).strip() == "template<> class fl::FastPin<ARDUINO_PIN>"
