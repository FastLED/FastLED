"""Tests for the `gh pr merge` review-guard hook's target resolution.

Regression coverage for the false positive where the hook always inspected
the current checkout's branch, so merging a sibling project's PR
(`gh pr merge N --repo OWNER/NAME`) was judged against the wrong repository
and blocked with "no PR found for current branch".
"""

from __future__ import annotations

import importlib.util
from pathlib import Path
from typing import Any

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
HOOK_PATH = REPO_ROOT / "ci" / "hooks" / "check_pr_merge_reviews.py"


def _load_hook() -> Any:
    spec = importlib.util.spec_from_file_location("check_pr_merge_reviews", HOOK_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize(
    "command,expected",
    [
        # The case that motivated the fix: a sibling repo's PR.
        (
            "gh pr merge 1268 --repo FastLED/fbuild --squash --admin",
            ("1268", "FastLED/fbuild"),
        ),
        # --repo before the number.
        ("gh pr merge --repo FastLED/fbuild 1268 --squash", ("1268", "FastLED/fbuild")),
        # --repo=VALUE form.
        ("gh pr merge --repo=FastLED/fbuild 1268 --merge", ("1268", "FastLED/fbuild")),
        # A PR URL carries its own owner/repo — must not fall back to the
        # current checkout, or we would check the wrong repository.
        (
            "gh pr merge https://github.com/FastLED/fbuild/pull/1268 --squash",
            ("1268", "FastLED/fbuild"),
        ),
        # Same-repo merge by number: no --repo means current checkout.
        ("gh pr merge 3866 --squash --delete-branch", ("3866", None)),
        # No number at all: "the PR for the current branch".
        ("gh pr merge --squash --admin", (None, None)),
    ],
)
def test_target_extracts_pr_and_repo(
    command: str, expected: tuple[str | None, str | None]
) -> None:
    hook = _load_hook()
    assert hook._target(command) == expected


def test_non_merge_commands_are_ignored() -> None:
    """The guard must only engage on an actual merge."""
    hook = _load_hook()
    assert not hook.MERGE_RE.search("gh pr view 1268 --repo FastLED/fbuild")
    assert not hook.MERGE_RE.search("gh pr create --title x")
    assert hook.MERGE_RE.search("gh pr merge 1268")


# ---------------------------------------------------------------------------
# `cd <dir> && gh pr merge N` — the repo comes from the directory, not the
# checkout the hook happens to run in.
# ---------------------------------------------------------------------------


def _make_repo(tmp_path: Any, name: str, origin: str) -> str:
    """A real git repo with an origin, so `_repo_at` is exercised rather than
    mocked. The hook shells out to `git config`; a fake would not prove it
    parses what git actually prints."""
    import subprocess

    path = tmp_path / name
    path.mkdir()
    subprocess.run(["git", "init", "-q"], cwd=path, check=True)  # noqa: SRC001
    subprocess.run(["git", "remote", "add", "origin", origin], cwd=path, check=True)  # noqa: SRC001
    return str(path)


def test_cd_prefix_resolves_the_repo_from_that_directory(tmp_path: Any) -> None:
    """The false positive this fixes: merging another project's PR from its
    worktree was judged against *this* checkout. clud#1150 and
    FastLED/FastLED#1150 are unrelated PRs that happen to share a number, and
    the hook blocked the clud merge citing the FastLED one."""
    hook = _load_hook()
    other = _make_repo(tmp_path, "clud-wt", "https://github.com/zackees/clud.git")

    pr, repo = hook._target(f"cd {other} && gh pr merge 1150 --squash --delete-branch")

    assert pr == "1150"
    assert repo == "zackees/clud", "must not fall back to the current checkout"


def test_ssh_remote_urls_resolve(tmp_path: Any) -> None:
    hook = _load_hook()
    other = _make_repo(tmp_path, "ssh-wt", "git@github.com:zackees/clud.git")

    _, repo = hook._target(f"cd {other} && gh pr merge 7 --squash")

    assert repo == "zackees/clud"


def test_explicit_repo_still_wins_over_the_cd(tmp_path: Any) -> None:
    """`--repo` is what the user actually asked for; a `cd` earlier in the
    chain must not override it."""
    hook = _load_hook()
    other = _make_repo(tmp_path, "wt", "https://github.com/zackees/clud.git")

    _, repo = hook._target(
        f"cd {other} && gh pr merge 1268 --repo FastLED/fbuild --squash"
    )

    assert repo == "FastLED/fbuild"


def test_no_cd_is_unchanged(tmp_path: Any) -> None:
    """The ordinary same-repo merge keeps resolving to the current checkout,
    so this fix cannot weaken the guard it exists to enforce."""
    hook = _load_hook()

    assert hook._target("gh pr merge 3866 --squash --delete-branch") == ("3866", None)


def test_a_cd_to_a_non_repo_is_allowed_rather_than_misjudged(tmp_path: Any) -> None:
    """If the directory is not a git checkout we cannot name the target repo.
    Judging the number against *this* checkout is exactly the original bug, so
    the hook declines instead."""
    hook = _load_hook()
    plain = tmp_path / "not-a-repo"
    plain.mkdir()

    _, repo = hook._target(f"cd {plain} && gh pr merge 1150 --squash")

    assert repo is None


def test_addressor_crash_exits_2_not_1(tmp_path: Any) -> None:
    """A crash in the checker must not read as "unresolved reviews".

    An unhandled traceback exits 1, which is precisely the code the hook
    treats as a real verdict — so a `gh` failure, a network blip, or a PR
    number that does not exist in the resolved repo all blocked the merge
    while claiming there were review comments to address.
    """
    import subprocess
    import sys as _sys

    addressor = REPO_ROOT / "ci" / "tools" / "coderabbit_addressor.py"
    # A PR number that cannot exist drives `gh api` to fail inside the check.
    result = subprocess.run(  # noqa: SRC001
        [
            _sys.executable,
            str(addressor),
            "--check",
            "999999999",
            "--repo",
            "FastLED/FastLED",
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )

    assert result.returncode != 1, (
        "exit 1 is the hook's 'unresolved reviews' verdict; an internal "
        f"failure must not use it. stderr:\n{result.stderr[-600:]}"
    )
