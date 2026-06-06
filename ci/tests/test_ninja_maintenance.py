"""Tests for perform_ninja_maintenance (issues #2857, #2873).

On Windows, a partially-written ``.ninja_deps`` triggers a recursive crash
handler in ``ninja -t recompact`` (upstream ninja-build/ninja#2787). The
FastLED-side workaround is a snapshot+restore + broken-marker scheme:

  1. Skip recompact entirely if ``.ninja_deps_recompact_broken`` exists.
     Recompact is an optimization, not a correctness requirement.
  2. Snapshot ``.ninja_deps`` to ``.ninja_deps.bak`` before invoking
     recompact, so the original log can be put back if recompact fails.
  3. On success: delete the snapshot, log success.
  4. On failure: restore the snapshot, set the broken marker, and log
     a warning. Subsequent calls short-circuit on the broken marker.
  5. Touch the 24h cooldown marker on every attempt, success or failure.

These tests pin all of those behaviors.
"""

# pyright: reportUnknownArgumentType=false, reportUnknownVariableType=false
# pyright: reportUnknownMemberType=false, reportUnknownLambdaType=false
# pyright: reportMissingTypeArgument=false

from __future__ import annotations

import shutil
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
    def test_snapshot_taken_then_discarded_on_success(self) -> None:
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            deps = build_dir / ".ninja_deps"
            deps.write_bytes(b"# ninjadeps\x04\x00\x00\x00valid_record")

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(0)
            ):
                self.assertTrue(build_config.perform_ninja_maintenance(build_dir))

            self.assertTrue(
                deps.exists(),
                ".ninja_deps must stay in place — recompact reads it directly",
            )
            self.assertFalse(
                (build_dir / ".ninja_deps.bak").exists(),
                "snapshot must be discarded on successful recompact",
            )

    def test_stale_bak_is_replaced(self) -> None:
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            deps = build_dir / ".ninja_deps"
            bak = build_dir / ".ninja_deps.bak"
            deps.write_bytes(b"current")
            bak.write_bytes(b"stale")

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(1)
            ):
                build_config.perform_ninja_maintenance(build_dir)

            # On failure the snapshot is restored, so .ninja_deps holds the
            # pre-call bytes (which were "current"); the stale "stale" bak is
            # gone because it was overwritten before recompact ran.
            self.assertEqual(deps.read_bytes(), b"current")

    def test_snapshot_restored_on_recompact_failure(self) -> None:
        # The single most important guarantee from #2873: if recompact
        # crashes, the deps log is byte-identical to what we saw on entry.
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            deps = build_dir / ".ninja_deps"
            original = b"# ninjadeps\x04\x00\x00\x00the original valid records"
            deps.write_bytes(original)

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(1)
            ):
                build_config.perform_ninja_maintenance(build_dir)

            self.assertEqual(
                deps.read_bytes(),
                original,
                "failed recompact must restore the pre-call .ninja_deps bytes",
            )
            self.assertFalse(
                (build_dir / ".ninja_deps.bak").exists(),
                "after restore there should be no stray .bak left behind",
            )

    def test_broken_marker_set_on_recompact_failure(self) -> None:
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            broken = build_dir / ".ninja_deps_recompact_broken"

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(1)
            ):
                build_config.perform_ninja_maintenance(build_dir)

            self.assertTrue(
                broken.exists(),
                ".ninja_deps_recompact_broken must be set when recompact fails",
            )

    def test_broken_marker_skips_future_runs(self) -> None:
        # Once recompact is known broken on this dir, it should never be
        # invoked again — no matter how stale the 24h cooldown is.
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            (build_dir / ".ninja_deps_recompact_broken").touch()

            with mock.patch.object(build_config, "RunningProcess") as proc_cls:
                self.assertTrue(build_config.perform_ninja_maintenance(build_dir))
                proc_cls.assert_not_called()

    def test_marker_touched_on_recompact_failure(self) -> None:
        # Belt-and-suspenders: the 24h marker also gets touched on
        # failure, so even without the broken-marker we still honor the
        # cooldown (defense in depth in case the broken-marker write
        # itself fails, e.g. permission denied).
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            marker = build_dir / ".ninja_deps_maintenance"

            with mock.patch.object(
                build_config, "RunningProcess", _fake_running_process(1)
            ):
                build_config.perform_ninja_maintenance(build_dir)

            self.assertTrue(
                marker.exists(),
                "cooldown marker must be touched on failure too",
            )

    def test_within_cooldown_short_circuits(self) -> None:
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            marker = build_dir / ".ninja_deps_maintenance"
            marker.touch()

            with mock.patch.object(build_config, "RunningProcess") as proc_cls:
                self.assertTrue(build_config.perform_ninja_maintenance(build_dir))
                proc_cls.assert_not_called()

    def test_snapshot_failure_skips_recompact_and_flags_broken(self) -> None:
        # If we can't take a snapshot we can't safely run recompact —
        # better to keep the existing .ninja_deps untouched. The function
        # must skip RunningProcess entirely and set the broken marker so
        # the same host never tries again.
        with TemporaryDirectory() as td:
            build_dir = Path(td)
            deps = build_dir / ".ninja_deps"
            original = b"# ninjadeps\x04\x00\x00\x00records"
            deps.write_bytes(original)

            with (
                mock.patch.object(
                    shutil,
                    "copy2",
                    side_effect=OSError("simulated snapshot failure"),
                ),
                mock.patch.object(build_config, "RunningProcess") as proc_cls,
            ):
                self.assertTrue(build_config.perform_ninja_maintenance(build_dir))
                proc_cls.assert_not_called()

            self.assertEqual(
                deps.read_bytes(),
                original,
                ".ninja_deps must stay untouched when snapshot fails",
            )
            self.assertTrue(
                (build_dir / ".ninja_deps_recompact_broken").exists(),
                "broken marker must be set when snapshot fails (no retry)",
            )

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
