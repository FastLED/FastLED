"""Unit tests for ci.meson.link_retry helpers (#2268).

These tests validate the pattern-matching and kill-under-build_dir logic
without spawning real runner.exe processes. The helpers are intentionally
no-ops on non-Windows platforms, so most tests assert that behavior too.
"""

from __future__ import annotations

import sys
from pathlib import Path
from unittest import mock

import pytest

from ci.meson.link_retry import (
    is_link_permission_denied_error,
    kill_stale_runner_processes,
)


# ---------------------------------------------------------------------------
# is_link_permission_denied_error
# ---------------------------------------------------------------------------


_WIN_ONLY = pytest.mark.skipif(
    sys.platform != "win32", reason="Retry logic is Windows-specific"
)


@_WIN_ONLY
def test_perm_denied_runner_exe_detected() -> None:
    """Positive match for the canonical runner.exe permission-denied error."""
    output = (
        "FAILED: [code=1] tests/runner.exe\n"
        "ld.lld: error: failed to write output 'tests/runner.exe': permission denied\n"
    )
    assert is_link_permission_denied_error(output)


@_WIN_ONLY
def test_perm_denied_example_runner_exe_detected() -> None:
    """Positive match for example_runner.exe variant."""
    output = (
        "ld.lld: error: failed to write output 'examples/example_runner.exe': "
        "permission denied"
    )
    assert is_link_permission_denied_error(output)


@_WIN_ONLY
def test_perm_denied_backslash_path_detected() -> None:
    """Windows-style backslashes in the path should still match."""
    output = (
        "ld.lld: error: failed to write output 'tests\\runner.exe': permission denied"
    )
    assert is_link_permission_denied_error(output)


@_WIN_ONLY
def test_perm_denied_non_runner_not_detected() -> None:
    """A permission-denied error on a NON-runner binary must NOT trigger retry.

    This is the guard against retrying legitimate build failures — e.g. an
    actual bug where a test .exe is locked by a user-spawned debugger.
    """
    output = (
        "ld.lld: error: failed to write output 'tests/test_foo.exe': permission denied"
    )
    assert not is_link_permission_denied_error(output)


@_WIN_ONLY
def test_perm_denied_empty_output_not_detected() -> None:
    assert not is_link_permission_denied_error("")


@_WIN_ONLY
def test_perm_denied_unrelated_output_not_detected() -> None:
    output = "ninja: build stopped: subcommand failed.\n"
    assert not is_link_permission_denied_error(output)


def test_perm_denied_returns_false_off_windows() -> None:
    """Detection is a no-op off Windows — never triggers retry on Linux/macOS."""
    if sys.platform == "win32":
        pytest.skip("Windows is the positive-case platform")
    output = (
        "ld.lld: error: failed to write output 'tests/runner.exe': permission denied"
    )
    assert not is_link_permission_denied_error(output)


# ---------------------------------------------------------------------------
# kill_stale_runner_processes
# ---------------------------------------------------------------------------


def test_kill_stale_runners_returns_zero_off_windows(tmp_path: Path) -> None:
    """Kill is a no-op on non-Windows platforms."""
    if sys.platform == "win32":
        pytest.skip("Windows is the positive-case platform")
    assert kill_stale_runner_processes(tmp_path) == 0


@_WIN_ONLY
def test_kill_stale_runners_skips_processes_outside_build_dir(
    tmp_path: Path,
) -> None:
    """Processes whose exe path is NOT under build_dir must be left alone."""
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    outside_dir = tmp_path / "other"
    outside_dir.mkdir()

    outside_exe = outside_dir / "runner.exe"
    outside_exe.write_bytes(b"")

    fake_proc = mock.MagicMock()
    fake_proc.info = {
        "pid": 1234,
        "name": "runner.exe",
        "exe": str(outside_exe),
    }

    with mock.patch("psutil.process_iter", return_value=[fake_proc]):
        killed = kill_stale_runner_processes(build_dir)

    assert killed == 0
    fake_proc.kill.assert_not_called()


@_WIN_ONLY
def test_kill_stale_runners_kills_matching_process(tmp_path: Path) -> None:
    """Runner under build_dir whose name matches should be killed."""
    build_dir = tmp_path / "build"
    (build_dir / "tests").mkdir(parents=True)
    inside_exe = build_dir / "tests" / "runner.exe"
    inside_exe.write_bytes(b"")

    fake_proc = mock.MagicMock()
    fake_proc.info = {
        "pid": 5678,
        "name": "runner.exe",
        "exe": str(inside_exe),
    }

    with mock.patch("psutil.process_iter", return_value=[fake_proc]):
        killed = kill_stale_runner_processes(build_dir)

    assert killed == 1
    fake_proc.kill.assert_called_once()


@_WIN_ONLY
def test_kill_stale_runners_ignores_non_runner_names(tmp_path: Path) -> None:
    """Processes in build_dir that are NOT runner/example_runner must be skipped."""
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    other_exe = build_dir / "some_test.exe"
    other_exe.write_bytes(b"")

    fake_proc = mock.MagicMock()
    fake_proc.info = {
        "pid": 9999,
        "name": "some_test.exe",
        "exe": str(other_exe),
    }

    with mock.patch("psutil.process_iter", return_value=[fake_proc]):
        killed = kill_stale_runner_processes(build_dir)

    assert killed == 0
    fake_proc.kill.assert_not_called()
