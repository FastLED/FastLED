#!/usr/bin/env python3
"""Tests for optimized lint checkers.

Validates that performance optimizations (pre-compiled regexes, single-regex
BannedHeadersChecker, file content caching) did not break detection logic.

Each test creates a synthetic FileContent and checks that the checker
correctly detects violations (or correctly passes clean code).
"""

import unittest

from ci.lint_cpp.banned_headers_checker import (
    BANNED_HEADERS_COMMON,
    BANNED_HEADERS_CORE,
    BannedHeadersChecker,
)
from ci.lint_cpp.cpp_include_checker import CppIncludeChecker
from ci.lint_cpp.google_member_style_checker import GoogleMemberStyleChecker
from ci.lint_cpp.numeric_limit_macros_checker import NumericLimitMacroChecker
from ci.lint_cpp.static_in_headers_checker import StaticInHeaderChecker
from ci.util.check_files import FileContent, MultiCheckerFileProcessor


def _make(content: str, path: str = "src/fl/test.h") -> FileContent:
    """Create a FileContent from a string."""
    return FileContent(path=path, content=content, lines=content.splitlines())


# ============================================================================
# BannedHeadersChecker
# ============================================================================


class TestBannedHeadersChecker(unittest.TestCase):
    """Verify the single-regex optimization still catches all banned headers."""

    def _check(
        self, content: str, path: str = "src/fl/test.cpp"
    ) -> list[tuple[int, str]]:
        checker = BannedHeadersChecker(
            banned_headers_list=BANNED_HEADERS_CORE, strict_mode=True
        )
        checker.check_file_content(_make(content, path))
        return checker.violations.get(path, [])

    def test_detects_banned_vector(self):
        violations = self._check('#include <vector>\nint x;')
        self.assertEqual(len(violations), 1)
        self.assertIn("vector", violations[0][1])

    def test_detects_banned_iostream(self):
        violations = self._check('#include <iostream>\nint x;')
        self.assertEqual(len(violations), 1)
        self.assertIn("iostream", violations[0][1])

    def test_detects_banned_stdint_h(self):
        violations = self._check('#include <stdint.h>\nint x;')
        self.assertEqual(len(violations), 1)
        self.assertIn("stdint.h", violations[0][1])

    def test_detects_banned_string_h(self):
        violations = self._check('#include <string.h>\nint x;')
        self.assertEqual(len(violations), 1)
        self.assertIn("string.h", violations[0][1])

    def test_detects_quoted_banned_header(self):
        violations = self._check('#include "algorithm"\nint x;')
        self.assertEqual(len(violations), 1)

    def test_clean_code_passes(self):
        violations = self._check('#include "fl/stl/vector.h"\nint x;')
        self.assertEqual(len(violations), 0)

    def test_comment_line_ignored(self):
        violations = self._check('// #include <vector>\nint x;')
        self.assertEqual(len(violations), 0)

    def test_detects_private_libcpp_header(self):
        violations = self._check('#include "__algorithm/min.h"\nint x;')
        self.assertEqual(len(violations), 1)
        self.assertIn("private libc++", violations[0][1])

    def test_bypass_comment_in_non_strict_cpp(self):
        checker = BannedHeadersChecker(
            banned_headers_list=BANNED_HEADERS_COMMON, strict_mode=False
        )
        path = "src/platforms/test.cpp"
        checker.check_file_content(
            _make('#include <vector>  // ok include\nint x;', path)
        )
        self.assertEqual(len(checker.violations.get(path, [])), 0)

    def test_multiple_banned_on_different_lines(self):
        content = '#include <vector>\n#include <map>\nint x;'
        violations = self._check(content)
        self.assertEqual(len(violations), 2)


# ============================================================================
# CppIncludeChecker
# ============================================================================


class TestCppIncludeChecker(unittest.TestCase):
    """Verify pre-compiled regex detects .cpp includes."""

    def _check(
        self, content: str, path: str = "src/fl/test.h"
    ) -> list[tuple[int, str]]:
        checker = CppIncludeChecker()
        checker.check_file_content(_make(content, path))
        return checker.violations.get(path, [])

    def test_detects_cpp_include(self):
        violations = self._check('#include "foo.cpp"\nint x;')
        self.assertEqual(len(violations), 1)

    def test_clean_hpp_include_passes(self):
        violations = self._check('#include "foo.h"\nint x;')
        self.assertEqual(len(violations), 0)

    def test_comment_ignored(self):
        violations = self._check('// #include "foo.cpp"\nint x;')
        self.assertEqual(len(violations), 0)


# ============================================================================
# GoogleMemberStyleChecker
# ============================================================================


class TestGoogleMemberStyleChecker(unittest.TestCase):
    """Verify pre-compiled regex detects Google-style members."""

    def _check(
        self, content: str, path: str = "src/fl/test.h"
    ) -> list[tuple[int, str]]:
        checker = GoogleMemberStyleChecker()
        checker.check_file_content(_make(content, path))
        return checker.violations.get(path, [])

    def test_detects_trailing_underscore(self):
        violations = self._check("class Foo {\n    int state_;\n};")
        self.assertEqual(len(violations), 1)
        self.assertIn("mState", violations[0][1])

    def test_m_prefix_passes(self):
        violations = self._check("class Foo {\n    int mState;\n};")
        self.assertEqual(len(violations), 0)

    def test_comment_ignored(self):
        violations = self._check("// int state_;")
        self.assertEqual(len(violations), 0)

    def test_string_literal_ignored(self):
        violations = self._check('const char* s = "state_";')
        self.assertEqual(len(violations), 0)


# ============================================================================
# NumericLimitMacroChecker
# ============================================================================


class TestNumericLimitMacroChecker(unittest.TestCase):
    """Verify pre-compiled regex detects numeric limit macros."""

    def _check(
        self, content: str, path: str = "src/fl/test.cpp"
    ) -> list[tuple[int, str]]:
        checker = NumericLimitMacroChecker()
        checker.check_file_content(_make(content, path))
        return checker.violations.get(path, [])

    def test_detects_uint32_max(self):
        violations = self._check("int x = UINT32_MAX;")
        self.assertEqual(len(violations), 1)
        self.assertIn("fl::numeric_limits<uint32_t>::max()", violations[0][1])

    def test_detects_int_min(self):
        violations = self._check("int x = INT_MIN;")
        self.assertEqual(len(violations), 1)

    def test_fl_numeric_limits_passes(self):
        violations = self._check(
            "int x = fl::numeric_limits<uint32_t>::max();"
        )
        self.assertEqual(len(violations), 0)

    def test_comment_ignored(self):
        violations = self._check("// int x = UINT32_MAX;")
        self.assertEqual(len(violations), 0)

    def test_suppression_comment(self):
        violations = self._check(
            "int x = UINT32_MAX;  // okay numeric limit macro"
        )
        self.assertEqual(len(violations), 0)


# ============================================================================
# StaticInHeaderChecker
# ============================================================================


class TestStaticInHeaderChecker(unittest.TestCase):
    """Verify pre-compiled regex detects static locals in headers."""

    def _check(
        self, content: str, path: str = "src/fl/test.h"
    ) -> list[tuple[int, str]]:
        checker = StaticInHeaderChecker()
        checker.check_file_content(_make(content, path))
        return checker.violations.get(path, [])

    def test_detects_static_local_in_header(self):
        content = (
            "int getAll() {\n"
            "    static int instances = 0;\n"
            "    return instances;\n"
            "}"
        )
        violations = self._check(content)
        self.assertEqual(len(violations), 1)

    def test_template_function_allowed(self):
        content = (
            "template<typename T>\n"
            "T& getSingleton() {\n"
            "    static T instance;\n"
            "    return instance;\n"
            "}"
        )
        violations = self._check(content)
        self.assertEqual(len(violations), 0)

    def test_suppression_comment(self):
        content = (
            "int getAll() {\n"
            "    static int instances = 0;  // okay static in header\n"
            "    return instances;\n"
            "}"
        )
        violations = self._check(content)
        self.assertEqual(len(violations), 0)

    def test_cpp_file_not_processed(self):
        checker = StaticInHeaderChecker()
        self.assertFalse(checker.should_process_file("src/fl/test.cpp"))

    def test_header_file_processed(self):
        checker = StaticInHeaderChecker()
        self.assertTrue(checker.should_process_file("src/fl/test.h"))


# ============================================================================
# MultiCheckerFileProcessor caching
# ============================================================================


class TestFileContentCaching(unittest.TestCase):
    """Verify that file content caching works correctly."""

    def test_cache_reuses_content(self):
        """Verify that reading the same file twice uses the cache."""
        import os
        import tempfile

        # Create a temporary file
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".h", delete=False, encoding="utf-8"
        ) as f:
            f.write("#include <vector>\nint x;")
            temp_path = f.name

        try:
            processor = MultiCheckerFileProcessor()

            # Read file twice
            content1 = processor._get_file_content(temp_path)
            content2 = processor._get_file_content(temp_path)

            # Same object should be returned (cache hit)
            self.assertIs(content1, content2)
        finally:
            os.unlink(temp_path)


if __name__ == "__main__":
    unittest.main()
