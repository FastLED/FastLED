"""Tests for perform_ninja_maintenance (issue #2857).

On Windows, a partially-written .ninja_deps file triggers a recursive
crash handler in `ninja -t recompact` that emits ~150 minidumps before
stack overflowing. To defuse that, ``perform_ninja_maintenance``:

  1. Quarantines an existing ``.ninja_deps`` to ``.ninja_deps.bak`` so
     recompact always starts from a clean slate.
  2. Touches the maintenance marker regardless of recompact's exit code
     so a host where recompact deterministically fails does not re-run
     it (and re-trigger the cascade) on every invocation.

These tests pin both behaviors so future refactors don't regress them.
"""

# pyright: reportUnknownArgumentType=false, reportUnknownVariableType=false
# pyright: reportUnknownMemberType=false, reportUnknownLambdaType=false
# pyright: reportMissingTypeArgument=false

from __future__ import annotations

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any
from unittest import mock

from ci.meson import build_config


class _FakeRepairProc:
    """Stand-in for RunningProcess returning a configurable exit code."""

    def __init__(self, returncode: int) -> None:
        self._rc = returncode

    def wait(self, echo: bool = False) -> int:
        del echo
        return self._rc


def _fake_running_process(returncode: int) -> Any:
    """Factory returning a callable that mimics RunningProcess(...)."""

    def _factory(*args: Any, **kwargs: Any) -> _FakeRepairProc:
        del args, kwargs
        return _FakeRepairProc(returncode)

    return _factory


class TestPerformNinjaMaintenance(unittest.TestCase):
    def test_quarantines_existing_deps_before_recompact(self) -> None:
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            deps = build_dir / ".ninja_deps"
            deps.write_bytes(b"# ninjadeps\x04\x00\x00\x00partial")

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(0)
            ):
                self.assertTrue(build_config.perform_ninja_maintenance(build_dir))

            self.assertFalse(
                deps.exists(),
                ".ninja_deps should have been moved aside before recompact",
            )
            self.assertTrue(
                (build_dir / ".ninja_deps.bak").exists(),
                ".ninja_deps.bak should have been created from the quarantined deps",
            )

    def test_stale_bak_is_replaced(self) -> None:
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            deps = build_dir / ".ninja_deps"
            bak = build_dir / ".ninja_deps.bak"
            deps.write_bytes(b"current")
            bak.write_bytes(b"stale")

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(0)
            ):
                build_config.perform_ninja_maintenance(build_dir)

            self.assertEqual(bak.read_bytes(), b"current")

    def test_marker_touched_on_recompact_failure(self) -> None:
        # Regression: previously the marker was only touched on
        # returncode == 0, so a Windows host crashing recompact every
        # time would re-run it on every invocation.
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            marker = build_dir / ".ninja_deps_maintenance"

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(1)
            ):
                build_config.perform_ninja_maintenance(build_dir)

            self.assertTrue(
                marker.exists(),
                "marker must be touched on failure to honor the 24h cooldown",
            )

    def test_within_cooldown_short_circuits(self) -> None:
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            marker = build_dir / ".ninja_deps_maintenance"
            marker.touch()

            with mock.patch.object(build_config, "RunningProcess") as proc_cls:
                self.assertTrue(build_config.perform_ninja_maintenance(build_dir))
                proc_cls.assert_not_called()

    def test_no_deps_file_is_a_clean_no_op(self) -> None:
        with TemporaryDirectory() as td:
            build_dir = Path(td)

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(0)
            ):
                self.assertTrue(build_config.perform_ninja_maintenance(build_dir))

            self.assertFalse((build_dir / ".ninja_deps").exists())
            self.assertFalse((build_dir / ".ninja_deps.bak").exists())
            self.assertTrue((build_dir / ".ninja_deps_maintenance").exists())


if __name__ == "__main__":
    unittest.main()
