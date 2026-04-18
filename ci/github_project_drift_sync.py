"""Reconcile FastLED Tracker project items against live repo state.

Safety net behind the event-driven add-to-project workflows: runs on a cron
and adds any open issue/PR from the feeder repos that isn't already in the
project. `addProjectV2ItemById` is idempotent, so re-adds cost nothing.

Why we need this: the per-repo `add-to-project.yml` (and
`project_automation.yml` on FastLED) listen for `issues.opened` and
`pull_request_target.opened`. Anything that slips past — workflow outage,
revoked App key, event delivery failure, items converted from discussion,
renames, etc. — eventually drifts. This drift-sync catches it up.

Environment variables (set by the workflow):
  GH_TOKEN           GitHub App installation token with Projects: read/write,
                     Contents/Issues/Pull requests: read.
  PROJECT_OWNER      Org login (e.g. "FastLED").
  PROJECT_NUMBER     Project number (e.g. "1").
  FEEDER_REPOS       Comma-separated list of repo names (owner implied).
  INCLUDE_CLOSED     "true" to include closed/merged items (default: "false").
                     Closed historical items bloat the project board; leave off
                     for routine runs, enable on manual triggers if desired.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from typing import Any


def run_gh(args: list[str]) -> str:
    result = subprocess.run(
        ["gh", *args],
        capture_output=True,
        text=True,
        check=False,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        print(f"FAIL: gh {' '.join(args)}: {result.stderr}", file=sys.stderr)
        sys.exit(1)
    return result.stdout or ""


def graphql(query: str, **variables: Any) -> dict[str, Any]:
    cmd = ["api", "graphql", "-f", f"query={query}"]
    for k, v in variables.items():
        cmd.extend(["-F", f"{k}={v}"])
    out = run_gh(cmd)
    return json.loads(out)  # type: ignore[no-any-return]


def resolve_project_node_id(owner: str, number: int) -> str:
    q = """
    query($owner: String!, $number: Int!) {
      organization(login: $owner) {
        projectV2(number: $number) { id }
      }
    }
    """
    d = graphql(q, owner=owner, number=number)
    node_id = d["data"]["organization"]["projectV2"]["id"]
    assert isinstance(node_id, str)
    return node_id


def fetch_project_content_keys(owner: str, number: int) -> set[tuple[str, int]]:
    """Return {(repo_name, issue_or_pr_number), ...} for everything currently in the project."""
    keys: set[tuple[str, int]] = set()
    cursor: str | None = None
    while True:
        after = f'"{cursor}"' if cursor else "null"
        q = (
            'query { organization(login: "'
            + owner
            + '") { projectV2(number: '
            + str(number)
            + ") { items(first: 100, after: "
            + after
            + ") { pageInfo { hasNextPage endCursor } nodes { content { "
            "... on Issue { number repository { name } } "
            "... on PullRequest { number repository { name } } "
            "} } } } } }"
        )
        d = graphql(q)
        items = d["data"]["organization"]["projectV2"]["items"]
        for n in items["nodes"]:
            c = n.get("content")
            if not c:
                continue
            repo = (c.get("repository") or {}).get("name")
            num = c.get("number")
            if repo and isinstance(num, int):
                keys.add((repo, num))
        page = items["pageInfo"]
        if not page.get("hasNextPage"):
            break
        cursor = page["endCursor"]
    return keys


def fetch_repo_items(
    owner: str, repo: str, include_closed: bool
) -> list[tuple[str, int, str]]:
    """Return [(node_id, number, kind), ...] for issues+PRs in the repo."""
    out: list[tuple[str, int, str]] = []
    state_flag = "all" if include_closed else "open"
    for kind, cmd in [("issue", "issue"), ("pr", "pr")]:
        raw = run_gh(
            [
                cmd,
                "list",
                "--repo",
                f"{owner}/{repo}",
                "--state",
                state_flag,
                "--limit",
                "1000",
                "--json",
                "id,number",
            ]
        )
        items = json.loads(raw) if raw.strip() else []
        for i in items:
            out.append((str(i["id"]), int(i["number"]), kind))
    return out


def add_to_project(project_id: str, content_id: str) -> str | None:
    mutation = (
        "mutation($project: ID!, $content: ID!) { "
        "addProjectV2ItemById(input: {projectId: $project, contentId: $content}) { "
        "item { id } } }"
    )
    try:
        d = graphql(mutation, project=project_id, content=content_id)
    except SystemExit:
        return None
    item = (d.get("data") or {}).get("addProjectV2ItemById", {}).get("item") or {}
    node_id = item.get("id")
    return str(node_id) if node_id else None


def main() -> int:
    owner = os.environ.get("PROJECT_OWNER", "").strip()
    number_raw = os.environ.get("PROJECT_NUMBER", "").strip()
    feeders_raw = os.environ.get("FEEDER_REPOS", "").strip()
    include_closed = os.environ.get("INCLUDE_CLOSED", "false").lower() == "true"

    if not owner or not number_raw or not feeders_raw:
        print(
            "PROJECT_OWNER, PROJECT_NUMBER, FEEDER_REPOS must all be set",
            file=sys.stderr,
        )
        return 2
    number = int(number_raw)
    feeders = [r.strip() for r in feeders_raw.split(",") if r.strip()]

    print(f"Project: {owner}/projects/{number}")
    print(f"Feeders: {feeders}")
    print(f"Include closed: {include_closed}")

    project_id = resolve_project_node_id(owner, number)
    print(f"Project node: {project_id}")

    existing = fetch_project_content_keys(owner, number)
    print(f"Project currently has {len(existing)} linked items")

    added = 0
    failed = 0
    for repo in feeders:
        repo_items = fetch_repo_items(owner, repo, include_closed)
        missing = [
            (node_id, num, kind)
            for (node_id, num, kind) in repo_items
            if (repo, num) not in existing
        ]
        print(f"\n=== {owner}/{repo} ===")
        print(
            f"  repo items (state={'all' if include_closed else 'open'}): {len(repo_items)}"
        )
        print(f"  missing from project: {len(missing)}")
        for node_id, num, kind in missing:
            item_id = add_to_project(project_id, node_id)
            if item_id is None:
                failed += 1
                print(f"  FAIL  {owner}/{repo}#{num} ({kind})")
            else:
                added += 1

    print(f"\nDone. Added: {added}, failed: {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
