from __future__ import annotations

from types import SimpleNamespace
from typing import Any

from ci.debug_attached import run_cpp_lint


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
