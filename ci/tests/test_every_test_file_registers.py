"""A test file that registers no tests is worse than no test file.

`FL_TEST_CASE` bodies compile whether or not anything runs them. What makes
them run is `#include "test.h"`, which pulls in the registration harness;
including `fl/test/fltest.h` directly compiles the same macros into a
translation unit that registers nothing. The suite then reports success and
the cases are never executed.

Three files had drifted that way (#4201). Two are fixed -- 78 cases came back,
one of which had been failing since it was written and nobody could tell.
This asserts the shape so it cannot happen again quietly.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
TESTS = PROJECT_ROOT / "tests"

# Defines the harness rather than using it, so it is the one file that
# legitimately includes `fltest.h` directly.
HARNESS_OWN_TEST = "fl/test/fltest.cpp"

# Empty, and meant to stay that way. `fl/channels/channel.cpp` was the last
# entry; its two crashes were fixed in #4239 and its four assertion failures
# in #4240, so every test file under `tests/` now registers its cases.
#
# The mechanism is kept as a ratchet at zero rather than deleted: the test
# below asserts the tuple is still empty, so re-dormanting a file cannot be
# waved through silently -- it takes a visible edit here that a reviewer sees.
KNOWN_DORMANT: tuple[str, ...] = ()

DEFINES_A_CASE = re.compile(r"^\s*FL_TEST_CASE\s*\(", re.M)
INCLUDES_HARNESS = re.compile(r'^\s*#\s*include\s+"test\.h"', re.M)


def collect_sources() -> list[Path]:
    return sorted(TESTS.rglob("*.cpp"))


class TestEveryTestFileRegisters(unittest.TestCase):
    def test_there_are_test_files_to_check(self: "TestEveryTestFileRegisters") -> None:
        # Without this the sweep below passes vacuously if the layout moves.
        self.assertGreater(len(collect_sources()), 100)

    def test_every_file_with_cases_includes_the_harness(
        self: "TestEveryTestFileRegisters",
    ) -> None:
        offenders: list[str] = []
        for path in collect_sources():
            relative = path.relative_to(TESTS).as_posix()
            if relative == HARNESS_OWN_TEST or relative in KNOWN_DORMANT:
                continue
            source = path.read_text(encoding="utf-8", errors="replace")
            if not DEFINES_A_CASE.search(source):
                continue
            if not INCLUDES_HARNESS.search(source):
                offenders.append(relative)
        self.assertEqual(
            offenders,
            [],
            msg=(
                'these files define FL_TEST_CASE but do not include "test.h", '
                "so their cases compile and never run: " + ", ".join(offenders)
            ),
        )

    def test_the_dormant_list_is_empty(
        self: "TestEveryTestFileRegisters",
    ) -> None:
        # A ratchet, not an allowance. Every test file registers its cases
        # today, so any new exemption has to be a visible edit here.
        self.assertEqual(KNOWN_DORMANT, ())

    def test_the_file_that_was_last_dormant_now_registers(
        self: "TestEveryTestFileRegisters",
    ) -> None:
        # Guards the specific regression this list existed for. Without it the
        # sweep above would still pass if channel.cpp were deleted outright.
        woken = TESTS / "fl/channels/channel.cpp"
        self.assertTrue(woken.is_file(), msg=f"{woken} is missing")
        source = woken.read_text(encoding="utf-8")
        self.assertIsNotNone(DEFINES_A_CASE.search(source))
        self.assertIsNotNone(INCLUDES_HARNESS.search(source))

    def test_the_check_can_actually_fail(
        self: "TestEveryTestFileRegisters",
    ) -> None:
        # It is two regexes over text; prove they match what they claim.
        self.assertIsNotNone(DEFINES_A_CASE.search('FL_TEST_CASE("x") {}'))
        self.assertIsNotNone(INCLUDES_HARNESS.search('#include "test.h"\n'))
        self.assertIsNone(INCLUDES_HARNESS.search('#include "fl/test/fltest.h"\n'))
        # A case named inside a comment or a string must not count as a
        # definition, or the guard would flag files that merely discuss it.
        self.assertIsNone(DEFINES_A_CASE.search("// see FL_TEST_CASE (above)"))


if __name__ == "__main__":
    unittest.main()
