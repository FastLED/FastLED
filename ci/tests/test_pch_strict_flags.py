"""Tests for ci/lint/pch_strict_flags.py (strict PCH validation, no -fno-pch-timestamp)."""

from __future__ import annotations

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from running_process import RunningProcess

from ci.lint.pch_strict_flags import (
    PROJECT_ROOT,
    check_meson,
    check_python,
    run_pch_strict_flags_lint,
    scan,
)


MESON_PRODUCER = """
pch = custom_target('pch',
  command: [
    'compile_pch.py', '-x', 'c++-header', '@INPUT@', '-o', '@OUTPUT@',
    '-Werror=invalid-pch',
    '-verify-pch',
    '-fpch-validate-input-files-content',
  ],
)
"""

MESON_CONSUMER = """
args = [
  '-include-pch', 'test_pch.h.pch',
  '-Werror=invalid-pch',
  '-fpch-validate-input-files-content',
]
"""

PY_CONSUMER = """
def compile_object(pch_file):
    cmd = ["clang++", "-c", "-include-pch", str(pch_file),
           "-Werror=invalid-pch", "-fpch-validate-input-files-content"]
    return cmd
"""


class TestMeson(unittest.TestCase):
    def test_strict_producer_and_consumer_pass(self) -> None:
        self.assertEqual(check_meson(MESON_PRODUCER + MESON_CONSUMER), [])

    def test_each_producer_flag_is_required(self) -> None:
        for flag in (
            "-Werror=invalid-pch",
            "-verify-pch",
            "-fpch-validate-input-files-content",
        ):
            text = MESON_PRODUCER.replace(f"'{flag}',", "")
            hits = check_meson(text)
            self.assertEqual(len(hits), 1, flag)
            self.assertIn(flag, hits[0][1])
            self.assertIn("producer", hits[0][1])

    def test_each_consumer_flag_is_required(self) -> None:
        for flag in ("-Werror=invalid-pch", "-fpch-validate-input-files-content"):
            hits = check_meson(MESON_CONSUMER.replace(f"'{flag}',", ""))
            self.assertEqual(len(hits), 1, flag)
            self.assertIn("consumer", hits[0][1])

    def test_flag_only_in_a_comment_does_not_count(self) -> None:
        text = MESON_CONSUMER.replace(
            "'-fpch-validate-input-files-content',",
            "# '-fpch-validate-input-files-content',",
        )
        self.assertEqual(len(check_meson(text)), 1)


class TestMesonComments(unittest.TestCase):
    def test_hash_inside_a_string_is_not_a_comment(self) -> None:
        hits = check_meson("args = ['-DREV=#42', '-include-pch', 'x.pch']\n")
        self.assertEqual(len(hits), 1)
        self.assertIn("consumer", hits[0][1])

    def test_trailing_comment_after_strings_is_stripped(self) -> None:
        text = "args = ['-include-pch', 'x.pch', '-Werror=invalid-pch'] # '-fpch-validate-input-files-content'\n"
        self.assertEqual(len(check_meson(text)), 1)


class TestPython(unittest.TestCase):
    def test_strict_consumer_passes(self) -> None:
        self.assertEqual(check_python(PY_CONSUMER), [])

    def test_consumer_missing_flag_fails(self) -> None:
        hits = check_python(
            PY_CONSUMER.replace(', "-fpch-validate-input-files-content"', "")
        )
        self.assertEqual(len(hits), 1)
        self.assertIn("compile_object()", hits[0][1])

    def test_module_level_flag_list_counts(self) -> None:
        text = (
            'USE = ["-Werror=invalid-pch", "-fpch-validate-input-files-content"]\n'
            "def tu(pch):\n"
            '    return ["clang++", *USE, "-include-pch", str(pch)]\n'
        )
        self.assertEqual(check_python(text), [])

    def test_producer_needs_all_three(self) -> None:
        text = (
            "def build(PCH_OUT):\n"
            '    return ["emcc", "-x", "c++-header", "h.h", "-o", str(PCH_OUT),\n'
            '            "-Werror=invalid-pch", "-verify-pch"]\n'
        )
        hits = check_python(text)
        self.assertEqual(len(hits), 1)
        self.assertIn("-fpch-validate-input-files-content", hits[0][1])

    def test_header_analysis_without_pch_is_not_a_producer(self) -> None:
        # IWYU-style: -x c++-header on a header, no PCH involved.
        text = (
            'def iwyu(h):\n    return ["include-what-you-use", "-x", "c++-header", h]\n'
        )
        self.assertEqual(check_python(text), [])

    def test_comparisons_and_module_tables_are_not_commands(self) -> None:
        text = (
            'FLAGS = ("-include", "-include-pch")\n'
            "def strip(args):\n"
            '    return [a for a in args if a != "-include-pch"]\n'
        )
        self.assertEqual(check_python(text), [])


class TestScan(unittest.TestCase):
    def test_real_tree_is_clean(self) -> None:
        self.assertEqual(scan(PROJECT_ROOT), [])
        self.assertTrue(run_pch_strict_flags_lint(PROJECT_ROOT))

    def test_fno_pch_timestamp_is_banned_everywhere(self) -> None:
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            RunningProcess.run(["git", "init", "-q"], cwd=str(root), check=True)
            (root / "ci").mkdir()
            (root / "meson.build").write_text(
                MESON_PRODUCER.replace(
                    "'-verify-pch',", "'-verify-pch', '-Xclang', '-fno-pch-timestamp',"
                ),
                encoding="utf-8",
            )
            (root / "ci" / "build.py").write_text(
                'FLAGS = ["-Xclang", "-fno-pch-timestamp"]\n', encoding="utf-8"
            )
            violations = scan(root)
            banned = [
                v for v in violations if "-fno-pch-timestamp is banned" in v.message
            ]
            self.assertEqual(
                sorted(v.path for v in banned), ["ci/build.py", "meson.build"]
            )
            self.assertFalse(run_pch_strict_flags_lint(root))


if __name__ == "__main__":
    unittest.main()
