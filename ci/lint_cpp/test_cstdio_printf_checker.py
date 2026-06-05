#!/usr/bin/env python3
"""Unit tests for CstdioPrintfChecker."""

import unittest

from ci.lint_cpp.cstdio_printf_checker import CstdioPrintfChecker
from ci.util.check_files import FileContent


_SRC_PATH = "src/fl/example.cpp"
_THIRD_PARTY_PATH = "src/third_party/TJpg/driver.cpp.hpp"
_STDIO_H_PATH = "src/fl/stl/stdio.h"
_CSTDIO_H_PATH = "src/fl/stl/cstdio.h"
_WASM_PATH = "src/platforms/wasm/audio_input_wasm.cpp.hpp"
_POSIX_PATH = "src/platforms/posix/run_unit_test.hpp"
_STUB_PATH = "src/platforms/stub/Arduino.h"


def _violations(code: str, path: str = _SRC_PATH) -> list[tuple[int, str]]:
    checker = CstdioPrintfChecker()
    if not checker.should_process_file(path):
        return []
    file_content = FileContent(path=path, content=code, lines=code.splitlines())
    checker.check_file_content(file_content)
    return checker.violations.get(path, [])


class TestCstdioPrintfChecker(unittest.TestCase):
    # --- Banned forms ------------------------------------------------------

    def test_bare_snprintf_is_banned(self) -> None:
        self.assertEqual(len(_violations('snprintf(buf, 8, "%d", x);')), 1)

    def test_bare_printf_is_banned(self) -> None:
        self.assertEqual(len(_violations('printf("hi\\n");')), 1)

    def test_bare_fprintf_is_banned(self) -> None:
        self.assertEqual(len(_violations('fprintf(stderr, "boom %d", code);')), 1)

    def test_explicit_root_namespace_is_banned(self) -> None:
        self.assertEqual(len(_violations('::printf("bad\\n");')), 1)

    def test_explicit_std_namespace_is_banned(self) -> None:
        self.assertEqual(len(_violations("std::snprintf(buf, n, fmt, x);")), 1)

    def test_all_family_members_caught(self) -> None:
        for func in (
            "snprintf",
            "sprintf",
            "printf",
            "fprintf",
            "vfprintf",
            "vsnprintf",
            "vprintf",
            "vsprintf",
        ):
            with self.subTest(func=func):
                self.assertEqual(
                    len(_violations(f"{func}(args);")),
                    1,
                    f"{func} should be flagged",
                )

    # --- Allowed forms -----------------------------------------------------

    def test_fl_snprintf_is_allowed(self) -> None:
        self.assertEqual(len(_violations('fl::snprintf(buf, 8, "%d", x);')), 0)

    def test_fl_printf_is_allowed(self) -> None:
        self.assertEqual(len(_violations('fl::printf("hi\\n");')), 0)

    def test_member_dot_call_is_allowed(self) -> None:
        self.assertEqual(len(_violations('Serial.printf("x");')), 0)

    def test_member_arrow_call_is_allowed(self) -> None:
        self.assertEqual(len(_violations('ptr->printf("x");')), 0)

    def test_user_class_qualified_call_is_allowed(self) -> None:
        # `SerialPort::printf(...)` — user-defined class method, not libc.
        self.assertEqual(
            len(_violations("inline size_t SerialPort::printf(const char* fmt) {")),
            0,
        )

    def test_user_class_qualified_snprintf_is_allowed(self) -> None:
        self.assertEqual(len(_violations("size_t MyLogger::snprintf(char* buf) {")), 0)

    def test_function_declaration_with_type_prefix_is_allowed(self) -> None:
        # `void printf(...)` as a member declaration inside a class body.
        self.assertEqual(
            len(_violations("    void printf(const char* fmt, Args... args);")), 0
        )

    def test_substring_not_matched(self) -> None:
        # `my_snprintf` shares a suffix with `snprintf` but \b prevents
        # a sub-word match.
        self.assertEqual(len(_violations('my_snprintf(buf, n, "%d", x);')), 0)

    # --- Comment / preprocessor handling -----------------------------------

    def test_single_line_comments_are_ignored(self) -> None:
        self.assertEqual(len(_violations('// snprintf(buf, n, "x");')), 0)
        self.assertEqual(len(_violations('int x = 1; // snprintf(buf, n, "x");')), 0)

    def test_multiline_comments_are_ignored(self) -> None:
        self.assertEqual(len(_violations("/*\nsnprintf(buf, n, fmt);\n*/")), 0)

    def test_include_directives_are_ignored(self) -> None:
        self.assertEqual(len(_violations("#include <cstdio>")), 0)
        # Even if a macro-like include text contains the word.
        self.assertEqual(len(_violations("#define USE_PRINTF 1")), 0)

    def test_opt_out_marker_skips_line(self) -> None:
        self.assertEqual(
            len(_violations("snprintf(buf, n, fmt); // ok cstdio - boundary case")),
            0,
        )

    # --- Path exclusions ---------------------------------------------------

    def test_third_party_is_excluded(self) -> None:
        self.assertEqual(
            len(_violations('snprintf(buf, n, "x");', _THIRD_PARTY_PATH)), 0
        )

    def test_stdio_wrapper_header_is_excluded(self) -> None:
        self.assertEqual(
            len(_violations('int x = snprintf(buf, n, "x");', _STDIO_H_PATH)), 0
        )

    def test_cstdio_wrapper_header_is_excluded(self) -> None:
        self.assertEqual(
            len(_violations('int x = snprintf(buf, n, "x");', _CSTDIO_H_PATH)), 0
        )

    def test_wasm_platform_is_excluded(self) -> None:
        # Emscripten libc handles printf without newlib's _svfprintf_r.
        self.assertEqual(len(_violations('printf("wasm call");', _WASM_PATH)), 0)

    def test_posix_host_runner_is_excluded(self) -> None:
        self.assertEqual(
            len(_violations('fprintf(stderr, "host\\n");', _POSIX_PATH)),
            0,
        )

    def test_stub_platform_is_excluded(self) -> None:
        self.assertEqual(len(_violations('printf("stub call");', _STUB_PATH)), 0)

    # --- Format specifier coverage ----------------------------------------

    def test_width_and_precision_specifiers_caught(self) -> None:
        self.assertEqual(len(_violations('snprintf(buf, 3, "%02x", b);')), 1)
        self.assertEqual(len(_violations('snprintf(buf, n, "%.2f", v);')), 1)

    def test_zx_size_t_specifier_caught(self) -> None:
        self.assertEqual(len(_violations('snprintf(buf, n, "%zx", len);')), 1)


if __name__ == "__main__":
    unittest.main()
