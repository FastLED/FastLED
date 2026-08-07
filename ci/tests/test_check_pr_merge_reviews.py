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
