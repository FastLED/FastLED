"""Regression coverage for platform-specific Meson compiler selection."""

from __future__ import annotations

import os
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch

from ci.meson.meson_setup_execute import (
    _resolve_xcode_sdk_root,
    _resolve_xcode_tool,
    build_setup_env,
    detect_compiler_and_cache,
    write_meson_native_file,
)
from ci.meson.meson_setup_phases import CompilerDetection, MarkerPaths
from ci.meson.runner import _setup_sanitizer_env
from ci.meson.runner_helpers import _start_zccache_session


class TestMesonNativeToolchain(unittest.TestCase):
    @staticmethod
    def _compiler() -> CompilerDetection:
        return CompilerDetection(
            clang_wrapper="/venv/clang-tool-chain-c",
            clangxx_wrapper="/venv/clang-tool-chain-cpp",
            llvm_ar_wrapper="/venv/clang-tool-chain-ar",
            cache_binary=None,
            cache_name=None,
            compiler_version="clang 19",
            zccache_version="",
        )

    def test_xcode_tool_resolution_uses_running_process(self) -> None:
        completed = SimpleNamespace(
            returncode=0,
            stdout="/Applications/Xcode.app/usr/bin/clang\n",
            stderr="",
        )
        with patch(
            "ci.meson.meson_setup_execute.RunningProcess.run",
            return_value=completed,
        ) as run:
            path = _resolve_xcode_tool("clang")

        self.assertEqual(path, "/Applications/Xcode.app/usr/bin/clang")
        run.assert_called_once_with(
            ["xcrun", "--find", "clang"],
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )

    def test_xcode_tool_resolution_propagates_failure(self) -> None:
        completed = SimpleNamespace(returncode=1, stdout="", stderr="not found")
        with (
            patch(
                "ci.meson.meson_setup_execute.RunningProcess.run",
                return_value=completed,
            ),
            self.assertRaisesRegex(RuntimeError, "not found"),
        ):
            _resolve_xcode_tool("clang")

    def test_xcode_sdk_root_resolution_uses_running_process(self) -> None:
        completed = SimpleNamespace(
            returncode=0,
            stdout="/Applications/Xcode.app/SDKs/MacOSX.sdk\n",
            stderr="",
        )
        with patch(
            "ci.meson.meson_setup_execute.RunningProcess.run",
            return_value=completed,
        ) as run:
            path = _resolve_xcode_sdk_root()

        self.assertEqual(path, "/Applications/Xcode.app/SDKs/MacOSX.sdk")
        run.assert_called_once_with(
            ["xcrun", "--sdk", "macosx", "--show-sdk-path"],
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )

    def test_apple_debug_uses_one_xcode_toolchain(self) -> None:
        """Sanitized Apple objects and runtimes must have matching ASan ABIs."""
        compiler = self._compiler()
        xcode_tools = {
            "clang": "/Applications/Xcode.app/usr/bin/clang",
            "clang++": "/Applications/Xcode.app/usr/bin/clang++",
            "ar": "/Applications/Xcode.app/usr/bin/ar",
        }

        with TemporaryDirectory() as temp_dir:
            native_file = Path(temp_dir) / "native.ini"
            with (
                patch("ci.meson.meson_setup_execute.sys.platform", "darwin"),
                patch("ci.meson.meson_setup_execute.os.name", "posix"),
                patch(
                    "ci.meson.meson_setup_execute._resolve_xcode_tool",
                    side_effect=xcode_tools.__getitem__,
                ),
                patch(
                    "ci.meson.meson_setup_execute._resolve_xcode_sdk_root",
                    return_value="/Applications/Xcode.app/SDKs/MacOSX.sdk",
                ),
            ):
                write_meson_native_file(
                    native_file_path=native_file,
                    compiler=compiler,
                    build_mode="debug",
                )

            content = native_file.read_text(encoding="utf-8")

        self.assertIn("c = ['/Applications/Xcode.app/usr/bin/clang']", content)
        self.assertIn("cpp = ['/Applications/Xcode.app/usr/bin/clang++']", content)
        self.assertIn("ar = ['/Applications/Xcode.app/usr/bin/ar']", content)
        self.assertIn("system = 'darwin'", content)
        self.assertIn(
            "cpp_args = ['-isysroot', '/Applications/Xcode.app/SDKs/MacOSX.sdk']",
            content,
        )
        self.assertIn(
            "cpp_link_args = ['-isysroot', '/Applications/Xcode.app/SDKs/MacOSX.sdk']",
            content,
        )
        self.assertNotIn("clang-tool-chain-cpp", content)

    def test_apple_debug_ignores_stale_compiler_version_marker(self) -> None:
        """Migrating from Clang 19 must invalidate incompatible ASan objects."""
        with TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            markers = MarkerPaths.for_build_dir(temp_path)
            xcode_clangxx = temp_path / "Xcode" / "usr" / "bin" / "clang++"
            xcode_clangxx.parent.mkdir(parents=True)
            xcode_clangxx.touch()
            markers.compiler_version.write_text("clang 19", encoding="utf-8")
            newer_marker_time = xcode_clangxx.stat().st_mtime + 10
            os.utime(markers.compiler_version, (newer_marker_time, newer_marker_time))

            wrappers = {
                "clang-tool-chain-c": "/venv/clang-tool-chain-c",
                "clang-tool-chain-cpp": "/venv/clang-tool-chain-cpp",
                "clang-tool-chain-ar": "/venv/clang-tool-chain-ar",
            }
            with (
                patch("ci.meson.meson_setup_execute.sys.platform", "darwin"),
                patch(
                    "ci.meson.meson_setup_execute.shutil.which",
                    side_effect=wrappers.get,
                ),
                patch(
                    "ci.meson.meson_setup_execute._resolve_xcode_tool",
                    return_value=str(xcode_clangxx),
                ),
                patch(
                    "ci.meson.meson_setup_execute._find_zccache_binary",
                    return_value=None,
                ),
                patch(
                    "ci.meson.meson_setup_execute.get_compiler_version",
                    return_value="Apple clang 17.0.0",
                ) as get_version,
            ):
                detected = detect_compiler_and_cache(
                    markers, verbose=False, build_mode="debug"
                )

        self.assertEqual(detected.compiler_version, "Apple clang 17.0.0")
        get_version.assert_called_once_with(str(xcode_clangxx))

    def test_apple_debug_setup_environment_uses_xcode_tools(self) -> None:
        xcode_tools = {
            "clang": "/Xcode/usr/bin/clang",
            "clang++": "/Xcode/usr/bin/clang++",
            "ar": "/Xcode/usr/bin/ar",
        }
        with (
            patch("ci.meson.meson_setup_execute.sys.platform", "darwin"),
            patch(
                "ci.meson.meson_setup_execute._resolve_xcode_tool",
                side_effect=xcode_tools.__getitem__,
            ),
            patch(
                "ci.meson.meson_setup_execute._resolve_xcode_sdk_root",
                return_value="/Xcode/SDKs/MacOSX.sdk",
            ),
            patch(
                "ci.meson.meson_setup_execute._resolve_fast_compiler_binary"
            ) as resolve_fast,
        ):
            env = build_setup_env(self._compiler(), build_mode="debug")

        self.assertEqual(env["CC"], xcode_tools["clang"])
        self.assertEqual(env["CXX"], xcode_tools["clang++"])
        self.assertEqual(env["AR"], xcode_tools["ar"])
        self.assertEqual(env["SDKROOT"], "/Xcode/SDKs/MacOSX.sdk")
        resolve_fast.assert_not_called()

    def test_custom_pch_commands_inherit_native_cpp_args(self) -> None:
        project_root = Path(__file__).parents[2]
        for relative_path in (
            Path("ci/meson/native/meson.build"),
            Path("tests/meson.build"),
        ):
            with self.subTest(meson_file=relative_path.as_posix()):
                content = (project_root / relative_path).read_text(encoding="utf-8")
                self.assertIn("get_option('cpp_args')", content)

    def test_apple_debug_disables_unsupported_leak_detection(self) -> None:
        import clang_tool_chain

        prepared = {
            "ASAN_OPTIONS": "fast_unwind_on_malloc=0:symbolize=1:detect_leaks=1",
            "LSAN_OPTIONS": "symbolize=1",
        }
        with (
            TemporaryDirectory() as temp_dir,
            patch("ci.meson.runner.sys.platform", "darwin"),
            patch.dict(
                clang_tool_chain.__dict__,
                {"prepare_sanitizer_environment": lambda **_kwargs: prepared},
            ),
            patch.dict(os.environ, {}, clear=True),
        ):
            source_dir = Path(temp_dir)
            suppressions = source_dir / "tests" / "lsan_suppressions.txt"
            suppressions.parent.mkdir()
            suppressions.write_text("leak:known_false_positive\n", encoding="utf-8")
            _setup_sanitizer_env(source_dir, verbose=False)
            options = os.environ["ASAN_OPTIONS"].split(":")
            lsan_options = os.environ["LSAN_OPTIONS"]

        self.assertIn("fast_unwind_on_malloc=0", options)
        self.assertIn("symbolize=1", options)
        self.assertIn("detect_leaks=0", options)
        self.assertNotIn("detect_leaks=1", options)
        self.assertNotIn("suppressions=", lsan_options)

    def test_apple_non_debug_keeps_clang_tool_chain(self) -> None:
        compiler = self._compiler()
        with TemporaryDirectory() as temp_dir:
            native_file = Path(temp_dir) / "native.ini"
            with (
                patch("ci.meson.meson_setup_execute.sys.platform", "darwin"),
                patch("ci.meson.meson_setup_execute.os.name", "posix"),
                patch(
                    "ci.meson.meson_setup_execute._resolve_fast_native_entries",
                    return_value=SimpleNamespace(cc=None, cxx=None, ar=None),
                ),
                patch(
                    "ci.meson.meson_setup_execute._resolve_xcode_tool"
                ) as resolve_xcode,
            ):
                write_meson_native_file(
                    native_file_path=native_file,
                    compiler=compiler,
                    build_mode="quick",
                )

            content = native_file.read_text(encoding="utf-8")

        self.assertIn("c = ['/venv/clang-tool-chain-c']", content)
        self.assertIn("cpp = ['/venv/clang-tool-chain-cpp']", content)
        resolve_xcode.assert_not_called()

    def test_apple_debug_disables_clang_tool_chain_runtime_deployer(self) -> None:
        with (
            patch("ci.meson.runner_helpers.sys.platform", "darwin"),
            patch("ci.meson.runner_helpers._find_zccache_binary", return_value=None),
            patch.dict(
                os.environ,
                {"ZCCACHE_LINK_DEPLOY_CMD": "clang-tool-chain-libdeploy"},
                clear=False,
            ),
        ):
            _start_zccache_session(Path("unused"), build_mode="debug")
            self.assertNotIn("ZCCACHE_LINK_DEPLOY_CMD", os.environ)

    def test_apple_non_debug_keeps_runtime_deployer(self) -> None:
        with (
            patch("ci.meson.runner_helpers.sys.platform", "darwin"),
            patch("ci.meson.runner_helpers._find_zccache_binary", return_value=None),
            patch.dict(os.environ, {}, clear=True),
        ):
            _start_zccache_session(Path("unused"), build_mode="quick")
            self.assertEqual(
                os.environ["ZCCACHE_LINK_DEPLOY_CMD"],
                "clang-tool-chain-libdeploy",
            )


if __name__ == "__main__":
    unittest.main()
