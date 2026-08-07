#!/usr/bin/env python3
"""
PreToolUse hook: block `gh pr merge` when CodeRabbit has unresolved review comments.

Reads the tool-use payload from stdin (Claude Code hook contract). If the
Bash command looks like a PR merge, defers to coderabbit_addressor.py --check
to decide whether to allow or block.

Exits:
    0 — allow
    2 — block with message on stderr (Claude Code treats as deny)
    anything else — treated as soft failure, allow-through with warning
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from typing import Any, cast

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


MERGE_RE = re.compile(r"\bgh\s+pr\s+merge\b")
# `gh pr merge [<number> | <url> | <branch>]` — we can only act on a number
# or a PR URL ending in one.
PR_ARG_RE = re.compile(
    r"\bgh\s+pr\s+merge\s+(?:[^\s]*/pull/)?(\d+)\b|"
    r"\bgh\s+pr\s+merge\b(?:\s+--?[^\s]+(?:[= ][^\s]+)?)*?\s+(?:[^\s]*/pull/)?(\d+)\b"
)
REPO_ARG_RE = re.compile(r"(?:--repo|-R)[= ]\s*([\w.-]+/[\w.-]+)")
# A PR URL carries its own owner/repo; without this a
# `gh pr merge https://github.com/OWNER/NAME/pull/N` would be checked
# against the current checkout instead of OWNER/NAME.
PR_URL_RE = re.compile(r"github\.com/([\w.-]+/[\w.-]+)/pull/\d+")


def _target(command: str) -> tuple[str | None, str | None]:
    """Extract the (pr_number, repo_slug) the merge command targets.

    Either may be None: `gh pr merge` with no number means "the PR for the
    current branch", and no `--repo` means "the current checkout".
    """
    pr_match = PR_ARG_RE.search(command)
    pr = None
    if pr_match:
        pr = pr_match.group(1) or pr_match.group(2)
    repo_match = REPO_ARG_RE.search(command) or PR_URL_RE.search(command)
    repo = repo_match.group(1) if repo_match else None
    return pr, repo


def main() -> int:
    try:
        payload = cast(dict[str, Any], json.load(sys.stdin))
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return 0

    tool_name = cast(str, payload.get("tool_name", ""))
    if tool_name != "Bash":
        return 0

    tool_input = cast(dict[str, Any], payload.get("tool_input", {}))
    command = cast(str, tool_input.get("command", ""))
    if not MERGE_RE.search(command):
        return 0

    # Check the PR the command actually targets. Without this the hook always
    # inspected the current checkout's branch, so merging a sibling project's
    # PR (`gh pr merge N --repo OWNER/NAME`) was judged against the wrong
    # repository — and blocked with "no PR found for current branch" whenever
    # the current branch had no PR of its own.
    pr, repo = _target(command)
    cmd = ["uv", "run", "python", "ci/tools/coderabbit_addressor.py", "--check"]
    if pr:
        cmd.append(pr)
    if repo and pr:
        cmd.extend(["--repo", repo])

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=30,
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return 0

    if result.returncode == 0:
        return 0

    # Exit 2 from the addressor means "could not identify a PR", not
    # "this PR has unresolved reviews". Blocking then is a false positive:
    # there is no review state to protect. Only a real unresolved-comment
    # verdict (exit 1) blocks.
    if result.returncode != 1:
        sys.stderr.write(result.stderr or "")
        sys.stderr.write(
            "\n[check_pr_merge_reviews] Could not identify a PR to check; "
            "allowing merge.\n"
        )
        return 0

    sys.stderr.write(result.stderr or "")
    sys.stderr.write(
        "\n[check_pr_merge_reviews] Blocking merge — run /address-reviews first.\n"
    )
    return 2


if __name__ == "__main__":
    sys.exit(main())
