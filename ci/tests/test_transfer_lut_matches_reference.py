"""The checked-in decode LUTs must stay equal to the P5 float64 reference.

`src/fl/gfx/transfer.cpp.hpp` carries two 256-entry u16 tables generated from
`ci/color_reference.py::decode_rgb8`. Nothing at build time re-derives them, so
without this test the embedded decode could drift away from the reference every
later phase is measured against, and the drift would show up as a ΔE budget
failure in P6-P8 rather than as the table edit that caused it.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

from ci.color_reference import SourceProfile, TransferFunction, decode_rgb8


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
TRANSFER_IMPL = PROJECT_ROOT / "src" / "fl" / "gfx" / "transfer.cpp.hpp"


def parse_table(name: str) -> list[int]:
    text = TRANSFER_IMPL.read_text(encoding="utf-8")
    match = re.search(rf"{name}\[256\][^=]*=\s*\{{(.*?)\}};", text, re.S)
    if match is None:
        raise AssertionError(f"{name} not found in {TRANSFER_IMPL}")
    return [int(v) for v in re.findall(r"-?\d+", match.group(1))]


def reference_table(transfer: TransferFunction) -> list[int]:
    profile = SourceProfile(SourceProfile.srgb_bt709().primaries, transfer)
    return [
        min(65535, max(0, round(decode_rgb8((code, 0, 0), profile)[0] * 65535.0)))
        for code in range(256)
    ]


class TestTransferLutMatchesReference(unittest.TestCase):
    def test_srgb_table_matches_reference(self) -> None:
        self.assertEqual(
            parse_table("SRGB_DECODE_LUT"), reference_table(TransferFunction.SRGB)
        )

    def test_bt709_table_matches_reference(self) -> None:
        self.assertEqual(
            parse_table("BT709_DECODE_LUT"), reference_table(TransferFunction.BT709)
        )

    def test_tables_are_full_length_and_hit_both_endpoints(self) -> None:
        for name in ("SRGB_DECODE_LUT", "BT709_DECODE_LUT"):
            table = parse_table(name)
            with self.subTest(table=name):
                self.assertEqual(len(table), 256)
                self.assertEqual(table[0], 0)
                self.assertEqual(table[255], 65535)

    def test_srgb_and_bt709_are_not_the_same_table(self) -> None:
        # A copy-paste of one table over the other would otherwise pass every
        # endpoint and monotonicity check in the C++ suite.
        self.assertNotEqual(
            parse_table("SRGB_DECODE_LUT"), parse_table("BT709_DECODE_LUT")
        )


if __name__ == "__main__":
    unittest.main()
