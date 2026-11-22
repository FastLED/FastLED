"""Hard-deadline and quiet-output regressions for Meson compilation."""

import sys
import time
from collections.abc import Iterator
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace
from typing import cast

import pytest
from running_process import RunningProcess

from ci.meson import compile as compile_mod
from ci.meson import sequential_runner, streaming, streaming_runner
from ci.meson.native_input_guard import NativeInputState


def _short_thresholds(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(compile_mod, "_COMPILE_POLL_SECONDS", 0.01)
    monkeypatch.setattr(compile_mod, "_COMPILE_QUIET_WARNING_SECONDS", 0.05)
    monkeypatch.setattr(compile_mod, "_COMPILE_QUIET_DUMP_SECONDS", 0.11)
    monkeypatch.setattr(compile_mod, "_COMPILE_HARD_TIMEOUT_SECONDS", 0.2)
    monkeypatch.setattr(compile_mod, "_COMPILE_STALL_SECONDS", 0.15)
    monkeypatch.setattr(compile_mod, "load_scale", lambda: 1.0)


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
            "import time; time.sleep(10)",
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
    # Drive quiet intervals with a virtual clock: child startup and host load
    # must not turn a progressing process into a sustained-silence diagnostic.
    clock = SimpleNamespace(now=0.0)
    monkeypatch.setattr(compile_mod.time, "monotonic", lambda: clock.now)

    class ProgressingLines:
        def __init__(self) -> None:
            self.events = iter(("start", None, "step", None, "done"))

        def __next__(self) -> str:
            event = next(self.events)
            if event is None:
                clock.now += 0.06
                raise TimeoutError
            return event

    class ProgressingProcess:
        pid = None

        @contextmanager
        def line_iter(self, **_kwargs: object) -> Iterator[ProgressingLines]:
            yield ProgressingLines()

        def poll(self) -> int:
            return 0

    proc = ProgressingProcess()
    lines = list(
        compile_mod._monitored_compile_lines(
            cast(RunningProcess, proc),
            command=["meson", "compile"],
            target="smoke",
            started=time.monotonic(),
        )
    )
    assert [line.strip() for line in lines] == ["start", "step", "done"]
    stderr = capsys.readouterr().err
    assert stderr.count("Compiler still quiet") == 2
    assert "Sustained silence" not in stderr


def test_compile_timeout_is_a_nonretryable_failure(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    _short_thresholds(monkeypatch)
    monkeypatch.setattr("ci.util.test_env.dump_thread_stacks", lambda: None)
    build_dir = tmp_path / "meson-quick"
    build_dir.mkdir()
    monkeypatch.setattr(compile_mod, "check_ninja_skip", lambda *_: False)
    monkeypatch.setattr(
        compile_mod,
        "_run_precompile_passes",
        lambda *_: NativeInputState(build_dir, {}, changed=False),
    )
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


@pytest.mark.parametrize(
    "diagnostic,retry",
    [
        ("FAILED: object.o\nzccache[err]: PCH alias validation failed", False),
        ("source.cpp:5: error: invalid conversion", False),
        ("undefined reference to setup", False),
        ("unexpected build backend failure", False),
        ("error: unknown target triple 'bad-target'", False),
        ("error: unknown target CPU 'bad-cpu'", False),
        ("ERROR: Can't invoke target `missing`: target not found", True),
        ("ERROR: Can't invoke target `json`: ambiguous name", True),
        ("ninja: error: unknown target 'missing'", True),
        ("ERROR: Can't invoke target `json`: ambiguous name\nFAILED: object.o", False),
    ],
)
def test_fuzzy_target_fallback_cannot_hide_compiler_failure(
    diagnostic: str, retry: bool, monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    attempted: list[str] = []

    def compile_candidate(
        _build_dir: Path, target: str, **_kwargs: object
    ) -> compile_mod.CompileResult:
        attempted.append(target)
        return compile_mod.CompileResult(
            success=target == "second", error_output=diagnostic, suppressed_errors=[]
        )

    monkeypatch.setattr(sequential_runner, "compile_meson", compile_candidate)
    monkeypatch.setattr(sequential_runner, "_print_compile_failure", lambda **_: None)
    ctx = SimpleNamespace(
        source_dir=tmp_path,
        build_mode="debug",
        build_dir=tmp_path,
        phase_tracker=SimpleNamespace(set_phase=lambda *_args, **_kw: None),
        test_name="first",
        meson_test_name="first",
        fallback_test_name="second",
        fuzzy_candidates=["second"],
        verbose=False,
        log_failures=None,
        build_timer=SimpleNamespace(checkpoint=lambda *_args: None),
        start_time=time.time(),
    )
    outcome = sequential_runner.run_sequential_compile(ctx)
    assert outcome.success is retry
    assert attempted == (["first", "second"] if retry else ["first"])


def test_retry_deadline_after_tee_closes_returns_timeout(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    build_dir = tmp_path / "meson-quick"
    build_dir.mkdir()
    monkeypatch.setattr(compile_mod, "check_ninja_skip", lambda *_: False)
    monkeypatch.setattr(
        compile_mod,
        "_run_precompile_passes",
        lambda *_: NativeInputState(build_dir, {}, changed=False),
    )
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
    # Match the direct monitor test: process-tree cleanup has its own grace
    # period, but must still finish before the child's natural ten-second exit.
    assert time.monotonic() - started < 8
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
        compile_target=None,
        include_suites=None,
        test_timeout=None,
    )
    assert streaming_runner.run_streaming_path(ctx) is sentinel
    assert attempts == [600.0, 595.0]


class _ScriptedLines:
    """Line source driven by a virtual clock: ``None`` advances time silently."""

    def __init__(self, clock: SimpleNamespace, events: list[str | None]) -> None:
        self._clock = clock
        self._events = iter(events)
        self.exhausted = False

    def __next__(self) -> str:
        try:
            event = next(self._events)
        except StopIteration:
            self.exhausted = True
            raise
        self._clock.now += 0.1
        if event is None:
            raise TimeoutError
        return event


class _ScriptedProcess:
    pid = None

    def __init__(self, clock: SimpleNamespace, events: list[str | None]) -> None:
        self._lines = _ScriptedLines(clock, events)
        self.killed = False

    @contextmanager
    def line_iter(self, **_kwargs: object) -> Iterator[_ScriptedLines]:
        yield self._lines

    def poll(self) -> int | None:
        if self.killed:
            return -9
        return 0 if self._lines.exhausted else None

    def kill(self) -> None:
        self.killed = True


def _virtual_clock(monkeypatch: pytest.MonkeyPatch) -> SimpleNamespace:
    clock = SimpleNamespace(now=0.0)
    monkeypatch.setattr(compile_mod.time, "monotonic", lambda: clock.now)
    monkeypatch.setattr(compile_mod.time, "sleep", lambda _s: None)
    return clock


def _drain(proc: _ScriptedProcess, deadline: float) -> list[str]:
    return list(
        compile_mod._monitored_compile_lines(
            cast(RunningProcess, proc),
            command=["meson", "compile"],
            target="smoke",
            started=0.0,
            deadline_seconds=deadline,
        )
    )


def test_progressing_build_is_not_killed_at_the_deadline(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """#5398: a build still linking targets at the deadline keeps running."""
    _short_thresholds(monkeypatch)
    monkeypatch.setattr(compile_mod, "_COMPILE_STALL_SECONDS", 0.35)
    clock = _virtual_clock(monkeypatch)
    # Output every 0.3 s of virtual time, out to 3x the 1.0 s deadline.
    events: list[str | None] = []
    for step in range(10):
        events += [f"[{step}/10] Linking target t{step}", None, None]
    events.append("[10/10] done")
    proc = _ScriptedProcess(clock, events)
    lines = _drain(proc, deadline=1.0)
    assert len(lines) == 11
    assert not proc.killed
    assert clock.now > 3.0
    assert "Hard compilation deadline exceeded" not in capsys.readouterr().err


def test_build_that_stalls_after_the_deadline_is_killed(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    _short_thresholds(monkeypatch)
    monkeypatch.setattr(compile_mod, "_COMPILE_STALL_SECONDS", 0.35)
    monkeypatch.setattr("ci.util.test_env.dump_thread_stacks", lambda: None)
    clock = _virtual_clock(monkeypatch)
    events: list[str | None] = ["[1/3] a", None, None, "[2/3] b", None, None]
    events += ["[3/3] c"] + [None] * 50
    proc = _ScriptedProcess(clock, events)
    with pytest.raises(compile_mod._CompileDeadlineExceeded):
        _drain(proc, deadline=0.5)
    assert proc.killed
    # Killed one stall window after the last line, not at the deadline.
    assert 0.9 <= clock.now <= 1.2
    stderr = capsys.readouterr().err
    assert "Hard compilation deadline exceeded" in stderr
    assert "PID" in stderr or "Process tree unavailable" in stderr


def test_stall_window_scales_with_host_load(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _short_thresholds(monkeypatch)
    monkeypatch.setattr(compile_mod, "_COMPILE_STALL_SECONDS", 0.35)
    monkeypatch.setattr(compile_mod, "load_scale", lambda: 4.0)
    monkeypatch.setattr("ci.util.test_env.dump_thread_stacks", lambda: None)
    clock = _virtual_clock(monkeypatch)
    # 1.2 s of silence: a hang at load 1, slow-but-alive at load 4 (1.4 s).
    events: list[str | None] = ["[1/2] a"] + [None] * 12 + ["[2/2] b"]
    proc = _ScriptedProcess(clock, events)
    assert len(_drain(proc, deadline=0.5)) == 2
    assert not proc.killed


def test_progressing_build_still_has_an_absolute_cap(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _short_thresholds(monkeypatch)
    monkeypatch.setattr(compile_mod, "_COMPILE_STALL_SECONDS", 0.35)
    monkeypatch.setattr("ci.util.test_env.dump_thread_stacks", lambda: None)
    clock = _virtual_clock(monkeypatch)
    proc = _ScriptedProcess(clock, ["[n/m] step"] * 1000)
    with pytest.raises(compile_mod._CompileDeadlineExceeded):
        _drain(proc, deadline=1.0)
    assert proc.killed
    cap = compile_mod._COMPILE_MAX_DEADLINE_FACTOR * 1.0
    assert cap - 0.01 <= clock.now <= cap + 0.2


def test_absolute_cap_stays_inside_the_ci_job(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("CI", raising=False)
    assert compile_mod._absolute_compile_cap(1800.0) == 9000.0
    monkeypatch.setenv("CI", "true")
    assert compile_mod._absolute_compile_cap(1800.0) == 2400.0
    assert compile_mod._absolute_compile_cap(600.0) == 2400.0
    # The cap never undercuts the deadline itself.
    assert compile_mod._absolute_compile_cap(3000.0) == 3000.0
