"""Reconcile FastLED Tracker project items against live repo state.

Two jobs, both safety nets behind the event-driven workflows:

  1. Add any open issue/PR from the feeder repos that isn't yet in the project.
  2. Set Status=Triage on any item currently in the project with no Status —
     otherwise the project board (grouped by Status) hides them from every
     column because they don't match any lane.

Both mutations are idempotent.

Env vars (set by the workflow):
  GH_TOKEN               GitHub App installation token with Projects: read/write,
                         Contents/Issues/Pull requests: read.
  PROJECT_OWNER          Org login (e.g. "FastLED").
  PROJECT_NUMBER         Project number (e.g. "1").
  FEEDER_REPOS           Comma-separated list of repo names (owner implied).
  INCLUDE_CLOSED         "true" to include closed/merged items. Default "false"
                         — closed items bloat the board; enable on rare manual
                         runs if you want a full historical backfill.
  STATUS_FIELD_NAME      Name of the Status single-select field. Default "Status".
  TRIAGE_OPTION_NAME     Name of the triage option. Default "Triage".
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


def fetch_project_items(
    owner: str, number: int, status_field_name: str
) -> tuple[set[tuple[str, int]], list[str]]:
    """Return (content_keys, items_without_status).

    content_keys: {(repo_name, issue_or_pr_number), ...} — for the
      "is this linked to the project already?" check.
    items_without_status: [item_id, ...] — existing project items whose
      Status field is unset, needing a Triage assignment.
    """
    keys: set[tuple[str, int]] = set()
    no_status: list[str] = []
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
            + ") { pageInfo { hasNextPage endCursor } "
            "nodes { id content { "
            "... on Issue { number repository { name } } "
            "... on PullRequest { number repository { name } } "
            "} "
            "fieldValues(first: 20) { nodes { "
            "... on ProjectV2ItemFieldSingleSelectValue { "
            "name field { ... on ProjectV2FieldCommon { name } } } "
            "} } } } } } }"
        )
        d = graphql(q)
        items = d["data"]["organization"]["projectV2"]["items"]
        for n in items["nodes"]:
            c = n.get("content")
            if c:
                repo = (c.get("repository") or {}).get("name")
                num = c.get("number")
                if repo and isinstance(num, int):
                    keys.add((repo, num))
            # Does this item have a Status set?
            has_status = False
            for fv in (n.get("fieldValues") or {}).get("nodes") or []:
                if fv and (fv.get("field") or {}).get("name") == status_field_name:
                    has_status = True
                    break
            if not has_status:
                no_status.append(str(n["id"]))
        page = items["pageInfo"]
        if not page.get("hasNextPage"):
            break
        cursor = page["endCursor"]
    return keys, no_status


def fetch_status_field(
    owner: str, number: int, status_field_name: str, triage_option_name: str
) -> tuple[str, str]:
    """Return (field_id, triage_option_id) for the Status single-select field."""
    q = (
        'query { organization(login: "'
        + owner
        + '") { projectV2(number: '
        + str(number)
        + ') { field(name: "'
        + status_field_name
        + '") { ... on ProjectV2SingleSelectField { id options { id name } } } } } }'
    )
    d = graphql(q)
    field = d["data"]["organization"]["projectV2"]["field"]
    if not field:
        raise RuntimeError(f"Status field '{status_field_name}' not found")
    field_id = str(field["id"])
    option_id: str | None = None
    for o in field["options"]:
        if o["name"] == triage_option_name:
            option_id = str(o["id"])
            break
    if option_id is None:
        raise RuntimeError(f"Option '{triage_option_name}' not found on Status field")
    return field_id, option_id


def set_status_triage(
    project_id: str, item_id: str, field_id: str, option_id: str
) -> bool:
    mutation = (
        "mutation($p: ID!, $i: ID!, $f: ID!, $o: String!) { "
        "updateProjectV2ItemFieldValue(input: "
        "{projectId: $p, itemId: $i, fieldId: $f, "
        "value: {singleSelectOptionId: $o}}) { projectV2Item { id } } }"
    )
    try:
        d = graphql(mutation, p=project_id, i=item_id, f=field_id, o=option_id)
    except SystemExit:
        return False
    return bool(
        (d.get("data") or {})
        .get("updateProjectV2ItemFieldValue", {})
        .get("projectV2Item")
    )


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
    status_field_name = (
        os.environ.get("STATUS_FIELD_NAME", "Status").strip() or "Status"
    )
    triage_option_name = (
        os.environ.get("TRIAGE_OPTION_NAME", "Triage").strip() or "Triage"
    )

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

    field_id, triage_option_id = fetch_status_field(
        owner, number, status_field_name, triage_option_name
    )
    print(
        f"Status field: {field_id[:20]}...  '{triage_option_name}' option: "
        f"{triage_option_id[:10]}"
    )

    existing_keys, items_without_status = fetch_project_items(
        owner, number, status_field_name
    )
    print(f"Project currently has {len(existing_keys)} linked items")
    print(f"  (of which {len(items_without_status)} have no Status set)")

    added = 0
    failed = 0
    status_set = 0
    status_failed = 0

    # ---- Part 1: add missing items ----
    new_item_ids: list[str] = []
    for repo in feeders:
        repo_items = fetch_repo_items(owner, repo, include_closed)
        missing = [
            (node_id, num, kind)
            for (node_id, num, kind) in repo_items
            if (repo, num) not in existing_keys
        ]
        print(f"\n=== {owner}/{repo} ===")
        print(
            f"  repo items (state={'all' if include_closed else 'open'}): "
            f"{len(repo_items)}"
        )
        print(f"  missing from project: {len(missing)}")
        for node_id, num, kind in missing:
            item_id = add_to_project(project_id, node_id)
            if item_id is None:
                failed += 1
                print(f"  FAIL  {owner}/{repo}#{num} ({kind})")
            else:
                added += 1
                new_item_ids.append(item_id)

    # ---- Part 2: set Status=Triage on anything without a Status ----
    # This covers both (a) items we just added above, and (b) items added
    # earlier by add-to-project@v1.0.2 which doesn't set fields.
    status_needed: list[str] = items_without_status + [
        iid for iid in new_item_ids if iid not in items_without_status
    ]
    if status_needed:
        print(
            f"\nSetting '{triage_option_name}' on {len(status_needed)} items "
            "without Status..."
        )
        for item_id in status_needed:
            if set_status_triage(project_id, item_id, field_id, triage_option_id):
                status_set += 1
            else:
                status_failed += 1

    print(
        f"\nDone. Added: {added}, add-failed: {failed}, "
        f"status-set: {status_set}, status-failed: {status_failed}"
    )
    return 1 if (failed or status_failed) else 0


if __name__ == "__main__":
    sys.exit(main())
