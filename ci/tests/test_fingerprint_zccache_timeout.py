"""`zccache fp` calls must survive a loaded host (#5398)."""

from dataclasses import dataclass, field

import pytest
from running_process import CompletedProcess, TimeoutExpired

from ci.fingerprint import core


@dataclass
class _FakeRunner:
    timeouts_left: int
    timeouts: list[float] = field(default_factory=list)

    def run(self, cmd: list[str], **kwargs: object) -> CompletedProcess[str]:
        timeout = kwargs["timeout"]
        assert isinstance(timeout, float)
        self.timeouts.append(timeout)
        if self.timeouts_left > 0:
            self.timeouts_left -= 1
            raise TimeoutExpired(cmd, timeout)
        return CompletedProcess(cmd, 1, "", "")


def _install(monkeypatch: pytest.MonkeyPatch, runner: _FakeRunner) -> None:
    monkeypatch.setattr(core, "RunningProcess", runner)
    monkeypatch.setattr(core, "_find_zccache", lambda: "zccache")
    monkeypatch.setattr(core, "load_scale", lambda: 3.0)


def test_timeout_scales_with_host_load(monkeypatch: pytest.MonkeyPatch) -> None:
    runner = _FakeRunner(timeouts_left=0)
    _install(monkeypatch, runner)
    assert core._run_zccache(["check"], timeout=60.0).returncode == 1
    assert runner.timeouts == [180.0]


def test_one_timeout_is_retried(monkeypatch: pytest.MonkeyPatch) -> None:
    runner = _FakeRunner(timeouts_left=1)
    _install(monkeypatch, runner)
    assert core._run_zccache(["check"], timeout=60.0).returncode == 1
    assert len(runner.timeouts) == 2


def test_repeated_timeout_still_fails(monkeypatch: pytest.MonkeyPatch) -> None:
    runner = _FakeRunner(timeouts_left=2)
    _install(monkeypatch, runner)
    with pytest.raises(TimeoutExpired):
        core._run_zccache(["check"], timeout=60.0)
    assert len(runner.timeouts) == 2
