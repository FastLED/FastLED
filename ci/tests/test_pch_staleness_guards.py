"""Regression tests for the two PCH-poisoning guards.

Both cover the same failure: a PCH built under one include spelling is reused
under another, clang re-parses headers the PCH already contains, ``#pragma
once`` fails to dedupe, and the build dies with "redefinition of ...".
"""

from __future__ import annotations

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.compile_pch import hash_compile_flags
from ci.meson.path_normalization import find_ninja_strict_path_violations


BASE_ARGS = [
    "clang++.exe",
    "-c",
    "-x",
    "c++-header",
    "C:/x/tests/test_pch.h",
    "-o",
    "tests/test_pch.h.pch",
    "-IC:/x/src",
]


class TestCompileFlagsHash(unittest.TestCase):
    def test_identical_args_hash_identically(self) -> None:
        self.assertEqual(hash_compile_flags(BASE_ARGS), hash_compile_flags(BASE_ARGS))

    def test_include_separator_spelling_changes_the_hash(self) -> None:
        """The whole point: the two spellings must NOT collapse together.

        The resolved dependency set is identical either way, so the input-file
        hash cannot tell these apart. If this hash also cannot, the stale PCH
        is reused and poisons the build.
        """
        backslashed = [a.replace("-IC:/x/src", "-IC:\\x\\src") for a in BASE_ARGS]
        self.assertNotEqual(
            hash_compile_flags(BASE_ARGS), hash_compile_flags(backslashed)
        )

    def test_added_include_root_changes_the_hash(self) -> None:
        self.assertNotEqual(
            hash_compile_flags(BASE_ARGS),
            hash_compile_flags([*BASE_ARGS, "-IC:/x/tests"]),
        )

    def test_arg_boundaries_are_unambiguous(self) -> None:
        """['-IC:/a', '-IC:/b'] must not hash like ['-IC:/a-IC:/b']."""
        self.assertNotEqual(
            hash_compile_flags(["-IC:/a", "-IC:/b"]),
            hash_compile_flags(["-IC:/a-IC:/b"]),
        )


class TestNinjaStrictPathViolations(unittest.TestCase):
    def _ninja(self, tmp: str, command: str) -> Path:
        build_dir = Path(tmp)
        (build_dir / "build.ninja").write_text(
            f"rule CUSTOM_COMMAND\n  COMMAND = {command}\n", encoding="utf-8"
        )
        return build_dir

    def test_clean_pch_command_is_accepted(self) -> None:
        with TemporaryDirectory() as tmp:
            build_dir = self._ninja(
                tmp, '"python" "C:/x/ci/compile_pch.py" "-IC:/x/src"'
            )
            self.assertEqual(find_ninja_strict_path_violations(build_dir), [])

    def test_backslash_include_in_pch_command_is_caught(self) -> None:
        """This is the case compile_commands.json structurally cannot see.

        The PCH is a Meson custom target, so its build command exists only in
        build.ninja -- the consuming -include-pch flags are validated but the
        flags it was built with were not.
        """
        with TemporaryDirectory() as tmp:
            build_dir = self._ninja(
                tmp, '"python" "C:/x/ci/compile_pch.py" "-IC:\\x\\src"'
            )
            self.assertTrue(find_ninja_strict_path_violations(build_dir))

    def test_relative_include_is_caught(self) -> None:
        with TemporaryDirectory() as tmp:
            build_dir = self._ninja(tmp, '"python" "compile_pch.py" "-Isrc"')
            self.assertTrue(find_ninja_strict_path_violations(build_dir))

    def test_dot_components_are_caught(self) -> None:
        with TemporaryDirectory() as tmp:
            build_dir = self._ninja(tmp, '"python" "x.py" "-IC:/x/tests/../src"')
            self.assertTrue(find_ninja_strict_path_violations(build_dir))

    def test_missing_build_ninja_is_not_a_violation(self) -> None:
        with TemporaryDirectory() as tmp:
            self.assertEqual(find_ninja_strict_path_violations(Path(tmp)), [])


if __name__ == "__main__":
    unittest.main()
