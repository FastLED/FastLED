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
