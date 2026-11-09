"""FASTLED_NATIVE_CLANG=soldr selects soldr's managed clang (soldr#4127)."""

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch

from ci.meson import native_launchers
from ci.meson.native_launchers import (
    SOLDR_CLANG_ENV,
    _resolve_fast_compiler_binary,
    _resolve_fast_native_entries,
    resolve_soldr_clang,
)


@unittest.skipUnless(sys.platform.startswith("linux"), "soldr clang gate is Linux-only")
class TestSoldrClangGate(unittest.TestCase):
    def test_unset_keeps_clang_tool_chain(self) -> None:
        env = {k: v for k, v in os.environ.items() if k != SOLDR_CLANG_ENV}
        with patch.dict(os.environ, env, clear=True):
            self.assertIsNone(resolve_soldr_clang())

    def test_opt_in_uses_soldr_print_paths(self) -> None:
        with TemporaryDirectory() as tmp:
            tools = {}
            for flag, name in (
                ("--print-cc", "clang"),
                ("--print-cxx", "clang++"),
                ("--print-ar", "llvm-ar"),
            ):
                path = Path(tmp) / name
                path.write_text("", encoding="utf-8")
                tools[flag] = str(path)

            def fake_run(cmd: list[str], **_: object) -> SimpleNamespace:
                self.assertEqual(cmd[1:3], ["c++", "--clang"])
                return SimpleNamespace(
                    returncode=0, stdout=tools[cmd[3]] + "\n", stderr=""
                )

            with (
                patch.dict(os.environ, {SOLDR_CLANG_ENV: "soldr"}),
                patch.object(
                    native_launchers.shutil, "which", return_value="/bin/soldr"
                ),
                patch.object(
                    native_launchers.RunningProcess, "run", side_effect=fake_run
                ),
            ):
                entries = _resolve_fast_native_entries()
                self.assertEqual(entries.cxx, f"['{tools['--print-cxx']}']")
                self.assertEqual(entries.ar, f"['{tools['--print-ar']}']")
                self.assertEqual(_resolve_fast_compiler_binary(), tools["--print-cxx"])

    def test_opt_in_without_soldr_fails_loudly(self) -> None:
        with (
            patch.dict(os.environ, {SOLDR_CLANG_ENV: "soldr"}),
            patch.object(native_launchers.shutil, "which", return_value=None),
        ):
            with self.assertRaises(RuntimeError):
                resolve_soldr_clang()


if __name__ == "__main__":
    unittest.main()
