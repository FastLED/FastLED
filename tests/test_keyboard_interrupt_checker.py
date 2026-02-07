"""Tests for the standalone KeyboardInterrupt checker.

Run with:
    uv run python -m pytest tests/test_keyboard_interrupt_checker.py
"""

from __future__ import annotations

import unittest

from ci.lint_python.keyboard_interrupt_checker import (
    Violation,
    check_file,
    has_broad_except,
)


class TestRegexPreFilter(unittest.TestCase):
    """Test the regex pre-filter that decides whether to run AST analysis."""

    def test_bare_except(self) -> None:
        assert has_broad_except("try:\n    pass\nexcept:\n    pass")

    def test_except_exception(self) -> None:
        assert has_broad_except("except Exception as e:")

    def test_except_base_exception(self) -> None:
        assert has_broad_except("except BaseException:")

    def test_except_keyboard_interrupt(self) -> None:
        assert has_broad_except("except KeyboardInterrupt:")

    def test_except_tuple(self) -> None:
        assert has_broad_except("except (ValueError, Exception):")

    def test_specific_exception_no_match(self) -> None:
        assert not has_broad_except("except ValueError as e:\n    pass")

    def test_no_except_at_all(self) -> None:
        assert not has_broad_except("try:\n    pass\nfinally:\n    pass")


class TestKBI001(unittest.TestCase):
    """KBI001: broad except without KeyboardInterrupt handler."""

    def _violations(self, code: str) -> list[Violation]:
        return check_file("<test>", code)

    def _codes(self, code: str) -> list[str]:
        return [v.code for v in self._violations(code)]

    def test_bare_except(self) -> None:
        code = """\
try:
    pass
except:
    pass
"""
        assert "KBI001" in self._codes(code)

    def test_except_exception(self) -> None:
        code = """\
try:
    pass
except Exception:
    pass
"""
        assert "KBI001" in self._codes(code)

    def test_except_base_exception(self) -> None:
        code = """\
try:
    pass
except BaseException:
    pass
"""
        assert "KBI001" in self._codes(code)

    def test_except_tuple_with_exception(self) -> None:
        code = """\
try:
    pass
except (ValueError, Exception):
    pass
"""
        assert "KBI001" in self._codes(code)


class TestKBI002(unittest.TestCase):
    """KBI002: KeyboardInterrupt handler missing required call."""

    def _violations(self, code: str) -> list[Violation]:
        return check_file("<test>", code)

    def _codes(self, code: str) -> list[str]:
        return [v.code for v in self._violations(code)]

    def test_kbi_handler_no_call(self) -> None:
        code = """\
try:
    pass
except KeyboardInterrupt:
    print("interrupted")
"""
        assert "KBI002" in self._codes(code)

    def test_kbi_handler_raise_only(self) -> None:
        code = """\
try:
    pass
except KeyboardInterrupt:
    raise
"""
        assert "KBI002" in self._codes(code)


class TestGoodCode(unittest.TestCase):
    """These should produce NO violations."""

    def _violations(self, code: str) -> list[Violation]:
        return check_file("<test>", code)

    def test_kbi_with_notify_and_broad(self) -> None:
        code = """\
try:
    pass
except KeyboardInterrupt:
    notify_main_thread()
    raise
except Exception:
    pass
"""
        assert self._violations(code) == []

    def test_kbi_with_thread_interrupt_main(self) -> None:
        code = """\
try:
    pass
except KeyboardInterrupt:
    _thread.interrupt_main()
    raise
except Exception:
    pass
"""
        assert self._violations(code) == []

    def test_kbi_with_handle_properly(self) -> None:
        code = """\
try:
    pass
except KeyboardInterrupt:
    handle_keyboard_interrupt_properly()
except Exception:
    pass
"""
        assert self._violations(code) == []

    def test_tuple_with_kbi_and_exception(self) -> None:
        code = """\
try:
    pass
except (KeyboardInterrupt, Exception):
    notify_main_thread()
"""
        assert self._violations(code) == []

    def test_specific_exception_only(self) -> None:
        code = """\
try:
    pass
except ValueError:
    pass
"""
        assert self._violations(code) == []

    def test_try_finally_no_except(self) -> None:
        code = """\
try:
    pass
finally:
    pass
"""
        assert self._violations(code) == []


class TestEdgeCases(unittest.TestCase):
    """Edge cases: nesting, multiple handlers, etc."""

    def _violations(self, code: str) -> list[Violation]:
        return check_file("<test>", code)

    def _codes(self, code: str) -> list[str]:
        return [v.code for v in self._violations(code)]

    def test_nested_inner_ok_outer_missing(self) -> None:
        """Inner try has KBI handler, outer try does not -> KBI001 on outer."""
        code = """\
try:
    try:
        pass
    except KeyboardInterrupt:
        notify_main_thread()
        raise
    except Exception:
        pass
except Exception:
    pass
"""
        codes = self._codes(code)
        assert "KBI001" in codes
        # The outer try at line 1 should be flagged
        lines = [v.line for v in self._violations(code) if v.code == "KBI001"]
        assert 1 in lines

    def test_kbi_handler_alone_missing_call(self) -> None:
        """KBI handler without broad catch, but missing required call -> KBI002."""
        code = """\
try:
    pass
except KeyboardInterrupt:
    print("bye")
"""
        assert "KBI002" in self._codes(code)


if __name__ == "__main__":
    unittest.main(verbosity=2)
