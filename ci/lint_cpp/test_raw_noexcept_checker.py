#!/usr/bin/env python3
"""Unit tests for RawNoexceptChecker."""

import unittest

from ci.lint_cpp.raw_noexcept_checker import RawNoexceptChecker
from ci.util.check_files import FileContent


# Default test path — a normal src/fl/ header
_FL_PATH = "src/fl/some_module.h"
_CPP_PATH = "src/fl/some_module.cpp"
_CPP_HPP_PATH = "src/fl/some_module.cpp.hpp"
_PLATFORMS_PATH = "src/platforms/esp/32/drivers/parlio/parlio_engine.h"
_NOEXCEPT_DEF_PATH = "src/fl/stl/noexcept.h"
_THIRD_PARTY_PATH = "src/third_party/some_lib.h"
_OUT_OF_SRC_PATH = "tests/fl/some_test.cpp"


def _make(code: str, path: str = _FL_PATH) -> FileContent:
    lines = code.splitlines()
    return FileContent(path=path, content=code, lines=lines)


def _violations(code: str, path: str = _FL_PATH) -> list[tuple[int, str]]:
    c = RawNoexceptChecker()
    file_content = _make(code, path)
    if not c.should_process_file(path):
        return []
    c.check_file_content(file_content)
    return c.violations.get(path, [])


# ── File filtering ───────────────────────────────────────────────────────────


class TestFileFiltering(unittest.TestCase):
    def test_fl_header_processed(self) -> None:
        self.assertTrue(RawNoexceptChecker().should_process_file(_FL_PATH))

    def test_cpp_file_processed(self) -> None:
        self.assertTrue(RawNoexceptChecker().should_process_file(_CPP_PATH))

    def test_cpp_hpp_file_processed(self) -> None:
        self.assertTrue(RawNoexceptChecker().should_process_file(_CPP_HPP_PATH))

    def test_platforms_header_processed(self) -> None:
        self.assertTrue(RawNoexceptChecker().should_process_file(_PLATFORMS_PATH))

    def test_noexcept_definition_file_skipped(self) -> None:
        """The noexcept.h definition file must never be flagged."""
        self.assertFalse(RawNoexceptChecker().should_process_file(_NOEXCEPT_DEF_PATH))

    def test_third_party_skipped(self) -> None:
        self.assertFalse(RawNoexceptChecker().should_process_file(_THIRD_PARTY_PATH))

    def test_outside_src_skipped(self) -> None:
        """Tests/ files are not in src/ — checker should ignore them."""
        self.assertFalse(RawNoexceptChecker().should_process_file(_OUT_OF_SRC_PATH))

    def test_non_cpp_extension_skipped(self) -> None:
        self.assertFalse(RawNoexceptChecker().should_process_file("src/fl/build.py"))


# ── Should flag (raw noexcept) ───────────────────────────────────────────────


class TestShouldFlag(unittest.TestCase):
    def test_noexcept_on_method_declaration(self) -> None:
        self.assertEqual(len(_violations("void foo() noexcept;")), 1)

    def test_noexcept_on_method_definition(self) -> None:
        self.assertEqual(len(_violations("void foo() noexcept {")), 1)

    def test_noexcept_const_method(self) -> None:
        self.assertEqual(len(_violations("bool isReady() const noexcept;")), 1)

    def test_noexcept_move_constructor(self) -> None:
        self.assertEqual(len(_violations("MyClass(MyClass&& other) noexcept;")), 1)

    def test_noexcept_move_assignment(self) -> None:
        self.assertEqual(
            len(_violations("MyClass& operator=(MyClass&& other) noexcept;")), 1
        )

    def test_noexcept_in_platforms_file(self) -> None:
        """Platform files under src/ are also checked."""
        self.assertEqual(
            len(_violations("void process() noexcept;", path=_PLATFORMS_PATH)), 1
        )

    def test_multiple_noexcept_on_separate_lines(self) -> None:
        code = "void foo() noexcept;\nvoid bar() noexcept;"
        self.assertEqual(len(_violations(code)), 2)

    def test_noexcept_specifier_with_expression_allowed(self) -> None:
        """noexcept(expr) is the operator form and should NOT be flagged."""
        self.assertEqual(
            len(_violations("void swap(Foo& o) noexcept(noexcept(x.swap(o)));")), 0
        )

    def test_bare_noexcept_still_flagged_next_to_operator(self) -> None:
        """Bare noexcept on a different line should still be flagged."""
        self.assertEqual(len(_violations("void foo() noexcept;")), 1)


# ── Should pass (no raw noexcept) ────────────────────────────────────────────


class TestShouldPass(unittest.TestCase):
    def test_fl_noexcept_declaration(self) -> None:
        self.assertEqual(len(_violations("void foo() FL_NOEXCEPT;")), 0)

    def test_fl_noexcept_definition(self) -> None:
        self.assertEqual(len(_violations("void foo() FL_NOEXCEPT {")), 0)

    def test_fl_noexcept_const(self) -> None:
        self.assertEqual(len(_violations("bool bar() const FL_NOEXCEPT;")), 0)

    def test_fl_noexcept_override(self) -> None:
        self.assertEqual(len(_violations("void run() FL_NOEXCEPT override;")), 0)

    def test_code_without_noexcept(self) -> None:
        self.assertEqual(len(_violations("int add(int a, int b) { return a + b; }")), 0)

    def test_include_noexcept_header(self) -> None:
        """Including the noexcept header must not trigger a violation."""
        self.assertEqual(len(_violations('#include "fl/stl/noexcept.h"')), 0)

    def test_noexcept_in_definition_file_is_skipped(self) -> None:
        """noexcept.h itself must be excluded by should_process_file."""
        c = RawNoexceptChecker()
        self.assertFalse(c.should_process_file(_NOEXCEPT_DEF_PATH))

    def test_fl_noexcept_macro_expansion_in_other_file(self) -> None:
        """A file that defines its own alias must not trigger for the #define line."""
        code = "#define FL_NOEXCEPT noexcept\nvoid foo() FL_NOEXCEPT;"
        # The #define line should be allowed, the declaration uses the macro
        self.assertEqual(len(_violations(code)), 0)


# ── Comment exemptions ───────────────────────────────────────────────────────


class TestCommentExemptions(unittest.TestCase):
    def test_single_line_comment(self) -> None:
        self.assertEqual(len(_violations("// void foo() noexcept;")), 0)

    def test_inline_comment_only(self) -> None:
        """noexcept appearing only in the comment portion of a line is ignored."""
        self.assertEqual(len(_violations("void foo(); // use noexcept")), 0)

    def test_multiline_comment_start(self) -> None:
        code = "/* void foo() noexcept; */"
        self.assertEqual(len(_violations(code)), 0)

    def test_multiline_comment_spanning_lines(self) -> None:
        code = "/*\n * void foo() noexcept;\n */"
        self.assertEqual(len(_violations(code)), 0)


# ── Suppression comments ─────────────────────────────────────────────────────


class TestSuppression(unittest.TestCase):
    def test_ok_noexcept_suppression(self) -> None:
        self.assertEqual(len(_violations("void foo() noexcept; // ok noexcept")), 0)

    def test_nolint_suppression(self) -> None:
        self.assertEqual(len(_violations("void foo() noexcept; // nolint")), 0)

    def test_nolint_case_insensitive(self) -> None:
        self.assertEqual(len(_violations("void foo() noexcept; // NOLINT")), 0)

    def test_suppression_does_not_bleed_to_next_line(self) -> None:
        """Suppression on one line must not suppress the next."""
        code = "void foo() noexcept; // ok noexcept\nvoid bar() noexcept;"
        self.assertEqual(len(_violations(code)), 1)


# ── Edge cases ───────────────────────────────────────────────────────────────


class TestEdgeCases(unittest.TestCase):
    def test_empty_file(self) -> None:
        self.assertEqual(len(_violations("")), 0)

    def test_file_with_no_noexcept(self) -> None:
        code = "class Foo {\n    void bar();\n};"
        self.assertEqual(len(_violations(code)), 0)

    def test_noexcept_in_string_literal_is_still_flagged(self) -> None:
        """We do a simple regex, so noexcept in a string literal is flagged.
        This is an acceptable false positive — rare in practice and easy to
        suppress with // ok noexcept.
        """
        # The word noexcept in a string is flagged by the simple regex
        code = 'const char* msg = "noexcept is good";'
        # Note: this IS a false positive but per design the rule is strict
        # Users can suppress with // ok noexcept
        result = _violations(code)
        self.assertEqual(len(result), 1)  # simple regex hits string content

    def test_noexcept_false_in_string_can_be_suppressed(self) -> None:
        code = 'const char* msg = "noexcept is good"; // ok noexcept'
        self.assertEqual(len(_violations(code)), 0)

    def test_noexceptkeyword_no_match(self) -> None:
        """'noexceptkeyword' must not match — word boundaries required."""
        self.assertEqual(len(_violations("void foo() noexceptkeyword;")), 0)

    def test_fl_noexcept_macro_not_double_flagged(self) -> None:
        """FL_NOEXCEPT itself must never trigger the checker."""
        self.assertEqual(len(_violations("void foo() FL_NOEXCEPT;")), 0)

    def test_cpp_file_extension(self) -> None:
        c = RawNoexceptChecker()
        self.assertTrue(c.should_process_file("src/fl/something.cpp"))

    def test_hpp_file_extension(self) -> None:
        c = RawNoexceptChecker()
        self.assertTrue(c.should_process_file("src/fl/something.hpp"))


if __name__ == "__main__":
    unittest.main()
