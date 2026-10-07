"""Pin the per-chip pin sets AutoResearch pin discovery must never touch.

Discovery drives every probed pin OUTPUT LOW/HIGH and reads others with
INPUT_PULLUP. A pin is skipped if and only if doing that at runtime resets
or locks up the board, or kills the RPC link. Strapping pins are sampled at
reset only, so they must never appear in these sets.

Sources: Espressif chip datasheets in FastLED/datasheets
(espressif/soc/<chip>/datasheet.pdf), pin overview / IO MUX tables.
"""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FASTPIN = ROOT / "src" / "platforms" / "esp" / "32" / "core" / "fastpin_esp32.h"
REMOTE = ROOT / "examples" / "AutoResearch" / "AutoResearchRemote.cpp"

# Library mask: flash/PSRAM/flash-power (VDD_SPI) and native-USB pads.
# C5 GPIO19 / C6 GPIO27 (VDD_SPI) are skipped AutoResearch-side only; whether
# the library mask should list them is an open maintainer question.
EXPECTED_LIBRARY_MASK = {
    "ESP_32DEV": {6, 7, 8, 9, 10, 11, 20},
    "ESP_32C3": {11, 12, 13, 14, 15, 16, 17},
    "ESP_32S2": {27, 28, 29, 30, 31, 32},
    "ESP_32S3": {19, 20, 27, 28, 29, 30, 31, 32},
    "ESP_32C5": {13, 14, 15, 16, 17, 18, 20, 21, 22},
    "ESP_32C6": {12, 13, 24, 25, 26, 28, 29, 30},
    "ESP_32P4": {24, 25},
    "ESP_32H2": {15, 16, 17, 18, 19, 20, 21},
    "ESP_32C2": {11, 12, 13, 14, 15, 16, 17},
}

# (uart0_tx, uart0_rx, usb_dm, usb_dp, psram_cs, psram_clk); -1 = none.
EXPECTED_LINK_PINS = {
    "ESP_32DEV": (1, 3, -1, -1, 16, 17),
    "ESP_32C3": (21, 20, 18, 19, -1, -1),
    "ESP_32S2": (43, 44, 19, 20, 26, -1),
    "ESP_32S3": (43, 44, 19, 20, 26, -1),
    "ESP_32C5": (11, 12, 13, 14, -1, -1),
    "ESP_32C6": (16, 17, 12, 13, -1, -1),
    "ESP_32P4": (37, 38, 24, 25, -1, -1),
    "ESP_32H2": (24, 23, 26, 27, -1, -1),
    "ESP_32C2": (20, 19, -1, -1, -1, -1),
}

# Strapping pins ("Default Configuration of Strapping Pins" table in each
# datasheet): sampled at reset only, so never skipped for that reason.
STRAPPING = {
    "ESP_32DEV": {0, 2, 5, 12, 15},
    "ESP_32C3": {2, 8, 9},
    "ESP_32S2": {0, 45, 46},
    "ESP_32S3": {0, 3, 45, 46},
    "ESP_32C5": {2, 3, 7, 25, 26, 27, 28},
    "ESP_32C6": {4, 5, 8, 9, 15},
    "ESP_32P4": {34, 35, 36, 37, 38},
    "ESP_32H2": {8, 9, 25},
    "ESP_32C2": {8, 9},
}

_CHIP = re.compile(r"defined\(FL_IS_(ESP_32\w+)\)")


def _library_masks() -> dict[str, set[int]]:
    source = FASTPIN.read_text(encoding="utf-8")
    masks: dict[str, set[int]] = {}
    for cond, body in re.findall(
        r"#(?:el)?if ([^\n]*)\n(?:(?://[^\n]*|\s*)\n)*"
        r"#define FASTLED_UNUSABLE_PIN_MASK \(0ULL([^\n]*)\)",
        source,
    ):
        chip = _CHIP.search(cond)
        assert chip, cond
        masks[chip.group(1)] = {int(b) for b in re.findall(r"_FL_BIT\((\d+)\)", body)}
    return masks


def _link_pins() -> dict[str, tuple[int, ...]]:
    source = REMOTE.read_text(encoding="utf-8")
    table: dict[str, tuple[int, ...]] = {}
    for cond, body in re.findall(
        r"#(?:el)?if ([^\n]*)\n(?://[^\n]*\n)*"
        r"constexpr AutoResearchLinkPins kLinkPins = \{([^}]*)\};",
        source,
    ):
        chip = _CHIP.search(cond)
        assert chip, cond
        table[chip.group(1)] = tuple(int(v) for v in body.split(","))
    return table


def test_library_unusable_pin_mask_per_chip() -> None:
    assert _library_masks() == EXPECTED_LIBRARY_MASK


def test_autoresearch_link_pins_per_chip() -> None:
    assert _link_pins() == EXPECTED_LINK_PINS


def test_strapping_pins_are_never_skipped() -> None:
    masks = _library_masks()
    links = _link_pins()
    for chip, straps in STRAPPING.items():
        # P4 UART0 is GPIO37/38, which double as strapping pins; the console
        # use is what makes them unsafe, not the strap.
        console = {links[chip][0], links[chip][1]}
        assert not (masks[chip] & straps), chip
        assert not ((set(links[chip]) - console) & straps), chip


def test_discovery_applies_link_mask_and_console_mode() -> None:
    source = REMOTE.read_text(encoding="utf-8")
    assert (
        "#if !(defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT)" in source
    )
    # USB D-/D+ are masked unconditionally (deploy/reset link), UART0 only
    # when Serial is not native USB.
    console = source[source.index("fl::u64 autoResearchConsolePinMask()") :]
    console = console[: console.index("\n}\n")]
    usb = console.index("kLinkPins.usb_dm")
    assert usb < console.index("#if !(defined(ARDUINO_USB_CDC_ON_BOOT)")
    assert '"PICO"' in source and '"U4WDH"' in source
    assert "psramFound()" in source
    assert "CONFIG_SPIRAM_MODE_OCT" in source
    assert "CONFIG_ESPTOOLPY_OCT_FLASH" in source
    assert "unsafeReason(a)" in source
    assert "unsafeReason(b)" in source


def test_vdd_spi_pins_skipped_by_autoresearch() -> None:
    source = REMOTE.read_text(encoding="utf-8")
    memory = source[source.index("fl::u64 autoResearchMemoryPinMask()") :]
    memory = memory[: memory.index("\n}\n")]
    assert "defined(FL_IS_ESP_32C5)" in memory and "linkPinBit(19)" in memory
    assert "defined(FL_IS_ESP_32C6)" in memory and "linkPinBit(27)" in memory
    assert "SOC_GPIO_VALID_GPIO_MASK" in source
