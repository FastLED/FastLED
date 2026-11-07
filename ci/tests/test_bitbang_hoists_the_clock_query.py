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

# Comments and string or char literals, whose brackets are not code. One pattern,
# so that a `//` inside a string is not taken for a comment.
NOISE = re.compile(
    r"""//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'""", re.S
)

# Any spelling of the query: qualified, unqualified, or the macro.
FETCH = re.compile(r"\b(?:cpuFrequencyHz|FL_CPU_FREQUENCY)\s*\(\s*\)")

DELAY_NAME = re.compile(r"\b(delayNanoseconds|delayNs)\s*(?:<[^<>]*>\s*)?\(")


def _one_arg_delays(text: str) -> list[str]:
    """Delay calls with one argument, so the frequency is looked up inside.

    The two-argument form takes a frequency the caller already has. Parentheses
    are balanced so a cast in the argument does not hide the call.
    """

    found = []
    for match in DELAY_NAME.finditer(text):
        depth = 1
        commas = 0
        index = match.end()
        while depth:
            char = text[index]
            depth += {"(": 1, ")": -1}.get(char, 0)
            commas += char == "," and depth == 1
            index += 1
        if commas == 0:
            found.append(text[match.start() : index])
    return found


# The functions that transmit. Each owns one fetch, once per transmission.
TRANSMITTERS = (
    "void BitBangChannelDriver::transmitClockless(",
    "void BitBangChannelDriver::transmitSpi(",
)


def _function_body(text: str, signature: str) -> str:
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[brace : index + 1]
    raise AssertionError(f"{signature} body not found")


def _bit_loop_body(text: str) -> str:
    return _function_body(text, "void BitBangChannelDriver::transmitClocklessBit")


class TestTheClockQueryIsHoisted(unittest.TestCase):
    def setUp(self: "TestTheClockQueryIsHoisted") -> None:
        self.text = NOISE.sub(" ", DRIVER.read_text(encoding="utf-8"))

    def test_the_bit_function_takes_a_frequency(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        self.assertIn("transmitClocklessBit", self.text)
        self.assertRegex(self.text, r"transmitClocklessBit\([^)]*u32 hz")

    def test_the_bit_loop_never_uses_the_one_argument_delay(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        body = _bit_loop_body(self.text)
        offenders = _one_arg_delays(body)
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

    def test_each_transmitter_fetches_the_frequency_once(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        for signature in TRANSMITTERS:
            with self.subTest(signature):
                body = _function_body(self.text, signature)
                self.assertEqual(len(FETCH.findall(body)), 1)

    def test_nothing_else_fetches_the_frequency(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        self.assertEqual(len(FETCH.findall(self.text)), len(TRANSMITTERS))

    def test_the_fetch_is_outside_every_loop(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        """Once per transmission, not once per timing group.

        The first version of this put the fetch inside the loop over timing
        groups. A strip with two timing configurations is still one
        transmission, so that reintroduced an ESP-IDF call on a path meant not
        to have one -- while the comment above it claimed otherwise.

        Depth 1 is the function's own block, so no loop can enclose the fetch,
        while a finished loop before it is fine.
        """

        for signature in TRANSMITTERS:
            with self.subTest(signature):
                body = _function_body(self.text, signature)
                preceding = body[: FETCH.search(body).start()]
                depth = preceding.count("{") - preceding.count("}")
                self.assertEqual(depth, 1, msg="the fetch is in a nested block")

    def test_spi_delays_twice_per_bit_with_the_frequency(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        body = _function_body(self.text, "void BitBangChannelDriver::transmitSpi(")
        self.assertEqual(body.count("delayNanoseconds"), 2)
        self.assertEqual(_one_arg_delays(body), [])

    def test_the_detector_would_catch_a_one_argument_call(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        # A positive control: the pattern has to match the shape it forbids,
        # or the sweep above passes for a detector that never fires.
        self.assertTrue(_one_arg_delays("fl::delayNanoseconds(t1_ns);"))
        self.assertTrue(_one_arg_delays("fl::delayNanoseconds(static_cast<u32>(t));"))
        self.assertTrue(_one_arg_delays("fl::delayNs(t1_ns);"))
        self.assertFalse(_one_arg_delays("fl::delayNanoseconds(t1_ns, hz);"))
        self.assertFalse(_one_arg_delays("fl::delayNanoseconds(f(a, b), hz);"))

    def test_literals_cannot_confuse_the_call_scanner(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        call = 'fl::delayNanoseconds(sizeof("(") + t, hz); // )'
        self.assertFalse(_one_arg_delays(NOISE.sub(" ", call)))
        self.assertEqual(NOISE.sub(" ", '"//" x'), "  x")

    def test_the_fetch_detector_sees_every_spelling(
        self: "TestTheClockQueryIsHoisted",
    ) -> None:
        for spelling in (
            "fl::cpuFrequencyHz()",
            "cpuFrequencyHz()",
            "FL_CPU_FREQUENCY()",
        ):
            self.assertTrue(FETCH.search(spelling), msg=spelling)


if __name__ == "__main__":
    unittest.main()
