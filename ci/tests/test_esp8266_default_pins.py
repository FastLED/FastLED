"""ESP8266 example defaults must resolve to safe physical GPIOs."""

import re
from pathlib import Path

import pytest


HEADER = (
    Path(__file__).resolve().parents[2]
    / "src"
    / "platforms"
    / "esp"
    / "8266"
    / "fastpin_esp8266.h"
)

EXPECTED_GPIOS = {
    "FL_PIN_CLOCKLESS_1": 4,
    "FL_PIN_SPI_DATA_1": 13,
    "FL_PIN_SPI_CLOCK_1": 14,
}
UNSAFE_GPIOS = {0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 15}

_BRANCH = re.compile(
    r"^#(?:ifdef\s+|(?:if|elif) defined\()"
    r"(FASTLED_ESP8266_[A-Z0-9_]+_PIN_ORDER)\)?"
)
_PIN = re.compile(r"_FL_DEFPIN\(\s*(\d+)\s*,\s*(\d+)\s*\)")
_DEFAULT = re.compile(r"^#define (FL_PIN_(?:CLOCKLESS|SPI_DATA|SPI_CLOCK)_1) (\d+)$")


def _variants() -> dict[str, dict]:
    variants: dict[str, dict] = {}
    current: str | None = None
    for line in HEADER.read_text(encoding="utf-8").splitlines():
        match = _BRANCH.match(line)
        if match:
            current = match.group(1)
            variants[current] = {"pins": {}, "defaults": {}}
            continue
        if line.startswith("#else // if defined(FASTLED_ESP8266_NODEMCU_PIN_ORDER)"):
            current = "FASTLED_ESP8266_NODEMCU_PIN_ORDER"
            variants[current] = {"pins": {}, "defaults": {}}
            continue
        if current is None:
            continue
        for logical, gpio in _PIN.findall(line):
            variants[current]["pins"][int(logical)] = int(gpio)
        match = _DEFAULT.match(line)
        if match:
            variants[current]["defaults"][match.group(1)] = int(match.group(2))
    return variants


VARIANTS = _variants()


def test_parser_found_all_pin_orders() -> None:
    assert set(VARIANTS) == {
        "FASTLED_ESP8266_RAW_PIN_ORDER",
        "FASTLED_ESP8266_D1_PIN_ORDER",
        "FASTLED_ESP8266_NODEMCU_PIN_ORDER",
    }


@pytest.mark.parametrize("variant,block", VARIANTS.items())
@pytest.mark.parametrize("macro,expected_gpio", sorted(EXPECTED_GPIOS.items()))
def test_default_resolves_to_expected_safe_gpio(
    variant: str, block: dict, macro: str, expected_gpio: int
) -> None:
    assert set(block["defaults"]) == set(EXPECTED_GPIOS), (
        f"{variant}: all three platform defaults must be explicitly defined"
    )
    logical = block["defaults"][macro]
    assert logical in block["pins"], f"{variant}: {macro} uses absent pin {logical}"
    gpio = block["pins"][logical]
    assert gpio == expected_gpio, (
        f"{variant}: {macro} resolves through pin {logical} to GPIO {gpio}, "
        f"expected GPIO {expected_gpio}"
    )
    assert gpio not in UNSAFE_GPIOS, (
        f"{variant}: {macro} resolves through pin {logical} to unsafe GPIO {gpio}; "
        "avoid UART, boot-strapping, and flash pins"
    )
