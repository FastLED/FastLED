#!/usr/bin/env python3
"""Tests for NoexceptSpecialMembersChecker."""

from __future__ import annotations

import pytest

from ci.lint_cpp.noexcept_special_members_checker import (
    NoexceptSpecialMembersChecker,
    classify_line,
    fix_file,
    signature_has_noexcept,
)
from ci.util.check_files import FileContent


# ── Helpers ─────────────────────────────────────────────────────────────────

def _make_fc(path: str, content: str) -> FileContent:
    return FileContent(path=path, content=content, lines=content.splitlines())


def _check(content: str, path: str = "src/fl/test.h") -> list[tuple[int, str]]:
    checker = NoexceptSpecialMembersChecker()
    fc = _make_fc(path, content)
    checker.check_file_content(fc)
    return checker.violations.get(path, [])


# ── File filtering ──────────────────────────────────────────────────────────


class TestShouldProcessFile:
    def test_fl_header(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("src/fl/foo.h") is True

    def test_fl_cpp(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("src/fl/foo.cpp") is True

    def test_fl_cpp_hpp(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("src/fl/bar.cpp.hpp") is True

    def test_fl_nested(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("src/fl/stl/vector.h") is True

    def test_not_fl(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("src/platforms/esp/foo.h") is False

    def test_noexcept_h_excluded(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("src/fl/stl/noexcept.h") is False

    def test_third_party_excluded(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("src/fl/third_party/foo.h") is False

    def test_txt_excluded(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("src/fl/foo.txt") is False

    def test_absolute_path(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("/repo/src/fl/foo.h") is True

    def test_windows_path(self):
        c = NoexceptSpecialMembersChecker()
        assert c.should_process_file("C:\\repo\\src\\fl\\foo.h") is True


# ── Destructor detection ────────────────────────────────────────────────────


class TestDestructors:
    def test_missing_noexcept_declaration(self):
        code = "class Foo {\n    ~Foo();\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "destructor" in viols[0][1]

    def test_missing_noexcept_inline(self):
        code = "class Foo {\n    ~Foo() {}\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "destructor" in viols[0][1]

    def test_missing_noexcept_default(self):
        code = "class Foo {\n    ~Foo() = default;\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_missing_noexcept_virtual(self):
        code = "class Foo {\n    virtual ~Foo();\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_missing_noexcept_override(self):
        code = "class Foo {\n    ~Foo() override;\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_has_fl_noexcept(self):
        code = "class Foo {\n    ~Foo() FL_NOEXCEPT;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_has_fl_noexcept_inline(self):
        code = "class Foo {\n    ~Foo() FL_NOEXCEPT {}\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_has_fl_noexcept_default(self):
        code = "class Foo {\n    ~Foo() FL_NOEXCEPT = default;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_has_bare_noexcept(self):
        # bare noexcept counts as having it (raw_noexcept_checker handles conversion)
        code = "class Foo {\n    ~Foo() noexcept;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_delete_missing(self):
        code = "class Foo {\n    ~Foo() = delete;\n};"
        viols = _check(code)
        # = delete should still be flagged (consistency)
        assert len(viols) == 1


# ── Default constructor detection ───────────────────────────────────────────


class TestDefaultConstructors:
    def test_missing_noexcept(self):
        code = "class Foo {\n    Foo();\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "default constructor" in viols[0][1]

    def test_missing_noexcept_default(self):
        code = "class Foo {\n    Foo() = default;\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_missing_noexcept_inline(self):
        code = "class Foo {\n    Foo() {}\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_explicit_missing(self):
        code = "class Foo {\n    explicit Foo();\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_has_fl_noexcept(self):
        code = "class Foo {\n    Foo() FL_NOEXCEPT;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_has_fl_noexcept_default(self):
        code = "class Foo {\n    Foo() FL_NOEXCEPT = default;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_not_constructor(self):
        # Has a return type → not a constructor
        code = "class Foo {\n    int Foo();\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_void_param(self):
        code = "class Foo {\n    Foo(void);\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "default constructor" in viols[0][1]


# ── Copy constructor detection ──────────────────────────────────────────────


class TestCopyConstructors:
    def test_missing_noexcept(self):
        code = "class Foo {\n    Foo(const Foo&);\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "copy constructor" in viols[0][1]

    def test_missing_noexcept_named(self):
        code = "class Foo {\n    Foo(const Foo& other);\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_missing_noexcept_default(self):
        code = "class Foo {\n    Foo(const Foo&) = default;\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_has_fl_noexcept(self):
        code = "class Foo {\n    Foo(const Foo&) FL_NOEXCEPT;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_has_fl_noexcept_default(self):
        code = "class Foo {\n    Foo(const Foo&) FL_NOEXCEPT = default;\n};"
        viols = _check(code)
        assert len(viols) == 0


# ── Move constructor detection ──────────────────────────────────────────────


class TestMoveConstructors:
    def test_missing_noexcept(self):
        code = "class Foo {\n    Foo(Foo&&);\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "move constructor" in viols[0][1]

    def test_missing_noexcept_named(self):
        code = "class Foo {\n    Foo(Foo&& other);\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_missing_noexcept_default(self):
        code = "class Foo {\n    Foo(Foo&&) = default;\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_has_fl_noexcept(self):
        code = "class Foo {\n    Foo(Foo&&) FL_NOEXCEPT;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_has_fl_noexcept_default(self):
        code = "class Foo {\n    Foo(Foo&&) FL_NOEXCEPT = default;\n};"
        viols = _check(code)
        assert len(viols) == 0


# ── Copy assignment detection ───────────────────────────────────────────────


class TestCopyAssignment:
    def test_missing_noexcept(self):
        code = "class Foo {\n    Foo& operator=(const Foo&);\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "assignment operator" in viols[0][1]

    def test_missing_noexcept_default(self):
        code = "class Foo {\n    Foo& operator=(const Foo&) = default;\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_has_fl_noexcept(self):
        code = "class Foo {\n    Foo& operator=(const Foo&) FL_NOEXCEPT;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_has_fl_noexcept_default(self):
        code = "class Foo {\n    Foo& operator=(const Foo&) FL_NOEXCEPT = default;\n};"
        viols = _check(code)
        assert len(viols) == 0


# ── Move assignment detection ───────────────────────────────────────────────


class TestMoveAssignment:
    def test_missing_noexcept(self):
        code = "class Foo {\n    Foo& operator=(Foo&&);\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "assignment operator" in viols[0][1]

    def test_missing_noexcept_default(self):
        code = "class Foo {\n    Foo& operator=(Foo&&) = default;\n};"
        viols = _check(code)
        assert len(viols) == 1

    def test_has_fl_noexcept(self):
        code = "class Foo {\n    Foo& operator=(Foo&&) FL_NOEXCEPT;\n};"
        viols = _check(code)
        assert len(viols) == 0


# ── Multi-line signatures ──────────────────────────────────────────────────


class TestMultiLine:
    def test_dtor_fl_noexcept_next_line(self):
        code = "class Foo {\n    ~Foo()\n        FL_NOEXCEPT;\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_move_ctor_multiline_init(self):
        code = (
            "class Foo {\n"
            "    Foo(Foo&& other) FL_NOEXCEPT\n"
            "        : member(other.member) {}\n"
            "};"
        )
        viols = _check(code)
        assert len(viols) == 0

    def test_multiline_params_missing(self):
        code = (
            "class Foo {\n"
            "    Foo(\n"
            "        const Foo& other);\n"
            "};"
        )
        viols = _check(code)
        # The opening line has the constructor start, closing ) is on line 3
        assert len(viols) == 1

    def test_multiline_params_has_noexcept(self):
        code = (
            "class Foo {\n"
            "    Foo(\n"
            "        const Foo& other) FL_NOEXCEPT;\n"
            "};"
        )
        viols = _check(code)
        assert len(viols) == 0


# ── Suppression ─────────────────────────────────────────────────────────────


class TestSuppression:
    def test_ok_no_fl_noexcept(self):
        code = "class Foo {\n    ~Foo(); // ok no FL_NOEXCEPT\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_nolint(self):
        code = "class Foo {\n    ~Foo(); // nolint\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_case_insensitive(self):
        code = "class Foo {\n    ~Foo(); // OK NO FL_NOEXCEPT\n};"
        viols = _check(code)
        assert len(viols) == 0


# ── Comments ────────────────────────────────────────────────────────────────


class TestComments:
    def test_line_comment(self):
        code = "class Foo {\n    // ~Foo();\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_block_comment(self):
        code = "class Foo {\n    /* ~Foo(); */\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_multiline_block_comment(self):
        code = "class Foo {\n/*\n    ~Foo();\n*/\n};"
        viols = _check(code)
        assert len(viols) == 0


# ── Edge cases ──────────────────────────────────────────────────────────────


class TestEdgeCases:
    def test_not_a_constructor_with_return_type(self):
        code = "class Foo {\n    static Foo* Foo();\n};"
        # "static Foo*" is not in _CTOR_QUALS → not flagged
        viols = _check(code)
        assert len(viols) == 0

    def test_regular_method_not_flagged(self):
        code = "class Foo {\n    void bar();\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_non_special_constructor_not_flagged(self):
        code = "class Foo {\n    Foo(int x);\n};"
        viols = _check(code)
        # Not a default/copy/move constructor → not flagged
        assert len(viols) == 0

    def test_preprocessor_not_flagged(self):
        code = "class Foo {\n#define FOO ~Foo()\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_empty_file(self):
        viols = _check("")
        assert len(viols) == 0

    def test_no_class(self):
        code = "void foo();"
        viols = _check(code)
        assert len(viols) == 0

    def test_struct_dtor(self):
        code = "struct Bar {\n    ~Bar();\n};"
        viols = _check(code)
        assert len(viols) == 1
        assert "destructor" in viols[0][1]

    def test_multiple_classes(self):
        code = (
            "class Foo {\n    ~Foo();\n};\n"
            "class Bar {\n    ~Bar() FL_NOEXCEPT;\n};"
        )
        viols = _check(code)
        assert len(viols) == 1
        assert "Foo" in viols[0][1]

    def test_manual_destructor_call_not_flagged(self):
        """obj->~T() is a destructor CALL, not a declaration."""
        code = "class T {\n    void destroy() { get_object()->~T(); }\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_manual_destructor_call_dot_not_flagged(self):
        """obj.~T() is a destructor CALL, not a declaration."""
        code = "class T {\n    void destroy() { obj.~T(); }\n};"
        viols = _check(code)
        assert len(viols) == 0

    def test_bitwise_not_not_flagged(self):
        """~block_type(0) is bitwise NOT, not a destructor."""
        code = (
            "class block_type {\n"
            "    void set_all() { x = static_cast<block_type>(~block_type(0)); }\n"
            "};"
        )
        viols = _check(code)
        assert len(viols) == 0

    def test_bitwise_not_cast_not_flagged(self):
        """~Type(value) with non-empty parens is not a destructor."""
        code = "class Foo {\n    int x = ~Foo(42);\n};"
        viols = _check(code)
        assert len(viols) == 0


# ── classify_line unit tests ───────────────────────────────────────────────


class TestClassifyLine:
    def test_destructor(self):
        r = classify_line("    ~Foo();", {"Foo"})
        assert r is not None
        assert r[0] == "destructor"

    def test_default_ctor(self):
        r = classify_line("    Foo();", {"Foo"})
        assert r is not None
        assert r[0] == "default constructor"

    def test_copy_ctor(self):
        r = classify_line("    Foo(const Foo& other);", {"Foo"})
        assert r is not None
        assert r[0] == "copy constructor"

    def test_move_ctor(self):
        r = classify_line("    Foo(Foo&& other);", {"Foo"})
        assert r is not None
        assert r[0] == "move constructor"

    def test_copy_assign(self):
        r = classify_line("    Foo& operator=(const Foo& other);", {"Foo"})
        assert r is not None
        assert r[0] == "assignment operator"

    def test_move_assign(self):
        r = classify_line("    Foo& operator=(Foo&& other);", {"Foo"})
        assert r is not None
        assert r[0] == "assignment operator"

    def test_regular_method(self):
        r = classify_line("    void bar();", {"Foo"})
        assert r is None

    def test_non_special_ctor(self):
        r = classify_line("    Foo(int x);", {"Foo"})
        assert r is None

    def test_return_type(self):
        r = classify_line("    Foo* Foo();", {"Foo"})
        # "Foo*" is not in allowed qualifiers
        assert r is None


# ── Auto-fix tests ──────────────────────────────────────────────────────────


class TestAutoFix:
    def test_fix_destructor(self, tmp_path):
        p = tmp_path / "test.h"
        p.write_text(
            '#pragma once\n#include "fl/stl/noexcept.h"\n'
            "class Foo {\n    ~Foo();\n};\n"
        )
        n, descs = fix_file(p)
        assert n == 1
        fixed = p.read_text()
        assert "~Foo() FL_NOEXCEPT;" in fixed

    def test_fix_default_ctor(self, tmp_path):
        p = tmp_path / "test.h"
        p.write_text(
            '#pragma once\n#include "fl/stl/noexcept.h"\n'
            "class Foo {\n    Foo() = default;\n};\n"
        )
        n, descs = fix_file(p)
        assert n == 1
        fixed = p.read_text()
        assert "Foo() FL_NOEXCEPT = default;" in fixed

    def test_fix_copy_ctor(self, tmp_path):
        p = tmp_path / "test.h"
        p.write_text(
            '#pragma once\n#include "fl/stl/noexcept.h"\n'
            "class Foo {\n    Foo(const Foo& other);\n};\n"
        )
        n, descs = fix_file(p)
        assert n == 1
        fixed = p.read_text()
        assert "Foo(const Foo& other) FL_NOEXCEPT;" in fixed

    def test_fix_move_assign(self, tmp_path):
        p = tmp_path / "test.h"
        p.write_text(
            '#pragma once\n#include "fl/stl/noexcept.h"\n'
            "class Foo {\n    Foo& operator=(Foo&&) = default;\n};\n"
        )
        n, descs = fix_file(p)
        assert n == 1
        fixed = p.read_text()
        assert "operator=(Foo&&) FL_NOEXCEPT = default;" in fixed

    def test_no_double_fix(self, tmp_path):
        p = tmp_path / "test.h"
        p.write_text(
            '#pragma once\n#include "fl/stl/noexcept.h"\n'
            "class Foo {\n    ~Foo() FL_NOEXCEPT;\n};\n"
        )
        n, descs = fix_file(p)
        assert n == 0

    def test_adds_include(self, tmp_path):
        p = tmp_path / "test.h"
        p.write_text("#pragma once\nclass Foo {\n    ~Foo();\n};\n")
        n, descs = fix_file(p)
        assert n == 1
        fixed = p.read_text()
        assert 'fl/stl/noexcept.h' in fixed

    def test_dry_run_no_change(self, tmp_path):
        p = tmp_path / "test.h"
        original = "#pragma once\nclass Foo {\n    ~Foo();\n};\n"
        p.write_text(original)
        n, descs = fix_file(p, dry_run=True)
        assert n == 1
        assert p.read_text() == original  # unchanged


# ── signature_has_noexcept tests ────────────────────────────────────────────


class TestSignatureHasNoexcept:
    def test_same_line(self):
        lines = ["    ~Foo() FL_NOEXCEPT;"]
        # open paren is at index where ( is
        idx = lines[0].index("(")
        assert signature_has_noexcept(lines, 0, idx) is True

    def test_same_line_missing(self):
        lines = ["    ~Foo();"]
        idx = lines[0].index("(")
        assert signature_has_noexcept(lines, 0, idx) is False

    def test_next_line(self):
        lines = ["    ~Foo()", "        FL_NOEXCEPT;"]
        idx = lines[0].index("(")
        assert signature_has_noexcept(lines, 0, idx) is True

    def test_next_line_missing(self):
        lines = ["    ~Foo()", "        ;"]
        idx = lines[0].index("(")
        assert signature_has_noexcept(lines, 0, idx) is False
