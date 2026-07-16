"""Tests for the Meson canonical-path join lint rule."""

from pathlib import Path

from ci.lint_meson.path_norm_join_checker import PathNormJoinChecker
from ci.lint_meson.run_all_checkers import _collect_meson_files
from ci.util.check_files import FileContent


def _check(source: str) -> dict[str, list[tuple[int, str]]]:
    checker = PathNormJoinChecker()
    checker.check_file_content(
        FileContent(path="fixture/meson.build", content=source, lines=[])
    )
    return checker.violations


def test_rejects_path_norm_variables_joined_with_meson_operator() -> None:
    violations = _check(
        "wasm_include = '-I' + path_norm_root / 'src/platforms/wasm'\n"
        "nested = path_norm_custom / 'generated'\n"
    )

    issues = violations["fixture/meson.build"]
    assert [line for line, _ in issues] == [1, 2]
    assert "path_norm_root" in issues[0][1]
    assert "path_norm_custom" in issues[1][1]


def test_allows_literal_forward_slash_concatenation_and_comments() -> None:
    violations = _check(
        "src = path_norm_root + '/src'\n"
        "include = '-I' + path_norm_I_src\n"
        "# example = path_norm_root / 'src'\n"
        "message('example = path_norm_root / \\'src\\'')\n"
    )

    assert violations == {}


def test_ignores_non_meson_files() -> None:
    assert not PathNormJoinChecker().should_process_file("fixture/meson_options.txt")


def test_collects_only_repository_meson_files(tmp_path: Path) -> None:
    (tmp_path / "meson.build").touch()
    generated = tmp_path / ".build" / "generated"
    generated.mkdir(parents=True)
    (generated / "meson.build").touch()
    worktree = tmp_path / ".claude" / "worktrees" / "other"
    worktree.mkdir(parents=True)
    (worktree / "meson.build").touch()

    assert _collect_meson_files(tmp_path) == [str(tmp_path / "meson.build")]
