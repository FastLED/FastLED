"""The outer lint stage must preserve mandatory semantic checks and deadlines."""

import importlib.util
from pathlib import Path
from types import SimpleNamespace

import pytest

from ci.lint import stage_impls
from ci.lint.args_parser import LintArgs
from ci.util.paths import PROJECT_ROOT


@pytest.mark.parametrize("use_rust", [False, True])
def test_unchanged_cpp_sources_still_surface_missing_ast_prerequisite(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str], use_rust: bool
) -> None:
    # Reproduce a previously successful whole-source fingerprint. SDK/tool
    # availability can change even while every C++ source stays unchanged.
    monkeypatch.setattr(
        stage_impls, "check_cpp_files_changed", lambda: False, raising=False
    )
    commands: list[list[str]] = []
    returncodes = iter([0, 1])

    def invoke(command: list[str], **kwargs: object) -> SimpleNamespace:
        commands.append(command)
        code = next(returncodes)
        return SimpleNamespace(
            returncode=code,
            stdout="",
            stderr="Missing required ESP SDK; provision again" if code else "",
        )

    monkeypatch.setattr(stage_impls.RunningProcess, "run", invoke)
    assert stage_impls.run_cpp_lint(False, False, False, use_rust)
    assert not stage_impls.run_cpp_lint(False, False, False, use_rust)
    assert len(commands) == 2
    assert all("ci/lint_cpp/run_all_checkers.py" in command for command in commands)
    assert all(
        ("--rust" if use_rust else "--no-rust") in command for command in commands
    )
    assert "Missing required ESP SDK" in capsys.readouterr().err


def test_outer_cpp_deadline_allows_nested_checker_and_stage_overhead(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    path: Path = PROJECT_ROOT / "ci/lint.py"
    spec = importlib.util.spec_from_file_location("lint_stage_deadline_test", path)
    assert spec is not None and spec.loader is not None
    lint = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(lint)
    monkeypatch.setattr(
        stage_impls, "check_cpp_files_changed", lambda: True, raising=False
    )
    monkeypatch.setattr(
        stage_impls, "mark_cpp_lint_success", lambda: None, raising=False
    )
    deadlines: list[float] = []

    def invoke(command: list[str], **kwargs: object) -> SimpleNamespace:
        deadlines.append(float(str(kwargs["timeout"])))
        return SimpleNamespace(returncode=0, stdout="", stderr="")

    monkeypatch.setattr(stage_impls.RunningProcess, "run", invoke)
    for name in (
        "run_iwyu_pragma_check",
        "run_iwyu_analysis",
        "run_clang_tidy",
        "run_noexcept_check",
    ):
        monkeypatch.setattr(lint, name, lambda *args: True)
    stages = lint.create_stages(LintArgs(cpp_only=True, skip_meson=True))
    cpp = next(stage for stage in stages if stage.name == "cpp_linting")
    assert cpp.run_fn()
    assert len(deadlines) == 1
    assert cpp.timeout >= deadlines[0] + 300
