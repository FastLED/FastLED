"""Parallel software SPI must take its GPIO registers from the target (#5062).

The header used to fall back to the ESP32-C3/C2 addresses on every non-host build,
which on a classic ESP32 are not GPIO registers at all. A host build cannot see this:
it selects simulation functions instead. So the selection is checked structurally
here, and the addresses themselves were checked by building against each real SDK.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


DIRECTORY = (
    Path(__file__).resolve().parent.parent.parent
    / "src"
    / "platforms"
    / "shared"
    / "spi_bitbang"
)
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
WIDTHS = (1, 2, 4, 8, 16, 32)


def _code(name: str) -> str:
    return COMMENT.sub(" ", (DIRECTORY / name).read_text(encoding="utf-8"))


class TestTargetSelection(unittest.TestCase):
    def test_no_hardcoded_register_address(self: "TestTargetSelection") -> None:
        self.assertEqual(re.findall(r"0x[0-9a-fA-F]{8}", _code("spi_platform.h")), [])

    def test_esp32_takes_addresses_from_the_sdk(self: "TestTargetSelection") -> None:
        code = _code("spi_platform.h")
        self.assertIn('#include "soc/gpio_reg.h"', code)
        self.assertRegex(code, r"FASTLED_GPIO_W1TS_ADDR\s+GPIO_OUT_W1TS_REG")
        self.assertRegex(code, r"FASTLED_GPIO_W1TC_ADDR\s+GPIO_OUT_W1TC_REG")

    def test_other_targets_have_no_addresses(self: "TestTargetSelection") -> None:
        self.assertRegex(
            _code("spi_platform.h"),
            r"#if defined\(FL_IS_ESP32\)\s+"
            r"#include \"soc/gpio_reg.h\"\s+"
            r"#define FASTLED_GPIO_W1TS_ADDR GPIO_OUT_W1TS_REG\s+"
            r"#define FASTLED_GPIO_W1TC_ADDR GPIO_OUT_W1TC_REG\s+"
            r"#else\s+#define FL_SPI_GPIO_AVAILABLE 0",
        )

    def test_the_isr_engine_is_not_built_without_gpio(
        self: "TestTargetSelection",
    ) -> None:
        self.assertRegex(
            _code("spi_isr_engine.cpp.hpp"),
            r"#if FL_SPI_GPIO_AVAILABLE\s+FL_EXTERN_C_BEGIN",
        )

    def test_every_block_rejects_a_target_with_no_gpio(
        self: "TestTargetSelection",
    ) -> None:
        for width in WIDTHS:
            with self.subTest(width):
                code = _code(f"spi_block_{width}.h")
                self.assertRegex(
                    code,
                    r"template <bool GPIO_AVAILABLE = FL_SPI_GPIO_AVAILABLE>\s+"
                    r"void transmit\(\) FL_NO_EXCEPT \{\s+"
                    r"FL_STATIC_ASSERT\(GPIO_AVAILABLE && FL_SPI_GPIO_AVAILABLE,",
                )


if __name__ == "__main__":
    unittest.main()
