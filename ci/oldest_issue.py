#!/usr/bin/env python3
"""Report open GitHub issues oldest-first, skipping deferred ones.

Backs the "work the oldest issue first" triage loop. Issues labeled
``deferred`` are ones a previous pass looked at and could not resolve; they
are skipped so the loop advances instead of re-picking the same issue.

Usage:
    uv run ci/oldest_issue.py                 # oldest actionable issue
    uv run ci/oldest_issue.py --list 20       # oldest 20, in age order
    uv run ci/oldest_issue.py --json          # machine-readable
    uv run ci/oldest_issue.py --max-age-days 365
    uv run ci/oldest_issue.py --defer 11 --reason "needs hardware"
"""

import argparse
import json
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone

from running_process import RunningProcess


DEFAULT_REPO = "FastLED/FastLED"
DEFERRED_LABEL = "deferred"

# gh caps --limit; issues are fetched in one page and sorted locally so that
# "oldest" is a property of the whole open set, not of one page.
FETCH_LIMIT = 1000


@dataclass
class Issue:
    number: int
    title: str
    created_at: datetime
    labels: list[str] = field(default_factory=list)

    @property
    def age_days(self) -> int:
        return (datetime.now(timezone.utc) - self.created_at).days

    @property
    def url(self) -> str:
        return f"https://github.com/{DEFAULT_REPO}/issues/{self.number}"

    def to_dict(self) -> dict[str, object]:
        return {
            "number": self.number,
            "title": self.title,
            "created_at": self.created_at.isoformat(),
            "age_days": self.age_days,
            "labels": self.labels,
        }


def _parse_created_at(raw: str) -> datetime:
    # gh emits RFC3339 with a trailing Z, which fromisoformat rejects before 3.11.
    return datetime.fromisoformat(raw.replace("Z", "+00:00"))


def fetch_issues(repo: str) -> list[Issue]:
    """Fetch all open issues via gh, sorted oldest-first."""
    result = RunningProcess.run(
        [
            "gh",
            "issue",
            "list",
            "--repo",
            repo,
            "--state",
            "open",
            "--limit",
            str(FETCH_LIMIT),
            "--json",
            "number,title,createdAt,labels",
        ],
        check=False,
        timeout=120,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"gh issue list failed ({result.returncode}): {result.stderr}"
        )

    issues = [
        Issue(
            number=row["number"],
            title=row["title"],
            created_at=_parse_created_at(row["createdAt"]),
            labels=[label["name"] for label in row.get("labels", [])],
        )
        for row in json.loads(result.stdout)
    ]
    issues.sort(key=lambda issue: issue.created_at)
    return issues


def filter_actionable(issues: list[Issue], max_age_days: int | None) -> list[Issue]:
    actionable = [i for i in issues if DEFERRED_LABEL not in i.labels]
    if max_age_days is not None:
        actionable = [i for i in actionable if i.age_days >= max_age_days]
    return actionable


def defer_issue(repo: str, number: int, reason: str | None) -> None:
    """Label an issue ``deferred`` so later scans skip it."""
    # Idempotent: gh creates the label only when it is missing.
    RunningProcess.run(
        [
            "gh",
            "label",
            "create",
            DEFERRED_LABEL,
            "--repo",
            repo,
            "--description",
            "Triage pass could not resolve this; skipped by ci/oldest_issue.py",
            "--color",
            "BFD4F2",
        ],
        check=False,
        timeout=60,
        capture_output=True,
        text=True,
    )
    result = RunningProcess.run(
        [
            "gh",
            "issue",
            "edit",
            str(number),
            "--repo",
            repo,
            "--add-label",
            DEFERRED_LABEL,
        ],
        check=False,
        timeout=60,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"failed to label #{number}: {result.stderr}")

    if reason:
        RunningProcess.run(
            [
                "gh",
                "issue",
                "comment",
                str(number),
                "--repo",
                repo,
                "--body",
                f"Deferred by automated triage: {reason}",
            ],
            check=False,
            timeout=60,
            capture_output=True,
            text=True,
        )
    print(f"Deferred #{number}" + (f": {reason}" if reason else ""))


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--repo", default=DEFAULT_REPO, help=f"owner/name (default: {DEFAULT_REPO})"
    )
    parser.add_argument(
        "--list",
        type=int,
        metavar="N",
        help="show the oldest N instead of just the first",
    )
    parser.add_argument("--json", action="store_true", help="emit JSON")
    parser.add_argument(
        "--max-age-days",
        type=int,
        help="only consider issues at least this old (e.g. 365 for the >1y backlog)",
    )
    parser.add_argument(
        "--include-deferred",
        action="store_true",
        help="do not skip deferred-labeled issues",
    )
    parser.add_argument(
        "--defer", type=int, metavar="NUMBER", help="label an issue deferred, then exit"
    )
    parser.add_argument("--reason", help="comment to leave alongside --defer")
    args = parser.parse_args()

    if args.defer is not None:
        defer_issue(args.repo, args.defer, args.reason)
        return 0

    issues = fetch_issues(args.repo)
    if args.include_deferred:
        candidates = (
            issues
            if args.max_age_days is None
            else [i for i in issues if i.age_days >= args.max_age_days]
        )
    else:
        candidates = filter_actionable(issues, args.max_age_days)

    deferred_count = sum(1 for i in issues if DEFERRED_LABEL in i.labels)

    if not candidates:
        if args.json:
            print(json.dumps({"issues": [], "deferred_skipped": deferred_count}))
        else:
            print("No actionable issues remaining.")
            if deferred_count:
                print(f"({deferred_count} deferred issue(s) skipped)")
        # Exit 1 so a driving loop can detect "backlog is done".
        return 1

    selected = candidates[: args.list] if args.list else candidates[:1]

    if args.json:
        print(
            json.dumps(
                {
                    "issues": [i.to_dict() for i in selected],
                    "total_open": len(issues),
                    "total_actionable": len(candidates),
                    "deferred_skipped": deferred_count,
                },
                indent=2,
            )
        )
        return 0

    for issue in selected:
        print(
            f"#{issue.number:<6} {issue.created_at:%Y-%m-%d}  ({issue.age_days}d)  {issue.title}"
        )
        print(f"        {issue.url}")

    print()
    print(
        f"{len(candidates)} actionable of {len(issues)} open; {deferred_count} deferred skipped"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
