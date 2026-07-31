"""Tests for the PlatformIO-internal-usage checker (#2701).

The checker gates internal PlatformIO shell-outs. It was promoted from
warn-only to error mode, which makes two properties load-bearing:

  1. It must still CATCH real invocations — otherwise the green gate is
     vacuous. Notably it must catch argv-list forms like ``["pio", "run"]``,
     which the original line-regex missed entirely.
  2. It must NOT catch prose. Docstrings, ``help=`` text, trailing comments
     and backtick-quoted commands describe the rule rather than violating
     it, and flagging them pushes authors to mangle documentation to appease
     a linter.

These tests drive ``NoInternalPlatformIOChecker`` directly so no repo walk
or real PlatformIO install is involved.
"""

from __future__ import annotations

import unittest

from ci.lint_platformio.check_no_internal_platformio import (
    MIGRATION_DEBT,
    SANCTIONED_PLATFORMIO_SURFACE,
    NoInternalPlatformIOChecker,
)
from ci.util.check_files import FileContent


def _violations(path: str, src: str) -> list[tuple[int, str]]:
    """Run the checker over in-memory source; return its violation list."""
    checker = NoInternalPlatformIOChecker()
    if not checker.should_process_file(path):
        return []
    content = FileContent(path=path, content=src, lines=src.splitlines())
    checker.check_file_content(content)
    return checker.violations.get(path, [])


class TestRealInvocationsAreCaught(unittest.TestCase):
    """The gate must actually bite."""

    def test_yaml_run_step(self) -> None:
        src = "steps:\n  - run: uv run platformio run\n"
        self.assertEqual(len(_violations("ci/x.yml", src)), 1)

    def test_yaml_chained_shell_command(self) -> None:
        src = "steps:\n  - run: cd ci/kitchensink && pio run\n"
        self.assertEqual(len(_violations("ci/x.yml", src)), 1)

    def test_version_probe(self) -> None:
        src = "steps:\n  - run: uv run platformio --version\n"
        self.assertEqual(len(_violations("ci/x.yml", src)), 1)

    def test_backend_flag_in_shell(self) -> None:
        src = "steps:\n  - run: ci-compile.py --backend platformio\n"
        self.assertEqual(len(_violations("ci/x.yml", src)), 1)

    def test_argv_list_is_caught(self) -> None:
        """Regression: the original line-regex missed every argv form."""
        src = 'cmd = ["pio", "run", "-d", str(build_dir), "-t", "size"]\n'
        violations = _violations("ci/x.py", src)
        self.assertEqual(len(violations), 1)
        self.assertIn("pio_run", violations[0][1])

    def test_flag_as_exact_string_literal(self) -> None:
        """An argparse flag definition is a real acceptance point."""
        for literal in ('"--platformio",', '"--pio",'):
            with self.subTest(literal=literal):
                self.assertEqual(len(_violations("ci/x.py", literal + "\n")), 1)

    def test_flag_passed_in_arg_list(self) -> None:
        src = 'args = ["esp32s3", "--examples", example, "--platformio"]\n'
        self.assertEqual(len(_violations("ci/x.py", src)), 1)


class TestProseIsNotCaught(unittest.TestCase):
    """Documentation describing the rule must not trip it."""

    def test_single_line_docstring(self) -> None:
        src = '"""Backend selection (fbuild vs PlatformIO\'s `pio run`)."""\n'
        self.assertEqual(_violations("ci/x.py", src), [])

    def test_multi_line_docstring(self) -> None:
        src = (
            '"""Measure opt-ins.\n'
            "\n"
            "On Linux/macOS run `bash compile esp32s3 --examples X\n"
            "--platformio` to reproduce. The PlatformIO `pio run` backend\n"
            "is comparison-only.\n"
            '"""\n'
        )
        self.assertEqual(_violations("ci/x.py", src), [])

    def test_trailing_comment(self) -> None:
        src = "ok = False  # some boards only work with pio run\n"
        self.assertEqual(_violations("ci/x.py", src), [])

    def test_ini_semicolon_comment(self) -> None:
        src = "; This file is consumed by `bash debug` (and raw `pio run`).\n"
        self.assertEqual(_violations("ci/x.ini", src), [])

    def test_help_text_mentioning_flag(self) -> None:
        src = 'p.add_argument("--backend", help="Use --platformio to compare.")\n'
        self.assertEqual(_violations("ci/x.py", src), [])

    def test_prose_string_containing_command(self) -> None:
        src = 'label = "fbuild" if use_fbuild else "platformio (pio run)"\n'
        self.assertEqual(_violations("ci/x.py", src), [])

    def test_backtick_quoted_command_in_yaml_prose(self) -> None:
        src = "# note\ndescription: reference for raw `pio run` invocations.\n"
        self.assertEqual(_violations("ci/x.yml", src), [])


class TestAllowlisting(unittest.TestCase):
    def test_sanctioned_surface_is_skipped(self) -> None:
        src = 'cmd = ["pio", "run", "--project-dir", str(d)]\n'
        # Same source, sanctioned path vs. an arbitrary one.
        self.assertEqual(_violations("ci/compiler/pio.py", src), [])
        self.assertEqual(len(_violations("ci/compiler/other.py", src)), 1)

    def test_migration_debt_is_skipped(self) -> None:
        src = "steps:\n  - run: cd ci/kitchensink && pio run\n"
        path = ".github/workflows/build_esp_extra_libs.yml"
        self.assertEqual(_violations(path, src), [])

    def test_every_debt_entry_carries_a_reason(self) -> None:
        """Debt must be justified and tracked, not silently parked."""
        for path, reason in MIGRATION_DEBT:
            with self.subTest(path=path):
                self.assertTrue(reason.strip(), f"{path} has no stated reason")
                self.assertIn("#", reason, f"{path} cites no tracking issue")

    def test_sanctioned_and_debt_lists_are_disjoint(self) -> None:
        debt_paths = {path for path, _reason in MIGRATION_DEBT}
        self.assertEqual(debt_paths & set(SANCTIONED_PLATFORMIO_SURFACE), set())


class TestFileSelection(unittest.TestCase):
    def test_cpp_sources_are_not_scanned(self) -> None:
        """Board source never drives PlatformIO; scanning it only adds noise."""
        self.assertEqual(_violations("src/platforms/esp/pio_run.cpp", "pio run\n"), [])


if __name__ == "__main__":
    unittest.main()
