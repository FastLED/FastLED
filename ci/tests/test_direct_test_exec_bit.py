"""The single-test path must restore the runner's executable bit (FastLED#4205).

`restore_executable_bits` was added for the `meson test` path, and the fix
stopped there. `bash test <name>` takes a different route -- `run_direct_test`
spawns the runner itself -- and that route had no such call. A runner zccache
had served from its link cache therefore stayed non-executable, and every
single-test invocation died with `failed to spawn process: Permission denied
(os error 13)`, which names neither the file nor the mode (FastLED#4168).

This drives the real `run_direct_test` against a real executable rather than
asserting that a particular function gets called, so it fails if the restore
moves, is reordered after the spawn, or is dropped.
"""

import os
import shutil
import stat
import time
from pathlib import Path

import pytest
from typeguard import typechecked

from ci.meson.build_timer import BuildTimer
from ci.meson.phase_tracker import PhaseTracker
from ci.meson.sequential_runner import DirectTestContext, run_direct_test


@typechecked
def _stub_build_tree(root: Path) -> Path:
    """A build directory shaped like the one `run_direct_test` looks at.

    The runner is a copy of a real system executable: `restore_executable_bits`
    only touches files carrying a native-executable magic number, so a stub
    written as text would be skipped for the right reason and prove nothing.
    `sh` rather than `true`, because a multi-call coreutils binary dispatches
    on argv[0] and refuses to run under the name `runner`.
    """
    real_exe = shutil.which("sh")
    if real_exe is None:
        pytest.skip("no system shell to stand in for the test runner")

    tests = root / "tests"
    tests.mkdir(parents=True)
    runner = tests / "runner"
    shutil.copyfile(real_exe, runner)
    # The state zccache leaves behind: readable, not executable.
    runner.chmod(0o644)
    # `run_direct_test` spawns `runner <artifact>`, so the artifact is this
    # shell's script and exits 0. A real runner would dlopen a real .so; what
    # matters here is that something got spawned at all.
    (tests / "stub_case.so").write_text("exit 0\n")
    return runner


@typechecked
def _context(source_dir: Path, build_dir: Path) -> DirectTestContext:
    timer = BuildTimer()
    timer.start()
    return DirectTestContext(
        source_dir=source_dir,
        build_dir=build_dir,
        meson_test_name="stub_case",
        build_mode="quick",
        verbose=False,
        log_failures=None,
        start_time=time.time(),
        build_timer=timer,
        phase_tracker=PhaseTracker(build_dir, "quick"),
    )


@pytest.mark.skipif(os.name == "nt", reason="POSIX mode bits")
@typechecked
def test_direct_test_run_restores_a_non_executable_runner(tmp_path: Path) -> None:
    build_dir = tmp_path / "build"
    runner = _stub_build_tree(build_dir)
    assert not runner.stat().st_mode & stat.S_IXUSR

    result = run_direct_test(_context(tmp_path, build_dir))

    # Spawned rather than refused, which is the whole claim.
    assert result.success, "run_direct_test could not spawn a cache-restored runner"
    assert runner.stat().st_mode & stat.S_IXUSR


@pytest.mark.skipif(os.name == "nt", reason="POSIX mode bits")
@typechecked
def test_an_already_executable_runner_is_left_alone(tmp_path: Path) -> None:
    """The restore must not widen permissions it did not need to touch."""
    build_dir = tmp_path / "build"
    runner = _stub_build_tree(build_dir)
    runner.chmod(0o700)

    result = run_direct_test(_context(tmp_path, build_dir))

    assert result.success
    assert stat.S_IMODE(runner.stat().st_mode) == 0o700
