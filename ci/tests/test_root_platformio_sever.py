"""Tests for the root-platformio.ini sever + tripwire on CI compile (#3278/#3279).

The legacy bridge that appended ``[env:<board>].build_flags`` from the
repo-root ``platformio.ini`` into the synthesised env was severed in
#3278 (opt-in default ``False``). #3279 Phase 4 removed the opt-in
parameter entirely because no production caller ever passed ``True`` —
the synthesised env now NEVER inherits root flags.

What remains is the ``FASTLED_FAIL_ON_ROOT_MERGE=1`` tripwire: CI
workflows set this env var, and ``_init_platformio_build`` probes the
root ini and raises if any flag would historically have been merged.
The tripwire catches per-board flags someone added to root but forgot
to port to ``ci/boards.py``.

These tests exercise both behaviours directly against
``_init_platformio_build`` so we don't have to spin up real PlatformIO.
"""

from __future__ import annotations

import os
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import mock

from ci.boards import Board
from ci.compiler import pio as pio_module
from ci.compiler.path_manager import FastLEDPaths


def _make_paths(tmp: Path) -> FastLEDPaths:
    """Build a FastLEDPaths object rooted under ``tmp`` for testing."""
    paths = FastLEDPaths("esp32c6")
    # Steer every cache/output path into the tempdir so this test never
    # touches the user's real ~/.fastled.
    paths._global_platformio_cache_dir = tmp / "pio_cache"
    return paths


def _write_root_ini_with_esp32c6_flag(root: Path, flag: str) -> None:
    """Write a minimal root platformio.ini exposing one esp32c6 flag."""
    (root / "platformio.ini").write_text(
        f"[env:esp32c6]\nplatform = espressif32\nbuild_flags = {flag}\n",
    )


class TestRootPlatformioSever(unittest.TestCase):
    """Verify the sever + tripwire semantics on _init_platformio_build."""

    def _run_init(
        self,
        tmp: Path,
        fail_on_root_merge: bool,
        captured_flags: list[list[str]],
    ) -> object:
        """Drive ``_init_platformio_build`` with the project root patched
        to ``tmp`` and all heavyweight side effects mocked out.

        ``captured_flags`` collects the final ``build_flags`` that
        ``apply_board_specific_config`` would have written, so each test
        can assert what reached the synthesised env.
        """
        env_patch: dict[str, str] = {}
        if fail_on_root_merge:
            env_patch["FASTLED_FAIL_ON_ROOT_MERGE"] = "1"
        else:
            # Ensure inherited env from the host doesn't accidentally
            # turn the tripwire on for the probe=off case.
            env_patch["FASTLED_FAIL_ON_ROOT_MERGE"] = ""

        def fake_apply(
            board: Board,
            platformio_ini_path: Path,
            example: str,
            paths: FastLEDPaths,
            additional_defines: object = None,
            additional_include_dirs: object = None,
            additional_libs: object = None,
        ) -> bool:
            # Snapshot the final flag list so the test can inspect it.
            captured_flags.append(list(board.build_flags or []))
            platformio_ini_path.write_text("[platformio]\n")
            return True

        board = Board(
            board_name="esp32c6",
            real_board_name="esp32-c6-devkitc-1",
            platform="espressif32",
            framework="arduino",
        )
        paths = _make_paths(tmp)
        build_dir = tmp / "build"

        with (
            mock.patch.dict(os.environ, env_patch, clear=False),
            mock.patch.object(pio_module, "resolve_project_root", return_value=tmp),
            mock.patch.object(
                pio_module, "apply_board_specific_config", side_effect=fake_apply
            ),
            mock.patch.object(pio_module, "copy_example_source", return_value=True),
            mock.patch.object(pio_module, "copy_fastled_library", return_value=True),
            mock.patch.object(pio_module, "copy_boards_directory", return_value=True),
        ):
            return pio_module._init_platformio_build(
                board,
                verbose=False,
                example="Blink",
                paths=paths,
                build_dir=build_dir,
                use_fbuild=True,  # skip ensure_platform_installed / pio run
            )

    def test_default_skips_root_merge(self) -> None:
        """Without the tripwire, root flags never reach the synthesised env."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            _write_root_ini_with_esp32c6_flag(tmp, "-D LEGACY_ROOT_ONLY=1")
            captured: list[list[str]] = []
            result = self._run_init(
                tmp,
                fail_on_root_merge=False,
                captured_flags=captured,
            )
            self.assertTrue(result.success, msg=getattr(result, "output", ""))
            self.assertEqual(len(captured), 1)
            # The legacy root-only flag MUST NOT appear in the final build_flags.
            self.assertFalse(
                any("LEGACY_ROOT_ONLY" in flag for flag in captured[0]),
                f"Root-only flag leaked into synthesised env: {captured[0]!r}",
            )

    def test_tripwire_raises_when_root_would_merge(self) -> None:
        """``FASTLED_FAIL_ON_ROOT_MERGE=1`` + non-empty root => raise."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            _write_root_ini_with_esp32c6_flag(tmp, "-D MUST_BE_PORTED=1")
            captured: list[list[str]] = []
            with self.assertRaises(RuntimeError) as ctx:
                self._run_init(
                    tmp,
                    fail_on_root_merge=True,
                    captured_flags=captured,
                )
            self.assertIn("MUST_BE_PORTED", str(ctx.exception))
            self.assertIn("FASTLED_FAIL_ON_ROOT_MERGE", str(ctx.exception))

    def test_tripwire_silent_when_root_has_no_matching_env(self) -> None:
        """Tripwire is a no-op when the root ini has no [env:<board>] match."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            # Write a root ini but with NO [env:esp32c6] section.
            (tmp / "platformio.ini").write_text(
                "[env:uno]\nplatform = atmelavr\nbuild_flags = -DUNO_ONLY\n",
            )
            captured: list[list[str]] = []
            result = self._run_init(
                tmp,
                fail_on_root_merge=True,
                captured_flags=captured,
            )
            # No exception, no leak.
            self.assertTrue(result.success, msg=getattr(result, "output", ""))
            self.assertFalse(
                any("UNO_ONLY" in flag for flag in captured[0]),
                f"Unrelated env flag leaked: {captured[0]!r}",
            )

    def test_tripwire_silent_when_root_missing(self) -> None:
        """Tripwire is a no-op when the root ini doesn't exist at all."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            # No platformio.ini at all.
            captured: list[list[str]] = []
            result = self._run_init(
                tmp,
                fail_on_root_merge=True,
                captured_flags=captured,
            )
            self.assertTrue(result.success, msg=getattr(result, "output", ""))


class TestProbeRootPlatformioBuildFlags(unittest.TestCase):
    """The tripwire-only probe moved to ``ci/compiler/pio.py`` in #3279
    Phase 4. Verify its surface still behaves correctly in isolation.
    """

    def test_missing_file_returns_empty(self) -> None:
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            self.assertEqual(
                pio_module._probe_root_platformio_build_flags("esp32c6", tmp),
                [],
            )

    def test_no_matching_env_returns_empty(self) -> None:
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            (tmp / "platformio.ini").write_text(
                "[env:uno]\nplatform = atmelavr\nbuild_flags = -DUNO_ONLY\n",
            )
            self.assertEqual(
                pio_module._probe_root_platformio_build_flags("esp32c6", tmp),
                [],
            )

    def test_direct_build_flags_returned(self) -> None:
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            (tmp / "platformio.ini").write_text(
                "[env:esp32c6]\n"
                "platform = espressif32\n"
                "build_flags =\n"
                "    -D ARDUINO_USB_MODE=1\n"
                "    -D PIN_DATA=21\n",
            )
            flags = pio_module._probe_root_platformio_build_flags("esp32c6", tmp)
            self.assertIn("-D ARDUINO_USB_MODE=1", flags)
            self.assertIn("-D PIN_DATA=21", flags)

    def test_malformed_file_returns_empty(self) -> None:
        """A broken root ini must never block a build — it returns []."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            (tmp / "platformio.ini").write_text(
                "= = = not actually ini = = =\n[env:esp32c6\nbuild_flags = -DX\n",
            )
            self.assertEqual(
                pio_module._probe_root_platformio_build_flags("esp32c6", tmp),
                [],
            )


class TestPioCompilerConstructor(unittest.TestCase):
    """Verify the constructor no longer exposes ``merge_root_platformio``."""

    def test_no_merge_root_platformio_attribute(self) -> None:
        """Constructing PioCompiler should NOT carry the removed kwarg."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            compiler = pio_module.PioCompiler(
                board=Board(board_name="uno"),
                verbose=False,
                global_cache_dir=tmp / "cache",
                use_fbuild=True,
            )
            self.assertFalse(
                hasattr(compiler, "merge_root_platformio"),
                "merge_root_platformio attribute should be removed in #3279 Phase 4",
            )

    def test_passing_removed_kwarg_raises(self) -> None:
        """``merge_root_platformio=`` was removed; passing it now raises."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            with self.assertRaises(TypeError):
                pio_module.PioCompiler(  # type: ignore[call-arg]
                    board=Board(board_name="uno"),
                    verbose=False,
                    global_cache_dir=tmp / "cache",
                    use_fbuild=True,
                    merge_root_platformio=True,
                )


if __name__ == "__main__":
    unittest.main()
