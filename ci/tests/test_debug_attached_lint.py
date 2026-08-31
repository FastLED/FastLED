from __future__ import annotations

from pathlib import Path
from types import SimpleNamespace
from typing import Any

from ci.debug_attached import _deploy_for_monitor, run_cpp_lint
from ci.util.fbuild_runner import FbuildCommandResult


def test_run_cpp_lint_uses_no_sync(monkeypatch: Any) -> None:
    calls: list[dict[str, Any]] = []

    def fake_run(cmd: list[str], **kwargs: Any) -> SimpleNamespace:
        calls.append({"cmd": cmd, **kwargs})
        return SimpleNamespace(returncode=0)

    monkeypatch.setattr("ci.debug_attached.subprocess.run", fake_run)

    assert run_cpp_lint()

    assert calls
    assert calls[0]["cmd"] == [
        "uv",
        "run",
        "--no-sync",
        "python",
        "ci/lint.py",
        "--cpp",
    ]


def test_deploy_for_monitor_defers_port_selection_to_fbuild(
    monkeypatch: Any,
) -> None:
    captured: dict[str, Any] = {}

    def fake_deploy(*args: Any, **kwargs: Any) -> FbuildCommandResult:
        captured["args"] = args
        captured["kwargs"] = kwargs
        return FbuildCommandResult(success=True, output="", port="COM42")

    def fail_if_called(_port: str) -> None:
        raise AssertionError("An auto-selected port must not be cleaned before deploy")

    monkeypatch.setattr("ci.util.fbuild_runner.run_fbuild_deploy", fake_deploy)
    monkeypatch.setattr("ci.debug_attached.kill_port_users", fail_if_called)

    port = _deploy_for_monitor(Path("."), "rp2040", None, verbose=False)

    assert port == "COM42"
    assert captured["kwargs"]["upload_port"] is None
