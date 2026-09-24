"""Hard-deadline and quiet-output regressions for Meson compilation."""

import sys
import time
from pathlib import Path
from types import SimpleNamespace

import pytest
from running_process import RunningProcess

from ci.meson import compile as compile_mod
from ci.meson import sequential_runner, streaming, streaming_runner


def _short_thresholds(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(compile_mod, "_COMPILE_POLL_SECONDS", 0.01)
    monkeypatch.setattr(compile_mod, "_COMPILE_QUIET_WARNING_SECONDS", 0.05)
    monkeypatch.setattr(compile_mod, "_COMPILE_QUIET_DUMP_SECONDS", 0.11)
    monkeypatch.setattr(compile_mod, "_COMPILE_HARD_TIMEOUT_SECONDS", 0.2)


def test_silent_child_is_diagnosed_and_killed_before_deadline_returns(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    _short_thresholds(monkeypatch)
    dumps: list[str] = []
    monkeypatch.setattr(
        "ci.util.test_env.dump_thread_stacks", lambda: dumps.append("dumped")
    )
    proc = RunningProcess(
        [
            sys.executable,
            "-c",
            "import time; print('started', flush=True); time.sleep(10)",
        ],
        auto_run=True,
        check=False,
    )
    started = time.monotonic()
    with pytest.raises(compile_mod._CompileDeadlineExceeded):
        list(
            compile_mod._monitored_compile_lines(
                proc, command=["meson", "compile"], target="smoke", started=started
            )
        )
    # Process-tree cleanup can take several seconds on a loaded runner.
    assert time.monotonic() - started < 8
    assert proc.poll() is not None
    stderr = capsys.readouterr().err
    assert stderr.count("Compiler still quiet") == 1
    assert stderr.count("Sustained silence") == 1
    assert "Hard compilation deadline exceeded" in stderr
    assert "PID" in stderr and "target=smoke" in stderr
    assert dumps == ["dumped"]


def test_progressing_child_can_be_quiet_for_more_than_warning_threshold(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    _short_thresholds(monkeypatch)
    monkeypatch.setattr(compile_mod, "_COMPILE_HARD_TIMEOUT_SECONDS", 1.0)
    proc = RunningProcess(
        [
            sys.executable,
            "-c",
            "import time; print('start', flush=True); time.sleep(.07); "
            "print('step', flush=True); time.sleep(.07); print('done', flush=True)",
        ],
        auto_run=True,
        check=False,
    )
    lines = list(
        compile_mod._monitored_compile_lines(
            proc, command=["meson", "compile"], target="smoke", started=time.monotonic()
        )
    )
    assert proc.wait(timeout=1) == 0
    assert [line.strip() for line in lines] == ["start", "step", "done"]
    assert "Sustained silence" not in capsys.readouterr().err


def test_compile_timeout_is_a_nonretryable_failure(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    _short_thresholds(monkeypatch)
    monkeypatch.setattr("ci.util.test_env.dump_thread_stacks", lambda: None)
    build_dir = tmp_path / "meson-quick"
    build_dir.mkdir()
    monkeypatch.setattr(compile_mod, "check_ninja_skip", lambda *_: False)
    monkeypatch.setattr(compile_mod, "_run_precompile_passes", lambda *_: None)
    monkeypatch.setattr(compile_mod, "kill_stale_runner_processes", lambda *_: 0)
    real_process = RunningProcess

    def slow_process(*_args: object, **_kwargs: object) -> RunningProcess:
        return real_process(
            [sys.executable, "-c", "import time; time.sleep(10)"],
            auto_run=True,
            check=False,
        )

    monkeypatch.setattr(compile_mod, "RunningProcess", slow_process)
    result = compile_mod.compile_meson(build_dir, target="smoke", quiet=True)
    assert not result.success
    assert result.timed_out
    assert "Hard compilation deadline exceeded" in result.error_output
    assert result.error_log_file is not None
    assert result.error_log_file.exists()

    attempted: list[str] = []

    def fake_compile(_build_dir: Path, target: str, **_kwargs: object):
        attempted.append(target)
        return result

    monkeypatch.setattr(sequential_runner, "compile_meson", fake_compile)
    monkeypatch.setattr(
        sequential_runner,
        "_resolve_compile_candidates",
        lambda _ctx: ["first", "second"],
    )
    monkeypatch.setattr(sequential_runner, "_print_compile_failure", lambda **_: None)
    ctx = SimpleNamespace(
        build_mode="quick",
        phase_tracker=SimpleNamespace(set_phase=lambda *_args, **_kw: None),
        test_name="smoke",
        build_dir=build_dir,
        verbose=False,
        build_timer=SimpleNamespace(checkpoint=lambda *_args: None),
        log_failures=None,
        start_time=time.time(),
        meson_test_name="first",
    )
    outcome = sequential_runner.run_sequential_compile(ctx)
    assert not outcome.success
    assert attempted == ["first"]


def test_retry_deadline_after_tee_closes_returns_timeout(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    build_dir = tmp_path / "meson-quick"
    build_dir.mkdir()
    monkeypatch.setattr(compile_mod, "check_ninja_skip", lambda *_: False)
    monkeypatch.setattr(compile_mod, "_run_precompile_passes", lambda *_: None)
    monkeypatch.setattr(compile_mod, "kill_stale_runner_processes", lambda *_: 0)
    real_process = RunningProcess

    def transient_failure(*_args: object, **_kwargs: object) -> RunningProcess:
        return real_process(
            [
                sys.executable,
                "-c",
                "print('FAILED: [code=113] source.cpp.o', flush=True); exit(1)",
            ],
            auto_run=True,
            check=False,
        )

    def retry_deadline(*_args: object, **_kwargs: object) -> None:
        raise compile_mod._CompileDeadlineExceeded("retry deadline reached")

    monkeypatch.setattr(compile_mod, "RunningProcess", transient_failure)
    monkeypatch.setattr(compile_mod, "_retry_ninja", retry_deadline)
    result = compile_mod.compile_meson(build_dir, target="smoke", quiet=True)

    assert not result.success
    assert result.timed_out
    assert result.error_output == "retry deadline reached"
    assert result.error_log_file is not None
    assert "retry deadline reached" in result.error_log_file.read_text(encoding="utf-8")


def test_retry_uses_original_compile_deadline(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _short_thresholds(monkeypatch)
    monkeypatch.setattr("ci.util.test_env.dump_thread_stacks", lambda: None)
    started = time.monotonic()
    time.sleep(0.05)  # Time already consumed by the first compile attempt.
    real_process = RunningProcess
    children: list[RunningProcess] = []

    def slow_retry(*_args: object, **_kwargs: object) -> RunningProcess:
        child = real_process(
            [sys.executable, "-c", "import time; time.sleep(10)"],
            auto_run=True,
            check=False,
        )
        children.append(child)
        return child

    monkeypatch.setattr(compile_mod, "RunningProcess", slow_retry)
    with pytest.raises(compile_mod._CompileDeadlineExceeded):
        compile_mod._retry_ninja(
            ["meson", "compile"],
            "transient error",
            max_retries=1,
            backoff_seconds=0,
            still_transient=lambda _out: True,
            describe=lambda _out, _attempt: "retrying",
            before_retry=None,
            label="zccache",
            started=started,
            deadline_seconds=compile_mod._COMPILE_HARD_TIMEOUT_SECONDS,
            env=None,
            output_formatter=None,
        )
    assert len(children) == 1
    assert children[0].poll() is not None


def test_streaming_full_build_enforces_deadline_while_child_is_silent(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    _short_thresholds(monkeypatch)
    monkeypatch.setattr("ci.util.test_env.dump_thread_stacks", lambda: None)
    build_dir = tmp_path / "meson-debug"
    build_dir.mkdir()
    monkeypatch.setattr(streaming, "kill_stale_runner_processes", lambda *_: 0)
    real_process = RunningProcess
    children: list[RunningProcess] = []

    def slow_process(*_args: object, **_kwargs: object) -> RunningProcess:
        child = real_process(
            [sys.executable, "-c", "import time; time.sleep(10)"],
            auto_run=True,
            check=False,
        )
        children.append(child)
        return child

    monkeypatch.setattr(streaming, "RunningProcess", slow_process)
    started = time.monotonic()
    result = streaming.stream_compile_only(build_dir, compile_timeout=0.2)
    assert time.monotonic() - started < 2
    assert not result.success
    assert "Hard compilation deadline exceeded" in result.compile_output
    assert len(children) == 1 and children[0].poll() is not None


def test_streaming_stale_recovery_gets_only_remaining_compile_time(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    attempts: list[float] = []
    clock = iter((100.0, 105.0))
    monkeypatch.setattr(
        streaming_runner,
        "time",
        SimpleNamespace(monotonic=lambda: next(clock), time=time.time),
    )
    monkeypatch.setattr(streaming_runner, "_make_streaming_env", lambda *_: {})
    monkeypatch.setattr(streaming_runner, "examples_are_included", lambda *_: False)
    monkeypatch.setattr(
        streaming_runner, "_recover_stale_build", lambda *_a, **_kw: True
    )
    sentinel = object()
    monkeypatch.setattr(
        streaming_runner, "_handle_streaming_failure", lambda *_a: sentinel
    )

    def compile_attempt(**kwargs: object) -> streaming.StreamingResult:
        attempts.append(float(kwargs["compile_timeout"]))
        return streaming.StreamingResult(
            success=False,
            compile_output="ninja: error: missing and no known rule to make it",
        )

    monkeypatch.setattr(
        streaming_runner, "stream_compile_and_run_tests", compile_attempt
    )
    ctx = SimpleNamespace(
        source_dir=tmp_path,
        build_dir=tmp_path,
        build_mode="quick",
        start_time=time.time(),
        use_debug=False,
        exclude_suites=set(),
        check=False,
        verbose=False,
        test_file_filter=None,
        build_optimizer=None,
        build_timer=None,
    )
    assert streaming_runner.run_streaming_path(ctx) is sentinel
    assert attempts == [600.0, 595.0]
