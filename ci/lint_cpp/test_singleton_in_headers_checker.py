#!/usr/bin/env python3
"""Unit tests for SingletonInHeadersChecker."""

import unittest

from ci.lint_cpp.singleton_in_headers_checker import SingletonInHeadersChecker
from ci.util.check_files import FileContent


def _make_content(code: str, path: str = "test.h") -> FileContent:
    lines = code.splitlines()
    return FileContent(path=path, content=code, lines=lines)


def _get_violations(code: str, path: str = "test.h") -> list[tuple[int, str]]:
    checker = SingletonInHeadersChecker()
    fc = _make_content(code, path)
    if not checker.should_process_file(path):
        return []
    checker.check_file_content(fc)
    return checker.violations.get(path, [])


class TestSingletonInHeadersChecker(unittest.TestCase):
    # --- Header rules: Singleton<T> instance calls forbidden, friend OK ---

    def test_singleton_instance_call_in_header_is_violation(self):
        violations = _get_violations("return Singleton<Bar>::instance();")
        self.assertEqual(len(violations), 1)

    def test_singleton_friend_in_header_passes(self):
        violations = _get_violations("friend class fl::Singleton<Foo>;")
        self.assertEqual(len(violations), 0)

    def test_singleton_shared_in_header_passes(self):
        violations = _get_violations("return fl::SingletonShared<Foo>::instance();")
        self.assertEqual(len(violations), 0)

    def test_singleton_shared_friend_in_header_passes(self):
        violations = _get_violations("friend class fl::SingletonShared<Foo>;")
        self.assertEqual(len(violations), 0)

    # --- .cpp.hpp rules: SingletonShared<T> forbidden ---

    def test_singleton_in_cpp_hpp_passes(self):
        violations = _get_violations(
            "return fl::Singleton<Foo>::instance();", path="foo.cpp.hpp"
        )
        self.assertEqual(len(violations), 0)

    def test_singleton_shared_in_cpp_hpp_is_violation(self):
        violations = _get_violations(
            "return fl::SingletonShared<Foo>::instance();", path="foo.cpp.hpp"
        )
        self.assertEqual(len(violations), 1)

    # --- Comment handling ---

    def test_singleton_in_comment_passes(self):
        violations = _get_violations("// return fl::Singleton<Foo>::instance();")
        self.assertEqual(len(violations), 0)

    def test_singleton_in_multiline_comment_passes(self):
        code = "/* return fl::Singleton<Foo>::instance(); */"
        violations = _get_violations(code)
        self.assertEqual(len(violations), 0)

    # --- File exclusions ---

    def test_singleton_h_itself_excluded(self):
        violations = _get_violations(
            "template <typename T> class Singleton {",
            path="src/fl/singleton.h",
        )
        self.assertEqual(len(violations), 0)

    def test_hpp_file_checked(self):
        violations = _get_violations(
            "return fl::Singleton<Baz>::instance();", path="test.hpp"
        )
        self.assertEqual(len(violations), 1)

    def test_hpp_friend_in_hpp_passes(self):
        violations = _get_violations(
            "friend class fl::Singleton<Baz>;", path="test.hpp"
        )
        self.assertEqual(len(violations), 0)


if __name__ == "__main__":
    unittest.main()
