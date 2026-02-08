#!/usr/bin/env python3
"""Checker to ensure test files use FL_ prefixed macros and include test.h."""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


TESTS_ROOT = PROJECT_ROOT / "tests"

# Files that legitimately need direct doctest.h inclusion
EXEMPT_FILES = {
    "test.h",  # The wrapper header itself
    "doctest.h",  # The doctest header itself
    "doctest_main.cpp",  # Test runner entry point (needs DOCTEST_CONFIG_IMPLEMENT)
}

# Banned bare doctest macros -> FL_ equivalents
BANNED_MACROS: dict[str, str] = {
    # Test structure
    "TEST_CASE": "FL_TEST_CASE",
    "SUBCASE": "FL_SUBCASE",
    "TEST_CASE_TEMPLATE": "FL_TEST_CASE_TEMPLATE",
    "TEST_CASE_FIXTURE": "FL_TEST_CASE_FIXTURE",
    "TEST_SUITE": "FL_TEST_SUITE",
    "TYPE_TO_STRING": "FL_TYPE_TO_STRING",
    # Logging / info
    "MESSAGE": "FL_MESSAGE",
    "INFO": "FL_DINFO",
    "CAPTURE": "FL_CAPTURE",
    # Explicit failure
    "FAIL": "FL_FAIL",
    "FAIL_CHECK": "FL_FAIL_CHECK",
    # CHECK family - basic
    "CHECK": "FL_CHECK",
    "CHECK_FALSE": "FL_CHECK_FALSE",
    "CHECK_UNARY": "FL_CHECK_UNARY",
    "CHECK_UNARY_FALSE": "FL_CHECK_UNARY_FALSE",
    # CHECK family - comparison
    "CHECK_EQ": "FL_CHECK_EQ",
    "CHECK_NE": "FL_CHECK_NE",
    "CHECK_GT": "FL_CHECK_GT",
    "CHECK_GE": "FL_CHECK_GE",
    "CHECK_LT": "FL_CHECK_LT",
    "CHECK_LE": "FL_CHECK_LE",
    # CHECK family - floating point
    "CHECK_CLOSE": "FL_CHECK_CLOSE",
    "CHECK_DOUBLE_EQ": "FL_CHECK_DOUBLE_EQ",
    "CHECK_APPROX": "FL_CHECK_APPROX",
    # CHECK family - string
    "CHECK_STREQ": "FL_CHECK_STREQ",
    # CHECK family - message
    "CHECK_MESSAGE": "FL_CHECK_MESSAGE",
    "CHECK_FALSE_MESSAGE": "FL_CHECK_FALSE_MESSAGE",
    # CHECK family - exception
    "CHECK_THROWS": "FL_CHECK_THROWS",
    "CHECK_THROWS_AS": "FL_CHECK_THROWS_AS",
    "CHECK_THROWS_WITH": "FL_CHECK_THROWS_WITH",
    "CHECK_NOTHROW": "FL_CHECK_NOTHROW",
    # CHECK family - other
    "CHECK_TRAIT": "FL_CHECK_TRAIT",
    # REQUIRE family - basic
    "REQUIRE": "FL_REQUIRE",
    "REQUIRE_FALSE": "FL_REQUIRE_FALSE",
    "REQUIRE_UNARY": "FL_REQUIRE_UNARY",
    "REQUIRE_UNARY_FALSE": "FL_REQUIRE_UNARY_FALSE",
    # REQUIRE family - comparison
    "REQUIRE_EQ": "FL_REQUIRE_EQ",
    "REQUIRE_NE": "FL_REQUIRE_NE",
    "REQUIRE_GT": "FL_REQUIRE_GT",
    "REQUIRE_GE": "FL_REQUIRE_GE",
    "REQUIRE_LT": "FL_REQUIRE_LT",
    "REQUIRE_LE": "FL_REQUIRE_LE",
    # REQUIRE family - floating point
    "REQUIRE_CLOSE": "FL_REQUIRE_CLOSE",
    "REQUIRE_APPROX": "FL_REQUIRE_APPROX",
    # REQUIRE family - message
    "REQUIRE_MESSAGE": "FL_REQUIRE_MESSAGE",
    "REQUIRE_FALSE_MESSAGE": "FL_REQUIRE_FALSE_MESSAGE",
    # REQUIRE family - exception
    "REQUIRE_THROWS": "FL_REQUIRE_THROWS",
    "REQUIRE_THROWS_AS": "FL_REQUIRE_THROWS_AS",
    "REQUIRE_THROWS_WITH": "FL_REQUIRE_THROWS_WITH",
    "REQUIRE_THROWS_WITH_MESSAGE": "FL_REQUIRE_THROWS_WITH_MESSAGE",
    "REQUIRE_NOTHROW": "FL_REQUIRE_NOTHROW",
    # WARN family - basic
    "WARN": "FL_DWARN",
    "WARN_FALSE": "FL_DWARN_FALSE",
    "WARN_UNARY": "FL_WARN_UNARY",
    "WARN_UNARY_FALSE": "FL_WARN_UNARY_FALSE",
    # WARN family - comparison
    "WARN_EQ": "FL_WARN_EQ",
    "WARN_NE": "FL_WARN_NE",
    "WARN_GT": "FL_WARN_GT",
    "WARN_GE": "FL_WARN_GE",
    "WARN_LT": "FL_WARN_LT",
    "WARN_LE": "FL_WARN_LE",
    # WARN family - message
    "WARN_MESSAGE": "FL_WARN_MESSAGE",
    "WARN_FALSE_MESSAGE": "FL_WARN_FALSE_MESSAGE",
    # WARN family - exception
    "WARN_THROWS": "FL_WARN_THROWS",
    "WARN_THROWS_AS": "FL_WARN_THROWS_AS",
    "WARN_THROWS_WITH": "FL_WARN_THROWS_WITH",
    "WARN_NOTHROW": "FL_WARN_NOTHROW",
    # BDD-style macros
    "SCENARIO": "FL_SCENARIO",
    "GIVEN": "FL_GIVEN",
    "WHEN": "FL_WHEN",
    "AND_WHEN": "FL_AND_WHEN",
    "THEN": "FL_THEN",
    "AND_THEN": "FL_AND_THEN",
}

# Pre-compile regex patterns for each banned macro.
# Sort by length descending so longer macros match first (e.g. CHECK_EQ before CHECK).
# Pattern uses \b word boundary (so FL_CHECK won't match \bCHECK) and
# lookahead for '(' (so CHECK_EQ won't match \bCHECK\s*\().
_BANNED_PATTERNS: list[tuple[re.Pattern[str], str, str]] = []
for _bare, _fl_ver in sorted(BANNED_MACROS.items(), key=lambda x: -len(x[0])):
    _BANNED_PATTERNS.append(
        (
            re.compile(r"\b" + re.escape(_bare) + r"(?=\s*\()"),
            _bare,
            _fl_ver,
        )
    )


class UnitTestChecker(FileContentChecker):
    """Checker that flags:
    1. Direct #include "doctest.h" (should use "test.h")
    2. Bare doctest macros (should use FL_ prefixed versions)
    """

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.startswith(str(TESTS_ROOT)):
            return False
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False
        from pathlib import Path

        if Path(file_path).name in EXEMPT_FILES:
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str]] = []

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Skip line comments
            if stripped.startswith("//"):
                continue

            # Check 1: direct doctest.h include
            if '#include "doctest.h"' in stripped or "#include <doctest.h>" in stripped:
                violations.append(
                    (
                        line_number,
                        'Use #include "test.h" instead of #include "doctest.h"',
                    )
                )

            # Check 2: bare doctest macros
            for pattern, bare, fl_ver in _BANNED_PATTERNS:
                if pattern.search(line):
                    violations.append(
                        (line_number, f"Use {fl_ver}() instead of bare {bare}()")
                    )

        if violations:
            self.violations[file_content.path] = violations
        return []


def main() -> None:
    from ci.util.check_files import run_checker_standalone

    checker = UnitTestChecker()
    run_checker_standalone(
        checker,
        [str(TESTS_ROOT)],
        "Found bare doctest macros or direct doctest.h includes",
    )


if __name__ == "__main__":
    main()
