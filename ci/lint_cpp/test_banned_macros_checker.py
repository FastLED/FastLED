#!/usr/bin/env python3
"""Unit tests for BannedMacrosChecker."""

import unittest

from ci.lint_cpp.banned_macros_checker import BannedMacrosChecker
from ci.util.check_files import FileContent


_SRC_PATH = "src/fl/example.h"
_TEST_PATH = "tests/fl/example.cpp"
_CPP_COMPAT_PATH = "src/cpp_compat.h"
_STATIC_ASSERT_PATH = "src/fl/stl/static_assert.h"
_THIRD_PARTY_PATH = "src/third_party/example.h"


def _make(code: str, path: str = _SRC_PATH) -> FileContent:
    return FileContent(path=path, content=code, lines=code.splitlines())


def _violations(code: str, path: str = _SRC_PATH) -> list[tuple[int, str]]:
    checker = BannedMacrosChecker()
    if not checker.should_process_file(path):
        return []
    checker.check_file_content(_make(code, path))
    return checker.violations.get(path, [])


class TestBannedMacrosChecker(unittest.TestCase):
    def test_static_assert_is_banned_in_src(self) -> None:
        self.assertEqual(len(_violations('static_assert(true, "ok");')), 1)

    def test_static_assert_is_banned_in_tests(self) -> None:
        self.assertEqual(
            len(_violations('static_assert(sizeof(int) > 0, "ok");', _TEST_PATH)), 1
        )

    def test_fl_static_assert_is_allowed(self) -> None:
        self.assertEqual(len(_violations('FL_STATIC_ASSERT(true, "ok");')), 0)

    def test_has_include_is_still_banned(self) -> None:
        self.assertEqual(len(_violations("#if __has_include(<vector>)")), 1)

    def test_comments_are_ignored(self) -> None:
        self.assertEqual(len(_violations('// static_assert(true, "ok");')), 0)
        self.assertEqual(
            len(_violations('int x = 1; // static_assert(true, "ok");')), 0
        )

    def test_multiline_comments_are_ignored(self) -> None:
        self.assertEqual(len(_violations('/*\nstatic_assert(true, "ok");\n*/')), 0)

    def test_definition_file_is_skipped(self) -> None:
        self.assertFalse(BannedMacrosChecker().should_process_file(_CPP_COMPAT_PATH))
        self.assertFalse(BannedMacrosChecker().should_process_file(_STATIC_ASSERT_PATH))

    def test_third_party_is_skipped(self) -> None:
        self.assertFalse(BannedMacrosChecker().should_process_file(_THIRD_PARTY_PATH))


if __name__ == "__main__":
    unittest.main()
