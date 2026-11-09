"""FASTLED_NATIVE_CLANG=soldr selects soldr's managed clang (soldr#4127)."""

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch

from mesonbuild.machinefile import parse_machine_files

from ci.meson import native_launchers
from ci.meson.meson_setup_execute import build_setup_env
from ci.meson.meson_setup_phases import CompilerDetection
from ci.meson.native_launchers import (
    SOLDR_CLANG_ENV,
    _resolve_fast_compiler_binary,
    _resolve_fast_native_entries,
    resolve_soldr_clang,
    resolve_soldr_linker,
)


@unittest.skipUnless(sys.platform.startswith("linux"), "soldr clang gate is Linux-only")
class TestSoldrClangGate(unittest.TestCase):
    """Exercise native compiler selection and fail-closed tool discovery."""

    def test_unset_keeps_clang_tool_chain(self) -> None:
        """Keep the existing toolchain unless the Linux opt-in is set."""
        env: dict[str, str] = {
            k: v for k, v in os.environ.items() if k != SOLDR_CLANG_ENV
        }
        with patch.dict(os.environ, env, clear=True):
            self.assertIsNone(resolve_soldr_clang())
            self.assertIsNone(resolve_soldr_linker())

    def test_opt_in_uses_soldr_print_paths(self) -> None:
        """Preserve tool paths through Meson parsing and setup environment."""
        with TemporaryDirectory() as tmp:
            bundle = Path(tmp) / "llvm's\\bundle"
            bundle.mkdir()
            tools: dict[str, str] = {}
            for flag, name in (
                ("--print-cc", "clang"),
                ("--print-cxx", "clang++"),
                ("--print-ar", "llvm-ar"),
                ("--print-linker", "bridge-linker"),
            ):
                path = bundle / name
                path.write_text("", encoding="utf-8")
                tools[flag] = str(path)

            def fake_run(cmd: list[str], **_: object) -> SimpleNamespace:
                """Return real fixture paths using the soldr print interface."""
                self.assertEqual(cmd[1:3], ["c++", "--clang"])
                timeout = _.get("timeout")
                self.assertTrue(
                    isinstance(timeout, (int, float)) and 0 < timeout <= 600
                )
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
                native_file = Path(tmp) / "native.ini"
                native_file.write_text(
                    f"[binaries]\nc = {entries.cc}\ncpp = {entries.cxx}\nar = {entries.ar}\n",
                    encoding="utf-8",
                )
                parsed = parse_machine_files([str(native_file)], tmp)["binaries"]
                self.assertEqual(parsed["c"], [tools["--print-cc"]])
                self.assertEqual(parsed["cpp"], [tools["--print-cxx"]])
                self.assertEqual(parsed["ar"], [tools["--print-ar"]])
                self.assertEqual(_resolve_fast_compiler_binary(), tools["--print-cxx"])
                self.assertEqual(resolve_soldr_linker(), tools["--print-linker"])
                compiler = CompilerDetection("", "", "", None, None, "", "")
                env = build_setup_env(compiler, build_mode="quick")
                self.assertEqual(env["CC"], tools["--print-cc"])
                self.assertEqual(env["CXX"], tools["--print-cxx"])
                self.assertEqual(env["AR"], tools["--print-ar"])

    def test_opt_in_without_soldr_fails_loudly(self) -> None:
        """Do not silently select another compiler when soldr is missing."""
        with (
            patch.dict(os.environ, {SOLDR_CLANG_ENV: "soldr"}),
            patch.object(native_launchers.shutil, "which", return_value=None),
        ):
            with self.assertRaises(RuntimeError):
                resolve_soldr_clang()

    def test_invalid_soldr_output_fails_loudly(self) -> None:
        """Reject failed queries and paths that do not name a regular file."""
        with TemporaryDirectory() as tmp:
            executable = Path(tmp) / "clang"
            executable.touch()
            for returncode, output in ((1, str(executable)), (0, tmp), (0, "")):
                with (
                    self.subTest(returncode=returncode, output=output),
                    patch.dict(os.environ, {SOLDR_CLANG_ENV: "soldr"}),
                    patch.object(
                        native_launchers.shutil, "which", return_value="/bin/soldr"
                    ),
                    patch.object(
                        native_launchers.RunningProcess,
                        "run",
                        return_value=SimpleNamespace(
                            returncode=returncode, stdout=output, stderr="query failed"
                        ),
                    ),
                ):
                    with self.assertRaisesRegex(RuntimeError, "failed"):
                        resolve_soldr_clang()
                    with self.assertRaisesRegex(RuntimeError, "failed"):
                        resolve_soldr_linker()


if __name__ == "__main__":
    unittest.main()
