import importlib.machinery
import importlib.util
from pathlib import Path
from types import ModuleType
from typing import Iterator

import pytest


SPEC: importlib.machinery.ModuleSpec | None = importlib.util.spec_from_file_location(
    "check_on_stop",
    Path(__file__).resolve().parents[1] / "hooks" / "check-on-stop.py",
)
assert SPEC and SPEC.loader
hook: ModuleType = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(hook)


def _blocked(count: int) -> dict[str, object]:
    return {"session_id": "s", "digest": "d", "count": count}


def test_release_after_cap_on_unchanged_tree() -> None:
    assert hook.should_release(True, "s", "d", _blocked(2))


def test_below_cap_still_blocks() -> None:
    assert not hook.should_release(True, "s", "d", _blocked(1))


def test_any_edit_resets() -> None:
    assert not hook.should_release(True, "s", "edited", _blocked(9))
    assert hook.next_block_state("s", "edited", _blocked(9))["count"] == 1


def test_new_session_is_not_released() -> None:
    assert not hook.should_release(True, "other", "d", _blocked(9))


def test_fresh_stop_is_not_released() -> None:
    assert not hook.should_release(False, "s", "d", _blocked(9))


def test_unknown_digest_or_session_never_releases() -> None:
    assert not hook.should_release(True, "s", None, _blocked(9))
    assert not hook.should_release(True, None, "d", _blocked(9))


def test_identical_block_counts_up() -> None:
    assert hook.next_block_state("s", "d", _blocked(1))["count"] == 2


def test_directive_rules_out_ownership_excuses() -> None:
    assert "must be a tool call" in hook.FIX_DIRECTIVE
    assert "even if you did not cause them" in hook.FIX_DIRECTIVE


def _run_main(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    stop_active: bool,
    checks_code: int,
    digest: str,
) -> int:
    payload: dict[str, object] = {"session_id": "s", "stop_hook_active": stop_active}
    monkeypatch.setattr(hook, "BLOCK_STATE_DIR", tmp_path / "blocks")
    monkeypatch.setattr(hook, "_read_stop_payload", lambda: payload)
    monkeypatch.setattr(hook, "_tree_digest", lambda: digest)
    monkeypatch.setattr(hook, "run_checks", lambda: checks_code)
    return hook.main()


def test_main_blocks_twice_then_releases_then_edit_blocks_again(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    assert _run_main(monkeypatch, tmp_path, False, 2, "d") == 2
    assert _run_main(monkeypatch, tmp_path, True, 2, "d") == 2
    assert _run_main(monkeypatch, tmp_path, True, 2, "d") == 0
    assert _run_main(monkeypatch, tmp_path, True, 2, "edited") == 2


def test_main_clears_state_once_checks_pass(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    _run_main(monkeypatch, tmp_path, False, 2, "d")
    code: int = _run_main(monkeypatch, tmp_path, True, 0, "d")
    assert code == 0 and not hook._block_state_file("s").exists()


def test_sessions_keep_separate_state_files() -> None:
    assert hook._block_state_file("a") != hook._block_state_file("b")


def test_block_is_counted_against_tree_checked_before_lint(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    digests: Iterator[str] = iter(["before", "reformatted"])
    payload: dict[str, object] = {"session_id": "s", "stop_hook_active": False}
    monkeypatch.setattr(hook, "BLOCK_STATE_DIR", tmp_path / "blocks")
    monkeypatch.setattr(hook, "_read_stop_payload", lambda: payload)
    monkeypatch.setattr(hook, "_tree_digest", lambda: next(digests))
    monkeypatch.setattr(hook, "run_checks", lambda: 2)
    assert hook.main() == 2
    assert hook._load_block_state("s")["digest"] == "before"


def test_state_write_failure_keeps_the_block(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    blocker: Path = tmp_path / "file"
    blocker.write_text("x")
    payload: dict[str, object] = {"session_id": "s", "stop_hook_active": False}
    monkeypatch.setattr(hook, "BLOCK_STATE_DIR", blocker / "blocks")
    monkeypatch.setattr(hook, "_read_stop_payload", lambda: payload)
    monkeypatch.setattr(hook, "_tree_digest", lambda: "d")
    monkeypatch.setattr(hook, "run_checks", lambda: 2)
    assert hook.main() == 2


def test_untracked_file_content_changes_digest(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    (tmp_path / "new.txt").write_text("one")
    monkeypatch.setattr(hook, "PROJECT_ROOT", tmp_path)

    class Result:
        returncode: int = 0

        def __init__(self, stdout: str) -> None:
            self.stdout = stdout

    monkeypatch.setattr(
        hook,
        "run_cmd",
        lambda cmd: Result("new.txt\0" if "ls-files" in cmd else ""),
    )
    first: str | None = hook._tree_digest()
    (tmp_path / "new.txt").write_text("two")
    assert hook._tree_digest() != first


def test_content_moved_between_untracked_files_changes_digest(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    (tmp_path / "a.txt").write_text("xy")
    (tmp_path / "b.txt").write_text("")
    monkeypatch.setattr(hook, "PROJECT_ROOT", tmp_path)

    class Result:
        returncode: int = 0

        def __init__(self, stdout: str) -> None:
            self.stdout = stdout

    monkeypatch.setattr(
        hook,
        "run_cmd",
        lambda cmd: Result("a.txt\0b.txt\0" if "ls-files" in cmd else ""),
    )
    first: str | None = hook._tree_digest()
    (tmp_path / "a.txt").write_text("x")
    (tmp_path / "b.txt").write_text("y")
    assert hook._tree_digest() != first
