"""Checks the nRF52 variant pin table and the macros that expand it (#5070).

Two bugs lived here unnoticed because only hardware could show them:

* `_FL_DEFPIN` took the port number from `BOARD_PIN / 32` and ignored
  `BOARD_PORT`, so the short form `_FL_DEFPIN(2, 10, 1)` (P1.10) gave the SPIM
  and PWM drivers PSEL P0.10. With the port fixed, a wrong `BOARD_PIN` stops
  being masked, so every entry is checked against the `Pp.nn` its comment
  names (the Metro's D36 said P1.02 but encoded P1.00).
* Every macro specialized `fl::FastPin` unconditionally, so
  `FASTLED_FORCE_SOFTWARE_PINS` could never select the software template.

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
    assert "_FL_NRF52_FASTPIN" in forced_branch
    assert "fl::FastPin" not in forced_branch
    assert "fl::FastPin<ARDUINO_PIN>" in normal_branch
    for name in ("_FL_DEFPIN", "_DEFPIN_ARM_IDENTITY_P0", "_DEFPIN_ARM_IDENTITY_P1"):
        body = _macro(name)
        assert "fl::FastPin" not in body, name
        assert "_FL_NRF52_FASTPIN(ARDUINO_PIN)" in body, name
        assert "fl::Nrf52HwPin<ARDUINO_PIN>" in body, name


def test_invalid_pins_keep_their_warning_fastpin_in_both_modes() -> None:
    body = _macro("_FL_DEF_INVALID_PIN")
    assert "fl::_INVALID_ARMPIN" in body
    assert (
        "template<> class fl::FastPin<ARDUINO_PIN> : public fl::Nrf52HwPin<ARDUINO_PIN>"
        in body
    )
