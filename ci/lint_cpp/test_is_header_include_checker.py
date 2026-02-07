#!/usr/bin/env python3
"""Tests for is_header_include_checker.py"""

import unittest

from ci.lint_cpp.is_header_include_checker import (
    FL_IS_PREFIX_TO_HEADER,
    PLATFORMS_ROOT,
    IsHeaderIncludeChecker,
    _extract_fl_is_macros_on_preprocessor_lines,
    _extract_included_filenames,
    _get_required_header,
)
from ci.util.check_files import FileContent


def _make_file_content(
    content: str, path: str = "src/platforms/avr/test.h"
) -> FileContent:
    return FileContent(path=path, content=content, lines=content.splitlines())


def _check(
    content: str, path: str = "src/platforms/avr/test.h"
) -> list[tuple[int, str]]:
    checker = IsHeaderIncludeChecker()
    checker.check_file_content(_make_file_content(content, path))
    return checker.violations.get(path, [])


class TestGetRequiredHeader(unittest.TestCase):
    def test_avr_general(self):
        self.assertEqual(_get_required_header("FL_IS_AVR"), "is_avr.h")

    def test_avr_subfamily(self):
        self.assertEqual(_get_required_header("FL_IS_AVR_ATMEGA_328P"), "is_avr.h")

    def test_avr_attiny_modern(self):
        self.assertEqual(_get_required_header("FL_IS_AVR_ATTINY_MODERN"), "is_avr.h")

    def test_esp32(self):
        self.assertEqual(_get_required_header("FL_IS_ESP32"), "is_esp.h")

    def test_esp_32s3(self):
        self.assertEqual(_get_required_header("FL_IS_ESP_32S3"), "is_esp.h")

    def test_stm32(self):
        self.assertEqual(_get_required_header("FL_IS_STM32"), "is_stm32.h")

    def test_teensy_4x(self):
        self.assertEqual(_get_required_header("FL_IS_TEENSY_4X"), "is_teensy.h")

    def test_samd21(self):
        self.assertEqual(_get_required_header("FL_IS_SAMD21"), "is_samd.h")

    def test_sam_not_samd(self):
        # FL_IS_SAM (not FL_IS_SAMD) should match is_sam.h
        self.assertEqual(_get_required_header("FL_IS_SAM"), "is_sam.h")

    def test_nrf52(self):
        self.assertEqual(_get_required_header("FL_IS_NRF52840"), "is_nrf52.h")

    def test_rp(self):
        self.assertEqual(_get_required_header("FL_IS_RP2040"), "is_rp.h")

    def test_wasm(self):
        self.assertEqual(_get_required_header("FL_IS_WASM"), "is_wasm.h")

    def test_unknown(self):
        self.assertIsNone(_get_required_header("FL_IS_UNKNOWN_PLATFORM_XYZ"))

    def test_all_prefixes_have_headers(self):
        for prefix, header in FL_IS_PREFIX_TO_HEADER.items():
            self.assertTrue(
                header.startswith("is_"), f"Header for {prefix} should start with is_"
            )
            self.assertTrue(
                header.endswith(".h"), f"Header for {prefix} should end with .h"
            )


class TestExtractFlIsMacros(unittest.TestCase):
    def test_ifdef(self):
        lines = ["#ifdef FL_IS_AVR"]
        self.assertEqual(
            _extract_fl_is_macros_on_preprocessor_lines(lines), {"FL_IS_AVR"}
        )

    def test_ifndef(self):
        lines = ["#ifndef FL_IS_ESP32"]
        self.assertEqual(
            _extract_fl_is_macros_on_preprocessor_lines(lines), {"FL_IS_ESP32"}
        )

    def test_if_defined(self):
        lines = ["#if defined(FL_IS_AVR_ATTINY_MODERN)"]
        self.assertEqual(
            _extract_fl_is_macros_on_preprocessor_lines(lines),
            {"FL_IS_AVR_ATTINY_MODERN"},
        )

    def test_elif(self):
        lines = ["#elif defined(FL_IS_STM32)"]
        self.assertEqual(
            _extract_fl_is_macros_on_preprocessor_lines(lines), {"FL_IS_STM32"}
        )

    def test_multiple_macros_same_line(self):
        lines = ["#if defined(FL_IS_AVR) || defined(FL_IS_ESP32)"]
        self.assertEqual(
            _extract_fl_is_macros_on_preprocessor_lines(lines),
            {"FL_IS_AVR", "FL_IS_ESP32"},
        )

    def test_non_preprocessor_line_ignored(self):
        lines = ["int x = FL_IS_AVR;"]
        self.assertEqual(_extract_fl_is_macros_on_preprocessor_lines(lines), set())

    def test_comment_line_ignored(self):
        lines = ["// #ifdef FL_IS_AVR"]
        self.assertEqual(_extract_fl_is_macros_on_preprocessor_lines(lines), set())

    def test_block_comment_ignored(self):
        lines = ["/* #ifdef FL_IS_AVR */"]
        self.assertEqual(_extract_fl_is_macros_on_preprocessor_lines(lines), set())

    def test_define_not_conditional(self):
        lines = ["#define FL_IS_AVR"]
        self.assertEqual(_extract_fl_is_macros_on_preprocessor_lines(lines), set())

    def test_else_not_conditional(self):
        lines = ["#else  // FL_IS_AVR"]
        self.assertEqual(_extract_fl_is_macros_on_preprocessor_lines(lines), set())

    def test_endif_not_conditional(self):
        lines = ["#endif  // FL_IS_AVR"]
        self.assertEqual(_extract_fl_is_macros_on_preprocessor_lines(lines), set())


class TestExtractIncludes(unittest.TestCase):
    def test_quoted_include(self):
        lines = ['#include "is_avr.h"']
        self.assertIn("is_avr.h", _extract_included_filenames(lines))

    def test_angle_include(self):
        lines = ["#include <is_avr.h>"]
        self.assertIn("is_avr.h", _extract_included_filenames(lines))

    def test_path_include(self):
        lines = ['#include "platforms/avr/is_avr.h"']
        self.assertIn("is_avr.h", _extract_included_filenames(lines))

    def test_relative_path_include(self):
        lines = ['#include "../is_avr.h"']
        self.assertIn("is_avr.h", _extract_included_filenames(lines))

    def test_is_platform(self):
        lines = ['#include "platforms/is_platform.h"']
        self.assertIn("is_platform.h", _extract_included_filenames(lines))

    def test_multiple_includes(self):
        lines = ['#include "is_avr.h"', '#include "is_arm.h"']
        names = _extract_included_filenames(lines)
        self.assertIn("is_avr.h", names)
        self.assertIn("is_arm.h", names)


class TestNoViolation(unittest.TestCase):
    """Files that correctly include the required header."""

    def test_has_is_avr(self):
        content = '#include "is_avr.h"\n#ifdef FL_IS_AVR\nint x;\n#endif'
        self.assertEqual(_check(content), [])

    def test_has_is_avr_with_path(self):
        content = '#include "platforms/avr/is_avr.h"\n#ifdef FL_IS_AVR\nint x;\n#endif'
        self.assertEqual(_check(content), [])

    def test_has_is_platform(self):
        content = '#include "platforms/is_platform.h"\n#ifdef FL_IS_AVR\nint x;\n#endif'
        self.assertEqual(_check(content), [])

    def test_is_platform_satisfies_all(self):
        content = (
            '#include "platforms/is_platform.h"\n'
            "#if defined(FL_IS_AVR) || defined(FL_IS_ESP32)\n"
            "int x;\n#endif"
        )
        self.assertEqual(_check(content), [])

    def test_no_fl_is_macros(self):
        content = "#ifdef __AVR__\nint x;\n#endif"
        self.assertEqual(_check(content), [])

    def test_empty_file(self):
        self.assertEqual(_check(""), [])

    def test_fl_is_in_non_preprocessor(self):
        content = "int FL_IS_AVR = 1;"
        self.assertEqual(_check(content), [])


class TestViolation(unittest.TestCase):
    """Files that use FL_IS_* without the required include."""

    def test_missing_is_avr(self):
        content = "#ifdef FL_IS_AVR\nint x;\n#endif"
        violations = _check(content)
        self.assertEqual(len(violations), 1)
        self.assertIn("is_avr.h", violations[0][1])

    def test_missing_is_esp(self):
        content = "#ifdef FL_IS_ESP32\nint x;\n#endif"
        violations = _check(content)
        self.assertEqual(len(violations), 1)
        self.assertIn("is_esp.h", violations[0][1])

    def test_missing_multiple_headers(self):
        content = "#if defined(FL_IS_AVR) || defined(FL_IS_STM32)\nint x;\n#endif"
        violations = _check(content)
        self.assertEqual(len(violations), 2)
        messages = {v[1] for v in violations}
        self.assertTrue(any("is_avr.h" in m for m in messages))
        self.assertTrue(any("is_stm32.h" in m for m in messages))

    def test_has_one_missing_another(self):
        content = '#include "is_avr.h"\n#if defined(FL_IS_AVR) || defined(FL_IS_ESP32)\nint x;\n#endif'
        violations = _check(content)
        self.assertEqual(len(violations), 1)
        self.assertIn("is_esp.h", violations[0][1])

    def test_wrong_header_included(self):
        content = '#include "is_esp.h"\n#ifdef FL_IS_AVR\nint x;\n#endif'
        violations = _check(content)
        self.assertEqual(len(violations), 1)
        self.assertIn("is_avr.h", violations[0][1])

    def test_avr_subfamilies_need_is_avr(self):
        content = "#ifdef FL_IS_AVR_ATTINY_MODERN\nint x;\n#endif"
        violations = _check(content)
        self.assertEqual(len(violations), 1)
        self.assertIn("is_avr.h", violations[0][1])


class TestShouldProcessFile(unittest.TestCase):
    def test_is_header_excluded(self):
        checker = IsHeaderIncludeChecker()
        self.assertFalse(
            checker.should_process_file(str(PLATFORMS_ROOT / "avr" / "is_avr.h"))
        )

    def test_is_platform_excluded(self):
        checker = IsHeaderIncludeChecker()
        self.assertFalse(
            checker.should_process_file(str(PLATFORMS_ROOT / "is_platform.h"))
        )

    def test_compile_test_excluded(self):
        checker = IsHeaderIncludeChecker()
        self.assertFalse(
            checker.should_process_file(
                str(PLATFORMS_ROOT / "arm" / "compile_test.hpp")
            )
        )

    def test_core_detection_excluded(self):
        checker = IsHeaderIncludeChecker()
        self.assertFalse(
            checker.should_process_file(
                str(PLATFORMS_ROOT / "arm" / "stm32" / "core_detection.h")
            )
        )

    def test_regular_file_included(self):
        checker = IsHeaderIncludeChecker()
        self.assertTrue(
            checker.should_process_file(str(PLATFORMS_ROOT / "avr" / "clockless_avr.h"))
        )

    def test_outside_platforms_excluded(self):
        checker = IsHeaderIncludeChecker()
        self.assertFalse(checker.should_process_file("src/fl/math.h"))


class TestSamdVsSam(unittest.TestCase):
    """Verify FL_IS_SAMD* matches is_samd.h, not is_sam.h."""

    def test_samd21_needs_is_samd(self):
        self.assertEqual(_get_required_header("FL_IS_SAMD21"), "is_samd.h")

    def test_samd51_needs_is_samd(self):
        self.assertEqual(_get_required_header("FL_IS_SAMD51"), "is_samd.h")

    def test_sam_needs_is_sam(self):
        self.assertEqual(_get_required_header("FL_IS_SAM"), "is_sam.h")


if __name__ == "__main__":
    unittest.main()
