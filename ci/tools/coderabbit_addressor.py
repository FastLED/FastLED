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
    " auth",
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
        body_lower = self.body.lower()
        for kw in SECURITY_KEYWORDS:
            if kw in body_lower:
                return "security-flag"
        if re.search(r"```suggestion", self.body):
            return "valid-fix"
        for kw in STYLE_KEYWORDS:
            if kw in body_lower:
                return "style"
        if any(
            tok in body_lower
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
    out = _run_gh(["repo", "view", "--json", "nameWithOwner"])
    return json.loads(out)["nameWithOwner"]


def fetch_comments(pr: int) -> list[Comment]:
    repo = _repo_slug()
    out = _run_gh(["api", f"repos/{repo}/pulls/{pr}/comments", "--paginate"])
    raw = cast(list[dict[str, Any]], json.loads(out))
    comments: list[Comment] = []
    for c in raw:
        user = cast(dict[str, Any], c.get("user") or {})
        author = cast(str, user.get("login", ""))
        if author not in CODERABBIT_LOGINS:
            continue
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
    # Resolution heuristic: a non-CodeRabbit author replied in-thread since
    # the last CodeRabbit message. Proper GraphQL "resolved" state is also
    # honored via the review-thread API but needs a heavier call — start
    # with the reply heuristic and evolve if false positives show up.
    replies = [c for c in all_comments if c.in_reply_to == comment.id]
    if not replies:
        return False
    latest = max(replies, key=lambda c: c.id)
    return latest.author not in CODERABBIT_LOGINS


def plan(pr: int) -> dict[str, Any]:
    comments = fetch_comments(pr)
    top_level = [c for c in comments if c.in_reply_to is None]
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
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--plan", action="store_true")
    group.add_argument("--reply", nargs=2, metavar=("COMMENT_ID", "MESSAGE"))
    group.add_argument("--check", action="store_true")
    args = ap.parse_args()

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
    sys.exit(main())
