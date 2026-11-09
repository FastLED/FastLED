"""Merge-queue gate: refuse a PR whose CodeRabbit review threads are unresolved.

The gate passes when every CodeRabbit review thread on the PR is resolved, or
when CodeRabbit is exhausted (its summary comment carries the rate-limit
marker), because then it cannot re-review the fixes and must not block the
queue forever.

Usage: uv run python ci/coderabbit_gate.py --repo OWNER/REPO (--pr N | --ref REF)
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass

from running_process import PIPE, RunningProcess


CODERABBIT_LOGINS = frozenset({"coderabbitai", "coderabbitai[bot]"})
RATE_LIMIT_MARKER = (
    "<!-- This is an auto-generated comment: rate limited by coderabbit.ai -->"
)
QUEUE_REF = re.compile(r"(?:refs/heads/)?gh-readonly-queue/[^/]+/pr-(\d+)-[0-9a-f]+$")

THREADS_QUERY = """
query($owner: String!, $name: String!, $pr: Int!, $after: String) {
  repository(owner: $owner, name: $name) {
    pullRequest(number: $pr) {
      reviewThreads(first: 100, after: $after) {
        pageInfo { hasNextPage endCursor }
        nodes {
          isResolved
          path
          line
          comments(first: 1) { nodes { author { login } url body } }
        }
      }
    }
  }
}
"""


@dataclass(frozen=True)
class Thread:
    author: str
    resolved: bool
    path: str
    url: str
    title: str


@dataclass(frozen=True)
class Verdict:
    ok: bool
    reason: str
    open_threads: tuple[Thread, ...] = ()


def pr_from_ref(ref: str) -> int | None:
    match = QUEUE_REF.search(ref)
    return int(match.group(1)) if match else None


def is_exhausted(comment_bodies: list[str]) -> bool:
    return any(RATE_LIMIT_MARKER in body for body in comment_bodies)


def decide(threads: list[Thread], coderabbit_comments: list[str]) -> Verdict:
    open_threads = tuple(
        t for t in threads if t.author in CODERABBIT_LOGINS and not t.resolved
    )
    if not open_threads:
        return Verdict(True, "no unresolved CodeRabbit threads")
    if is_exhausted(coderabbit_comments):
        return Verdict(
            True,
            f"{len(open_threads)} unresolved CodeRabbit thread(s), but CodeRabbit is "
            "rate limited; letting the PR pass",
            open_threads,
        )
    return Verdict(
        False,
        f"{len(open_threads)} unresolved CodeRabbit thread(s); address and resolve them",
        open_threads,
    )


def _gh_json(args: list[str]) -> object:
    result = RunningProcess.run(
        ["gh", *args],
        check=False,
        stdout=PIPE,
        stderr=PIPE,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"gh {' '.join(args[:2])} exited {result.returncode}: {result.stderr}"
        )
    return json.loads(result.stdout)


def _first_line(body: str) -> str:
    for line in body.splitlines():
        text = line.strip().strip("_*`").strip()
        if text and not text.startswith("<!--"):
            return text[:120]
    return ""


def fetch_threads(repo: str, pr: int) -> list[Thread]:
    owner, name = repo.split("/", 1)
    threads: list[Thread] = []
    after: str | None = None
    while True:
        args = [
            "api",
            "graphql",
            "-f",
            f"query={THREADS_QUERY}",
            "-F",
            f"owner={owner}",
            "-F",
            f"name={name}",
            "-F",
            f"pr={pr}",
        ]
        if after:
            args += ["-F", f"after={after}"]
        data = _gh_json(args)
        if not isinstance(data, dict):
            raise ValueError(f"unexpected GraphQL response: {data!r}")
        page = data["data"]["repository"]["pullRequest"]["reviewThreads"]
        for node in page["nodes"]:
            first = (node["comments"]["nodes"] or [{}])[0]
            author = ((first.get("author") or {}).get("login")) or ""
            threads.append(
                Thread(
                    author=author,
                    resolved=bool(node["isResolved"]),
                    path=f"{node.get('path')}:{node.get('line')}",
                    url=first.get("url", ""),
                    title=_first_line(first.get("body", "")),
                )
            )
        if not page["pageInfo"]["hasNextPage"]:
            return threads
        after = page["pageInfo"]["endCursor"]


def fetch_coderabbit_comments(repo: str, pr: int) -> list[str]:
    data = _gh_json(
        ["api", "--paginate", "--slurp", f"repos/{repo}/issues/{pr}/comments"]
    )
    pages = data if isinstance(data, list) else [data]
    return [
        c.get("body") or ""
        for page in pages
        for c in (page if isinstance(page, list) else [page])
        if (c.get("user") or {}).get("login") in CODERABBIT_LOGINS
    ]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--pr", type=int)
    group.add_argument(
        "--ref", help="merge-queue ref, e.g. gh-readonly-queue/master/pr-12-<sha>"
    )
    args = parser.parse_args(argv)

    pr = args.pr if args.pr is not None else pr_from_ref(args.ref)
    if pr is None:
        print(
            f"coderabbit gate: {args.ref!r} is not a merge-queue ref", file=sys.stderr
        )
        return 2

    verdict = decide(
        fetch_threads(args.repo, pr), fetch_coderabbit_comments(args.repo, pr)
    )
    print(
        f"coderabbit gate (PR #{pr}): {'PASS' if verdict.ok else 'FAIL'}: {verdict.reason}"
    )
    for t in verdict.open_threads:
        print(f"  - {t.path} {t.title} {t.url}")
    return 0 if verdict.ok else 1


if __name__ == "__main__":
    sys.exit(main())
