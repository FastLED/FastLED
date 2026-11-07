import importlib.util
from pathlib import Path


SPEC = importlib.util.spec_from_file_location(
    "check_on_stop",
    Path(__file__).resolve().parents[1] / "hooks" / "check-on-stop.py",
)
assert SPEC and SPEC.loader
hook = importlib.util.module_from_spec(SPEC)
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


def _run_main(monkeypatch, tmp_path, stop_active: bool, checks_code: int, digest: str):
    state = tmp_path / "block.json"
    payload = {"session_id": "s", "stop_hook_active": stop_active}
    monkeypatch.setattr(hook, "BLOCK_STATE_FILE", state)
    monkeypatch.setattr(hook, "_read_stop_payload", lambda: payload)
    monkeypatch.setattr(hook, "_tree_digest", lambda: digest)
    monkeypatch.setattr(hook, "run_checks", lambda: checks_code)
    return hook.main(), state


def test_main_blocks_twice_then_releases_then_edit_blocks_again(
    monkeypatch, tmp_path
) -> None:
    assert _run_main(monkeypatch, tmp_path, False, 2, "d")[0] == 2
    assert _run_main(monkeypatch, tmp_path, True, 2, "d")[0] == 2
    assert _run_main(monkeypatch, tmp_path, True, 2, "d")[0] == 0
    assert _run_main(monkeypatch, tmp_path, True, 2, "edited")[0] == 2


def test_main_clears_state_once_checks_pass(monkeypatch, tmp_path) -> None:
    _run_main(monkeypatch, tmp_path, False, 2, "d")
    code, state = _run_main(monkeypatch, tmp_path, True, 0, "d")
    assert code == 0 and not state.exists()
