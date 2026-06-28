"""Snapshot test: PARLIO-capable boards in ``ci/boards.py``.

The set of boards that get the PARLIO IRAM build flags (i.e.
``CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM=1`` /
``CONFIG_PARLIO_TX_ISR_CACHE_SAFE=1``) must equal the chip-family list that
FastLED's own driver supports — ``src/platforms/esp/32/drivers/parlio/bus_traits.h:6``
(C5/C6/H2/P4). Historically (#3276) this list was hand-maintained and
``esp32s3`` ended up in it incorrectly, even though S3 has no PARLIO
peripheral (``SOC_PARLIO_SUPPORTED`` is unset in its ``soc_caps.h``).

This test pins down both halves of that contract:

1.  The ``parlio_capable=True`` set on the Board dataclass equals exactly
    ``{esp32c5, esp32c6, esp32h2, esp32p4}``.
2.  The generated ``platformio.ini`` snippet for ``esp32s3`` does NOT contain
    ``CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM`` — guards against a regression of
    the #3276 bug.

See issue #3304 for the design rationale.
"""

import unittest

from ci import boards as boards_module
from ci.boards import ESP32_S3_DEVKITC_1, Board


# Canonical PARLIO-capable chip family — must stay in sync with
# src/platforms/esp/32/drivers/parlio/bus_traits.h:6.
EXPECTED_PARLIO_BOARDS = {"esp32c5", "esp32c6", "esp32h2", "esp32p4"}


def _all_boards() -> list[Board]:
    """Return every Board instance defined at module load time.

    ``ci.boards`` populates a module-level ``ALL`` list as a side effect of
    ``Board.__post_init__`` for any Board with ``add_board_to_all=True`` (the
    default). Importing the module is enough to populate it.
    """
    return list(boards_module.ALL)


class TestParlioCapableBoards(unittest.TestCase):
    """Pin the PARLIO-capable board set against the FastLED driver allowlist."""

    def test_parlio_capable_set_matches_driver_allowlist(self) -> None:
        parlio_boards = {b.board_name for b in _all_boards() if b.parlio_capable}
        self.assertEqual(
            parlio_boards,
            EXPECTED_PARLIO_BOARDS,
            "ci/boards.py's parlio_capable set drifted from the driver "
            "allowlist in src/platforms/esp/32/drivers/parlio/bus_traits.h:6. "
            f"Got {sorted(parlio_boards)}, expected {sorted(EXPECTED_PARLIO_BOARDS)}.",
        )

    def test_esp32s3_not_parlio_capable(self) -> None:
        # esp32s3 has no PARLIO peripheral. PR #3276 historically had this
        # wrong; do not regress.
        self.assertFalse(
            ESP32_S3_DEVKITC_1.parlio_capable,
            "esp32s3 has SOC_PARLIO_SUPPORTED unset in soc_caps.h — it must "
            "not be marked parlio_capable. See #3304.",
        )

    def test_esp32s3_platformio_ini_omits_parlio_flags(self) -> None:
        ini = ESP32_S3_DEVKITC_1.to_platformio_ini()
        self.assertNotIn(
            "CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM",
            ini,
            "esp32s3 platformio.ini contains a PARLIO IRAM flag — that's the "
            "#3276 bug. The flag must only be emitted on chips with "
            "parlio_capable=True (C5/C6/H2/P4).",
        )

    def test_each_parlio_board_emits_iram_flags(self) -> None:
        # Positive half of the contract: every parlio_capable board's
        # generated ini must carry the IRAM-pin flags so the ISR is not
        # placed in flash (which would cause cache-miss stalls and break
        # LED protocol timing).
        for board in _all_boards():
            if not board.parlio_capable:
                continue
            ini = board.to_platformio_ini()
            with self.subTest(board=board.board_name):
                self.assertIn(
                    "CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM=1",
                    ini,
                    f"{board.board_name} is parlio_capable but its "
                    "platformio.ini does not contain "
                    "CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM=1.",
                )
                self.assertIn(
                    "CONFIG_PARLIO_TX_ISR_CACHE_SAFE=1",
                    ini,
                    f"{board.board_name} is parlio_capable but its "
                    "platformio.ini does not contain "
                    "CONFIG_PARLIO_TX_ISR_CACHE_SAFE=1.",
                )


if __name__ == "__main__":
    unittest.main()
