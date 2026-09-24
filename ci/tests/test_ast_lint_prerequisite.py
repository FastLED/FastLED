"""Required clang-query AST lint must fail closed, including with warm caches."""

from pathlib import Path
from types import SimpleNamespace

import pytest

from ci.lint_cpp import ast_cache, run_all_checkers
from ci.tools import check_array_params, check_ast_combined, check_noexcept


def test_missing_ast_binary_is_fatal(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(check_noexcept, "_find_clang_query", lambda: [])
    with pytest.raises(check_noexcept.NoexceptCheckError, match="not found"):
        run_all_checkers._require_ast_tool()


def test_unspawnable_uv_ast_wrapper_is_fatal(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(
        check_noexcept,
        "_find_clang_query",
        lambda: ["uv", "run", "clang-tool-chain-query"],
    )
    monkeypatch.setattr(
        run_all_checkers.RunningProcess,
        "run",
        lambda *args, **kwargs: SimpleNamespace(
            returncode=1,
            stdout="",
            stderr="Failed to spawn: clang-tool-chain-query",
        ),
    )
    with pytest.raises(check_noexcept.NoexceptCheckError, match="Failed to spawn"):
        run_all_checkers._require_ast_tool()


def test_combined_ast_checks_tool_before_warm_cache(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    calls: list[str] = []
    monkeypatch.setattr(ast_cache, "_CACHE_DIR", tmp_path)
    monkeypatch.setattr(ast_cache, "_compute_fingerprint", lambda *args: "fixed")

    def probe() -> None:
        calls.append("probe")
        if len(calls) == 2:
            raise check_noexcept.NoexceptCheckError("clang-query missing")

    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", probe)
    monkeypatch.setattr(
        check_ast_combined, "find_combined_hits", lambda scope: ([], [])
    )

    first = run_all_checkers.run_combined_ast_check()
    assert not first[0].has_violations()
    assert not first[1].has_violations()
    assert (tmp_path / "noexcept_ast.fingerprint").exists()
    assert (tmp_path / "array_param_ast.fingerprint").exists()

    with pytest.raises(check_noexcept.NoexceptCheckError, match="missing"):
        run_all_checkers.run_combined_ast_check()
    assert calls == ["probe", "probe"]


def test_combined_ast_error_is_not_cached(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setattr(ast_cache, "_CACHE_DIR", tmp_path)
    monkeypatch.setattr(ast_cache, "_compute_fingerprint", lambda *args: "fixed")
    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", lambda: None)

    def fail(scope: str) -> tuple[list[object], list[object]]:
        raise check_noexcept.NoexceptCheckError("clang-query failed")

    monkeypatch.setattr(check_ast_combined, "find_combined_hits", fail)
    with pytest.raises(check_noexcept.NoexceptCheckError, match="failed"):
        run_all_checkers.run_combined_ast_check()
    assert not list(tmp_path.iterdir())


@pytest.mark.parametrize(
    "check_name",
    ["run_noexcept_ast_check", "run_array_param_ast_check"],
)
def test_single_file_ast_error_is_fatal(
    monkeypatch: pytest.MonkeyPatch, check_name: str
) -> None:
    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", lambda: None)

    def fail(scope: str) -> list[object]:
        raise check_noexcept.NoexceptCheckError("clang-query failed")

    if check_name == "run_noexcept_ast_check":
        monkeypatch.setattr(check_noexcept, "find_missing_noexcept", fail)
    else:

        def fail_array(scope: str) -> list[object]:
            raise check_array_params.ArrayParamCheckError("clang-query failed")

        monkeypatch.setattr(check_array_params, "find_decayed_array_params", fail_array)

    check = getattr(run_all_checkers, check_name)
    error_type = (
        check_noexcept.NoexceptCheckError
        if check_name == "run_noexcept_ast_check"
        else check_array_params.ArrayParamCheckError
    )
    with pytest.raises(error_type, match="failed"):
        check(
            str(run_all_checkers.PROJECT_ROOT / "src" / "fl" / "chipsets" / "hd108.h")
        )
