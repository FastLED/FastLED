#!/usr/bin/env python3
"""Unit tests for AsmJsLocationChecker."""

import unittest

from ci.lint_cpp.asm_js_location_checker import AsmJsLocationChecker
from ci.util.check_files import FileContent


_HEADER_PATH = "src/platforms/wasm/coroutine_platform_wasm.hpp"
_JS_CPP_HPP_PATH = "src/platforms/wasm/coroutine_platform_wasm.js.cpp.hpp"
_CPP_PATH = "src/platforms/wasm/example.cpp"
_THIRD_PARTY_PATH = "src/third_party/example.h"
_TEST_PATH = "tests/platforms/wasm/example.cpp"


def _make(code: str, path: str = _HEADER_PATH) -> FileContent:
    return FileContent(path=path, content=code, lines=code.splitlines())


def _violations(code: str, path: str = _HEADER_PATH) -> list[tuple[int, str]]:
    checker = AsmJsLocationChecker()
    if not checker.should_process_file(path):
        return []
    checker.check_file_content(_make(code, path))
    return checker.violations.get(path, [])


class TestFileFiltering(unittest.TestCase):
    def test_src_header_processed(self) -> None:
        self.assertTrue(AsmJsLocationChecker().should_process_file(_HEADER_PATH))

    def test_js_cpp_hpp_processed(self) -> None:
        self.assertTrue(AsmJsLocationChecker().should_process_file(_JS_CPP_HPP_PATH))

    def test_third_party_skipped(self) -> None:
        self.assertFalse(AsmJsLocationChecker().should_process_file(_THIRD_PARTY_PATH))

    def test_tests_skipped(self) -> None:
        self.assertFalse(AsmJsLocationChecker().should_process_file(_TEST_PATH))


class TestViolations(unittest.TestCase):
    def test_em_js_in_header_is_rejected(self) -> None:
        self.assertEqual(len(_violations("EM_JS(void, foo, (), {});")), 1)

    def test_em_async_js_in_cpp_is_rejected(self) -> None:
        self.assertEqual(
            len(_violations("EM_ASYNC_JS(void, foo, (), {});", _CPP_PATH)), 1
        )

    def test_em_asm_in_header_is_rejected(self) -> None:
        self.assertEqual(len(_violations("EM_ASM({ console.log('x'); });")), 1)

    def test_js_cpp_hpp_is_allowed(self) -> None:
        self.assertEqual(
            len(_violations("EM_JS(void, foo, (), {});", _JS_CPP_HPP_PATH)), 0
        )

    def test_comment_is_ignored(self) -> None:
        self.assertEqual(len(_violations("// EM_JS(void, foo, (), {});")), 0)

    def test_inline_comment_is_ignored(self) -> None:
        self.assertEqual(
            len(_violations("void foo(); // EM_JS(void, foo, (), {});")), 0
        )


if __name__ == "__main__":
    unittest.main()
