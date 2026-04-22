"""Regression tests for Meson private include path normalization."""

from __future__ import annotations

import json
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.meson.build_config import normalize_meson_private_include_paths


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
                build_dir / "ci/meson/native/fastled.so.p"
            ).resolve().as_posix()
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


if __name__ == "__main__":
    unittest.main()
