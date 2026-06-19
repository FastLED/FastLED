"""Tests for the Phase-2 sever of the root-platformio.ini merge (issue #3278).

Until #3278 landed, ``_init_platformio_build`` in ``ci/compiler/pio.py``
*always* appended ``[env:<board>].build_flags`` from the repo-root
``platformio.ini`` to the synthesised env. That made the root file an
implicit input to every CI binary — edits to root could silently change
CI behaviour. #3278 inverts the default: ``merge_root_platformio=False``
on every CI entry point, and ``FASTLED_FAIL_ON_ROOT_MERGE=1`` turns the
probe into a tripwire that fires when any flag would have been merged
silently.

These tests exercise that behaviour directly against
``_init_platformio_build`` so we don't have to spin up real PlatformIO.
"""

from __future__ import annotations

import os
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import mock

from ci.boards import Board
from ci.compiler import build_config as build_config_module
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


def _patch_no_op_pipeline(
    stack: "mock._patch_dict.__class__ | unittest.mock._patch",
) -> None:  # pragma: no cover - signature placeholder, see callers
    """Placeholder kept for future shared patching helpers."""
    raise NotImplementedError


class TestRootPlatformioSever(unittest.TestCase):
    """Verify the #3278 sever semantics on _init_platformio_build."""

    def _run_init(
        self,
        tmp: Path,
        merge_root_platformio: bool,
        fail_on_root_merge: bool,
        captured_flags: list[list[str]],
    ) -> object:
        """Drive ``_init_platformio_build`` with the project root patched
        to ``tmp`` and all heavyweight side effects mocked out.

        ``captured_flags`` collects the final ``build_flags`` that
        ``apply_board_specific_config`` would have written, so each test
        can assert what reached the synthesised env.
        """
        # Patch resolve_project_root() so the root platformio.ini probe
        # reads from our tempdir and the build directory lands there too.
        env_patch = {}
        if fail_on_root_merge:
            env_patch["FASTLED_FAIL_ON_ROOT_MERGE"] = "1"
        else:
            # Ensure inherited env from the host doesn't accidentally turn
            # the tripwire on for the merge=False/probe=off cases.
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
                merge_root_platformio=merge_root_platformio,
            )

    def test_default_skips_root_merge(self) -> None:
        """``merge_root_platformio=False`` does not append root flags."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            _write_root_ini_with_esp32c6_flag(tmp, "-D LEGACY_ROOT_ONLY=1")
            captured: list[list[str]] = []
            result = self._run_init(
                tmp,
                merge_root_platformio=False,
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

    def test_opt_in_appends_root_flags(self) -> None:
        """``merge_root_platformio=True`` restores the legacy merge."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            _write_root_ini_with_esp32c6_flag(tmp, "-D ROOT_OPT_IN=1")
            captured: list[list[str]] = []
            result = self._run_init(
                tmp,
                merge_root_platformio=True,
                fail_on_root_merge=False,
                captured_flags=captured,
            )
            self.assertTrue(result.success, msg=getattr(result, "output", ""))
            self.assertEqual(len(captured), 1)
            self.assertTrue(
                any("ROOT_OPT_IN" in flag for flag in captured[0]),
                f"Opt-in root flag did not reach synthesised env: {captured[0]!r}",
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
                    merge_root_platformio=False,
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
                merge_root_platformio=False,
                fail_on_root_merge=True,
                captured_flags=captured,
            )
            # No exception, no leak.
            self.assertTrue(result.success, msg=getattr(result, "output", ""))
            self.assertFalse(
                any("UNO_ONLY" in flag for flag in captured[0]),
                f"Unrelated env flag leaked: {captured[0]!r}",
            )

    def test_tripwire_allows_opt_in_merge(self) -> None:
        """``merge_root_platformio=True`` + tripwire set => still merges, no raise.

        The tripwire only fires when a flag would have been SILENTLY merged.
        Opt-in callers (e.g. `bash debug --use-root-ini`) are explicit and
        should not be blocked by the canary.
        """
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            _write_root_ini_with_esp32c6_flag(tmp, "-D EXPLICIT_OPT_IN=1")
            captured: list[list[str]] = []
            result = self._run_init(
                tmp,
                merge_root_platformio=True,
                fail_on_root_merge=True,
                captured_flags=captured,
            )
            self.assertTrue(result.success, msg=getattr(result, "output", ""))
            self.assertTrue(
                any("EXPLICIT_OPT_IN" in flag for flag in captured[0]),
                f"Opt-in flag did not reach synthesised env: {captured[0]!r}",
            )


class TestPioCompilerConstructor(unittest.TestCase):
    """Verify the new constructor parameter defaults to False."""

    def test_default_merge_root_platformio_is_false(self) -> None:
        """Constructing PioCompiler without the kwarg gives the sever default."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            compiler = pio_module.PioCompiler(
                board=Board(board_name="uno"),
                verbose=False,
                global_cache_dir=tmp / "cache",
                use_fbuild=True,
            )
            self.assertFalse(compiler.merge_root_platformio)

    def test_opt_in_keyword_is_stored(self) -> None:
        """Passing merge_root_platformio=True is preserved on the instance."""
        with TemporaryDirectory() as tmp_str:
            tmp = Path(tmp_str)
            compiler = pio_module.PioCompiler(
                board=Board(board_name="uno"),
                verbose=False,
                global_cache_dir=tmp / "cache",
                use_fbuild=True,
                merge_root_platformio=True,
            )
            self.assertTrue(compiler.merge_root_platformio)


if __name__ == "__main__":
    unittest.main()
