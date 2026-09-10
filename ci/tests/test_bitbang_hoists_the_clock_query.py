"""The clockless bit loop must not re-derive the CPU frequency per delay (#4203).

`delayNanoseconds(ns)` looks up the CPU frequency on every call, and on ESP32
that is an ESP-IDF call rather than a constant -- three of them per bit in the
hottest loop this driver has. The maintainer's instruction on that issue is
the rule this encodes: *any expensive clock related calculations should be
done once before entering into repeated inner loops.*

Structural rather than timed, because the cost is per-platform and the
property is not. A microbenchmark on the host would measure a machine that
does not have the problem.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
DRIVER = (
    PROJECT_ROOT
    / "src"
    / "platforms"
    / "shared"
    / "bitbang"
    / "bitbang_channel_driver.cpp.hpp"
)

COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)

# `delayNanoseconds(x)` -- one argument, so the frequency is looked up inside.
# The two-argument form takes a frequency the caller already has.
ONE_ARG_DELAY = re.compile(r"\bdelayNanoseconds\s*\(\s*[^(),]+\s*\)")


def _bit_loop_body(text: str) -> str:
    start = text.index("void BitBangChannelDriver::transmitClocklessBit")
    brace = text.index("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[brace : index + 1]
    raise AssertionError("transmitClocklessBit body not found")


class TestTheClockQueryIsHoisted(unittest.TestCase):
    def setUp(self: "TestTheClockQueryIsHoisted") -> None:
        self.text = COMMENT.sub(" ", DRIVER.read_text(encoding="utf-8"))

    def test_the_bit_function_takes_a_frequency(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        self.assertIn("transmitClocklessBit", self.text)
        self.assertRegex(self.text, r"transmitClocklessBit\([^)]*u32 hz")

    def test_the_bit_loop_never_uses_the_one_argument_delay(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        body = _bit_loop_body(self.text)
        offenders = ONE_ARG_DELAY.findall(body)
        self.assertEqual(
            offenders,
            [],
            msg=(
                "one-argument delayNanoseconds in the per-bit path re-derives "
                "the CPU frequency each call: " + ", ".join(offenders)
            ),
        )

    def test_the_body_really_does_delay(self: "TestTheClockQueryIsHoisted") -> None:
        # Without this the check above passes on a function that stopped
        # delaying at all.
        body = _bit_loop_body(self.text)
        self.assertEqual(body.count("delayNanoseconds"), 3)

    def test_the_frequency_is_fetched_once_outside_the_loop(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        self.assertEqual(self.text.count("fl::cpuFrequencyHz()"), 1)

    def test_the_detector_would_catch_a_one_argument_call(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        # A positive control: the pattern has to match the shape it forbids,
        # or the sweep above passes for a detector that never fires.
        self.assertTrue(ONE_ARG_DELAY.search("fl::delayNanoseconds(t1_ns);"))
        self.assertFalse(ONE_ARG_DELAY.search("fl::delayNanoseconds(t1_ns, hz);"))


if __name__ == "__main__":
    unittest.main()
