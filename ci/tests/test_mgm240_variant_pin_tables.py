"""MGM240 variant pin tables must cover every gPinNames entry (#5182).

The Silicon Labs Arduino core 2.2.0 variant files define 19 pins for the
Ezurio Lyra 24P20 (index 18 = PA7) and 28 pins for the Seeed XIAO MG24
(index 27 = PB5). `fastpin_arm_mgm240.h` used to stop one pin short on both,
so `FastPin<18>` (Lyra) and `FastPin<27>` (XIAO) had no hardware pin.

The header is parsed textually; real-SDK builds cover selection.
"""

import re
from pathlib import Path


MGM240 = Path(__file__).resolve().parents[2] / "src" / "platforms" / "arm" / "mgm240"
HEADER = MGM240 / "fastpin_arm_mgm240.h"
DEFPIN = re.compile(r"_FL_DEFPIN\(\s*(\d+),\s*(\d+),\s*([A-D]),\s*\(1 << (\d+)\)\)")


def _variant_block(define: str) -> str:
    lines = HEADER.read_text(encoding="utf-8").splitlines()
    start = next(
        i for i, line in enumerate(lines) if line.startswith("#elif") and define in line
    )
    body: list[str] = []
    for line in lines[start + 1 :]:
        if line.startswith("#elif") or line.startswith("#else"):
            break
        body.append(line)
    return "\n".join(body)


def _parse(define: str) -> dict[int, tuple[int, str, int]]:
    pins: dict[int, tuple[int, str, int]] = {}
    for m in DEFPIN.finditer(_variant_block(define)):
        pins[int(m.group(1))] = (int(m.group(2)), m.group(3), int(m.group(4)))
    return pins


def _bit_port(define: str) -> dict[int, tuple[int, str]]:
    return {pin: (bit, port) for pin, (bit, port, _) in _parse(define).items()}


def test_lyra24p20_table_covers_pin_18() -> None:
    pins = _bit_port("ARDUINO_LYRA24P20")
    assert sorted(pins) == list(range(19))
    assert pins[18] == (7, "A")


def test_xiao_mg24_table_covers_pin_27() -> None:
    pins = _bit_port("ARDUINO_XIAO_MG24")
    assert sorted(pins) == list(range(28))
    assert pins[27] == (5, "B")


def test_bit_matches_mask_shift() -> None:
    for define in ("ARDUINO_LYRA24P20", "ARDUINO_XIAO_MG24"):
        for pin, (bit, _, shift) in _parse(define).items():
            assert bit == shift, f"{define} pin {pin}: bit {bit} != mask {shift}"
