#!/usr/bin/env python3
"""
CodeRabbit review-comment addressor.

Drives the /address-reviews skill. Three modes:

  --plan              Fetch and classify open CodeRabbit comments on a PR.
  --reply <id> <msg>  Post a reply to a specific review-comment thread.
  --check             Exit non-zero if any unresolved CodeRabbit comments remain
                      (used by the pre-merge hook).

PR number is inferred from the current branch's PR if not given.

Usage:
    uv run ci/tools/coderabbit_addressor.py --plan
    uv run ci/tools/coderabbit_addressor.py 2266 --plan
    uv run ci/tools/coderabbit_addressor.py 2266 --reply 123456 "Fixed in abc1234"
    uv run ci/tools/coderabbit_addressor.py --check
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from typing import Any, Optional, cast


CODERABBIT_LOGINS = {"coderabbitai", "coderabbitai[bot]"}

SECURITY_KEYWORDS = [
    "security",
    "cve",
    # Spelled out rather than a bare "auth" so that "author"/"authored" -- which
    # CodeRabbit uses routinely in ordinary prose -- does not read as security.
    "authentication",
    "authorization",
    "authn",
    "authz",
    "credential",
    "secret",
    "rce",
    "injection",
    "critical",
    "data loss",
    "uaf",
    "use-after-free",
    "buffer overflow",
    "out-of-bounds",
]

# Anchored at the start of a word, open-ended at the end.
#
# The leading \b is the fix: plain substring matching made "rce" fire inside
# "Sou(rce)" -- and every guideline-sourced CodeRabbit comment ends with
# "_Source: Coding guidelines_" -- as well as inside "enfo(rce)s" and
# "resou(rce)s". That routed ordinary maintainability nits into security-flag,
# where the skill forbids auto-fixing and --check counts them as permanently
# unresolved, blocking merge forever.
#
# The trailing \w* is equally load-bearing: a closing \b would drop the plurals
# and inflections these findings actually use -- "leaks secrets", "hardcoded
# credentials", "breaks authentication". Those must stay in security-flag, so
# match the keyword as a word *prefix*, not as a whole word.
_SECURITY_RE = re.compile(
    r"\b(?:" + "|".join(re.escape(kw) for kw in SECURITY_KEYWORDS) + r")\w*"
)

# A bare "auth" on its own is still worth flagging; "author" is not.
_BARE_AUTH_RE = re.compile(r"\bauth\b")

# CodeRabbit appends boilerplate to every comment: HTML marker comments, a
# collapsed "Prompt for AI Agents" / "Committable suggestion" <details> block,
# and quoted coding-guideline text. None of it describes the finding, but it is
# full of trigger words, so classification looks at the prose only.
_BOILERPLATE_RE = re.compile(
    r"<!--.*?-->|<details>.*?</details>",
    re.DOTALL | re.IGNORECASE,
)


def _classifiable_text(body: str) -> str:
    """Strip CodeRabbit boilerplate so keywords match the finding, not the footer."""
    return _BOILERPLATE_RE.sub(" ", body)


STYLE_KEYWORDS = [
    "nit:",
    "style",
    "formatting",
    "prefer ",
    "consider renaming",
    "minor:",
]

MAX_ITERATIONS = 3


@dataclass
class Comment:
    id: int
    author: str
    body: str
    path: str
    line: Optional[int]
    in_reply_to: Optional[int]

    @property
    def classification(self) -> str:
        prose_lower = _classifiable_text(self.body).lower()
        if _SECURITY_RE.search(prose_lower) or _BARE_AUTH_RE.search(prose_lower):
            return "security-flag"
        if re.search(r"```suggestion", self.body):
            return "valid-fix"
        for kw in STYLE_KEYWORDS:
            if kw in prose_lower:
                return "style"
        if any(
            tok in prose_lower
            for tok in (
                "should be",
                "bug:",
                "error:",
                "issue:",
                "missing ",
                "incorrect",
            )
        ):
            return "valid-fix"
        return "false-positive"


# Repo slug to operate on, when the caller targets a repository other than
# the checkout we are running inside (e.g. the merge hook inspecting a
# `gh pr merge --repo OWNER/NAME` for a sibling project). None means
# "resolve from the current checkout".
_REPO_OVERRIDE: Optional[str] = None


def _run_gh(args: list[str]) -> str:
    result = subprocess.run(
        ["gh"] + args,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=True,
    )
    return result.stdout


def _current_pr() -> Optional[int]:
    try:
        out = _run_gh(["pr", "view", "--json", "number"])
    except subprocess.CalledProcessError:
        return None
    return int(json.loads(out)["number"])


def _repo_slug() -> str:
    if _REPO_OVERRIDE:
        return _REPO_OVERRIDE
    out = _run_gh(["repo", "view", "--json", "nameWithOwner"])
    return json.loads(out)["nameWithOwner"]


def fetch_comments(pr: int) -> list[Comment]:
    # Returns ALL review comments on the PR (both CodeRabbit and human),
    # because _is_resolved needs to see human replies to detect that a
    # CodeRabbit thread has been addressed. Top-level filtering to
    # CodeRabbit-authored threads happens in plan().
    repo = _repo_slug()
    out = _run_gh(["api", f"repos/{repo}/pulls/{pr}/comments", "--paginate"])
    raw = cast(list[dict[str, Any]], json.loads(out))
    comments: list[Comment] = []
    for c in raw:
        user = cast(dict[str, Any], c.get("user") or {})
        author = cast(str, user.get("login", ""))
        comments.append(
            Comment(
                id=int(c["id"]),
                author=author,
                body=cast(str, c.get("body", "")),
                path=cast(str, c.get("path", "")),
                line=cast(
                    Optional[int],
                    c.get("line") or c.get("original_line"),
                ),
                in_reply_to=cast(Optional[int], c.get("in_reply_to_id")),
            )
        )
    return comments


def _is_resolved(comment: Comment, all_comments: list[Comment]) -> bool:
    # Resolution heuristic: any non-CodeRabbit author has replied in-thread.
    # CodeRabbit acknowledgement replies that arrive after a human reply
    # (e.g. "thanks for the update") must not un-resolve the thread, so we
    # cannot use "latest reply is human" — CodeRabbit consistently gets the
    # last word a few seconds later.
    for c in all_comments:
        if c.in_reply_to == comment.id and c.author not in CODERABBIT_LOGINS:
            return True
    return False


def plan(pr: int) -> dict[str, Any]:
    comments = fetch_comments(pr)
    top_level = [
        c for c in comments if c.in_reply_to is None and c.author in CODERABBIT_LOGINS
    ]
    buckets: dict[str, list[dict[str, Any]]] = {
        "valid-fix": [],
        "style": [],
        "false-positive": [],
        "security-flag": [],
    }
    for c in top_level:
        if _is_resolved(c, comments):
            continue
        buckets[c.classification].append(
            {
                "id": c.id,
                "path": c.path,
                "line": c.line,
                "body": c.body[:400],
            }
        )
    return {
        "pr": pr,
        "total_open": sum(len(v) for v in buckets.values()),
        "buckets": buckets,
        "max_iterations": MAX_ITERATIONS,
    }


def reply(pr: int, comment_id: int, message: str) -> None:
    repo = _repo_slug()
    _run_gh(
        [
            "api",
            "--method",
            "POST",
            f"repos/{repo}/pulls/{pr}/comments/{comment_id}/replies",
            "-f",
            f"body={message}",
        ]
    )


def check(pr: int) -> int:
    """Exit non-zero if any unresolved CodeRabbit comments remain. Used by hook."""
    p = plan(pr)
    buckets = cast(dict[str, list[dict[str, Any]]], p["buckets"])
    unresolved = buckets["valid-fix"] + buckets["security-flag"]
    if unresolved:
        print(
            f"[address-reviews] {len(unresolved)} unresolved CodeRabbit "
            f"comment(s) on PR #{pr}. Run /address-reviews before merging.",
            file=sys.stderr,
        )
        return 1
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "pr", nargs="?", type=int, help="PR number (default: current branch)"
    )
    ap.add_argument(
        "--repo",
        metavar="OWNER/NAME",
        help=(
            "Repository to inspect, when it is not the current checkout "
            "(e.g. a sibling project). Requires an explicit PR number, since "
            "'the PR for the current branch' is meaningless across repos."
        ),
    )
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--plan", action="store_true")
    group.add_argument("--reply", nargs=2, metavar=("COMMENT_ID", "MESSAGE"))
    group.add_argument("--check", action="store_true")
    args = ap.parse_args()

    if args.repo:
        if args.pr is None:
            print(
                "[address-reviews] --repo requires an explicit PR number",
                file=sys.stderr,
            )
            return 2
        global _REPO_OVERRIDE
        _REPO_OVERRIDE = args.repo

    pr = args.pr or _current_pr()
    if pr is None:
        print("[address-reviews] no PR found for current branch", file=sys.stderr)
        return 2

    if args.plan:
        print(json.dumps(plan(pr), indent=2))
        return 0
    if args.reply:
        reply(pr, int(args.reply[0]), args.reply[1])
        return 0
    if args.check:
        return check(pr)
    return 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        raise
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - the exit code is the point
        # Exit 2, not the 1 an unhandled traceback would produce. The merge
        # hook reads 1 as "this PR has unresolved review comments" and blocks
        # on it; a crash in here is not that. Letting the traceback out made
        # every internal failure — a `gh` error, a network blip, a PR number
        # that does not exist in this repo — indistinguishable from a real
        # verdict, and it blocked the merge either way.
        import traceback

        traceback.print_exc()
        print(
            "[address-reviews] internal error (above); not a review verdict",
            file=sys.stderr,
        )
        sys.exit(2)
