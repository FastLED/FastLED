"""Regression tests for Meson private include path normalization."""

from __future__ import annotations

import json
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.meson.build_config import (
    find_strict_path_violations,
    normalize_meson_private_include_paths,
)


class TestMesonPrivateIncludeNormalization(unittest.TestCase):
    def test_normalizes_quoted_and_unquoted_private_includes(self) -> None:
        with TemporaryDirectory() as temp_dir:
            build_dir = Path(temp_dir)
            build_ninja = build_dir / "build.ninja"
            compile_commands = build_dir / "compile_commands.json"

            build_ninja.write_text(
                "build obj: cpp\n"
                ' command = zccache clang++ -Ici/meson/native/fastled.so.p "-Itests/test.dll.p"\n',
                encoding="utf-8",
            )
            compile_commands.write_text(
                json.dumps(
                    [
                        {
                            "directory": str(build_dir),
                            "command": 'clang++ -Ici/meson/native/fastled.so.p "-Itests/test.dll.p"',
                            "file": "src/main.cpp",
                        }
                    ]
                ),
                encoding="utf-8",
            )

            self.assertTrue(normalize_meson_private_include_paths(build_dir))

            native_private = (
                (build_dir / "ci/meson/native/fastled.so.p").resolve().as_posix()
            )
            test_private = (build_dir / "tests/test.dll.p").resolve().as_posix()

            normalized_ninja = build_ninja.read_text(encoding="utf-8")
            self.assertIn(f"-I{native_private}", normalized_ninja)
            self.assertIn(f'"-I{test_private}"', normalized_ninja)
            self.assertNotIn("-Ici/meson/native/fastled.so.p", normalized_ninja)
            self.assertNotIn('"-Itests/test.dll.p"', normalized_ninja)

            normalized_commands = json.loads(
                compile_commands.read_text(encoding="utf-8")
            )
            command = normalized_commands[0]["command"]
            self.assertIn(f"-I{native_private}", command)
            self.assertIn(f'"-I{test_private}"', command)

    def test_normalizes_non_dot_p_and_attached_strict_path_flags(self) -> None:
        """Issue #2378: all path-bearing flags (not just ``-I*.p``) must be
        rewritten to absolute forward-slash form with no dot components."""
        with TemporaryDirectory() as temp_dir:
            build_dir = Path(temp_dir)
            compile_commands = build_dir / "compile_commands.json"

            offending = (
                "clang++ "
                # implicit_include_directories=true emits target subdir + project src dir
                '"-I." "-I..\\..\\src" "-I..\\..\\tests" '
                # subdir() emits relative + backslash spellings
                '"-Ici/meson/native" "-I..\\..\\ci\\meson\\native" '
                # absolute path with /. component
                '"-IC:/Users/niteris/dev/fastled7/." '
                # other strict-path flag families
                '"-isystem..\\..\\third_party/include" '
                '"-iquotetests" '
                '"-include..\\..\\src/forced.h" '
                '"-include-pchci/meson/native/pch.h.pch" '
                '"-imacros..\\..\\src/macros.h" '
                # forward-slash absolute with no dot components — already canonical
                "-IC:/already/canonical"
            )
            compile_commands.write_text(
                json.dumps(
                    [
                        {
                            "directory": str(build_dir),
                            "command": offending,
                            "file": "src/main.cpp",
                        }
                    ]
                ),
                encoding="utf-8",
            )

            self.assertTrue(normalize_meson_private_include_paths(build_dir))
            data = json.loads(compile_commands.read_text(encoding="utf-8"))
            normalized = data[0]["command"]

            self.assertNotIn("\\", normalized)
            for offender in (
                '"-I."',
                '"-Ici/meson/native"',
                '"-IC:/Users/niteris/dev/fastled7/."',
                '"-iquotetests"',
            ):
                self.assertNotIn(offender, normalized)

            # Already-canonical absolute flag must be left untouched.
            self.assertIn("-IC:/already/canonical", normalized)

    def test_does_not_rewrite_bare_include_pch_token(self) -> None:
        """The bare ``-include-pch`` flag (with the path in the next token) must
        not be miscaptured by falling back from the ``-include-pch`` prefix to
        the shorter ``-include`` prefix + treating ``-pch`` as the attached path.
        This was the regression that produced ``-includeC:/.../-pch`` artifacts.
        """
        with TemporaryDirectory() as temp_dir:
            build_dir = Path(temp_dir)
            compile_commands = build_dir / "compile_commands.json"
            original = (
                'clang++ "-include-pch" "C:/abs/forward/slash/pch.h.pch" '
                '"-include-pchC:/attached/pch.h.pch" -c foo.cpp'
            )
            compile_commands.write_text(
                json.dumps(
                    [
                        {
                            "directory": str(build_dir),
                            "command": original,
                            "file": "foo.cpp",
                        }
                    ]
                ),
                encoding="utf-8",
            )

            normalize_meson_private_include_paths(build_dir)
            normalized = json.loads(compile_commands.read_text(encoding="utf-8"))[0][
                "command"
            ]

            self.assertIn('"-include-pch"', normalized)
            self.assertNotIn("-includeC:", normalized)
            self.assertNotIn("/-pch", normalized)


class TestFindStrictPathViolations(unittest.TestCase):
    """Issue #2378 — pre-compile gate that mirrors the zccache strict check."""

    def test_returns_empty_for_missing_compile_commands(self) -> None:
        with TemporaryDirectory() as temp_dir:
            self.assertEqual(find_strict_path_violations(Path(temp_dir)), [])

    def test_returns_empty_for_canonical_paths(self) -> None:
        with TemporaryDirectory() as temp_dir:
            build_dir = Path(temp_dir)
            (build_dir / "compile_commands.json").write_text(
                json.dumps(
                    [
                        {
                            "directory": str(build_dir),
                            "command": (
                                'clang++ "-IC:/a/b" "-isystemC:/x/y" '
                                '"-iquoteC:/p/q" -c foo.cpp'
                            ),
                            "file": "foo.cpp",
                        }
                    ]
                ),
                encoding="utf-8",
            )
            self.assertEqual(find_strict_path_violations(build_dir), [])

    def test_reports_backslash_relative_and_dot_paths(self) -> None:
        with TemporaryDirectory() as temp_dir:
            build_dir = Path(temp_dir)
            (build_dir / "compile_commands.json").write_text(
                json.dumps(
                    [
                        {
                            "directory": str(build_dir),
                            "command": (
                                'clang++ "-I..\\..\\src" "-Itests" '
                                '"-IC:/Users/x/." "-isystemC:/abs/canonical" '
                                "-c foo.cpp"
                            ),
                            "file": "foo.cpp",
                        }
                    ]
                ),
                encoding="utf-8",
            )
            violations = find_strict_path_violations(build_dir)
            self.assertEqual(len(violations), 3)


if __name__ == "__main__":
    unittest.main()
