"""Report observed Actions runner time for runs indexed by one commit SHA.

Usage: GITHUB_TOKEN=... uv run python ci/actions_usage.py OWNER/REPO SHA
This is incomplete event accounting: downstream workflow_run and
pull_request_target runs can be indexed under a different SHA.
"""

import argparse
import json
import os
from collections import defaultdict
from datetime import datetime
from urllib.parse import urlencode
from urllib.request import Request, urlopen


def pages(url: str, token: str, max_results: int | None = None) -> list[dict]:
    """Fetch every page; fail if a search may have hit GitHub's result cap."""
    items: list[dict] = []
    page = 1
    while True:
        separator = "&" if "?" in url else "?"
        request = Request(
            f"{url}{separator}{urlencode({'per_page': 100, 'page': page})}",
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {token}",
                "X-GitHub-Api-Version": "2022-11-28",
            },
        )
        with urlopen(request, timeout=30) as response:
            payload = json.load(response)
        if (
            max_results is not None
            and isinstance(payload, dict)
            and payload.get("total_count", 0) >= max_results
        ):
            raise ValueError(
                f"API search reached {max_results} results; narrow the query"
            )
        batch = (
            payload if isinstance(payload, list) else payload.get("workflow_runs", [])
        )
        if isinstance(payload, dict) and "jobs" in payload:
            batch = payload["jobs"]
        items.extend(batch)
        if max_results is not None and len(items) >= max_results:
            raise ValueError(
                f"API search reached {max_results} results; narrow the query"
            )
        if len(batch) < 100:
            break
        page += 1
    return items


def job_seconds(job: dict) -> int:
    """Only a started runner job consumes allocated runner time."""
    if not job.get("started_at") or not job.get("completed_at"):
        return 0
    start = datetime.fromisoformat(job["started_at"].replace("Z", "+00:00"))
    end = datetime.fromisoformat(job["completed_at"].replace("Z", "+00:00"))
    return max(0, round((end - start).total_seconds()))


def summarize(runs: list[dict], jobs_by_run: dict[int, list[dict]]) -> dict:
    by_event: dict[str, int] = defaultdict(int)
    details = []
    active_runs = []
    active_jobs = []
    for run in runs:
        if run.get("status", "completed") != "completed":
            active_runs.append(run["id"])
        jobs = jobs_by_run[run["id"]]
        active_jobs.extend(
            job.get("id")
            for job in jobs
            if job.get("status", "completed") != "completed"
            or (job.get("started_at") and not job.get("completed_at"))
        )
        seconds = sum(job_seconds(job) for job in jobs)
        by_event[run["event"]] += seconds
        details.append(
            {
                "run_id": run["id"],
                "workflow": run["name"],
                "event": run["event"],
                "attempt": run["run_attempt"],
                "conclusion": run.get("conclusion"),
                "runner_seconds": seconds,
            }
        )
    return {
        "coverage_complete": False,
        "coverage_limits": [
            "SHA-filtered runs can omit workflow_run and pull_request_target children indexed under the default-branch SHA; totals are lower bounds, not whole-event usage.",
            "A complete PR or master comparison requires causal event correlation and a matched full-validation baseline.",
        ],
        "active_run_ids": active_runs,
        "active_job_ids": active_jobs,
        "observed_runner_seconds": sum(by_event.values()),
        "observed_runner_seconds_by_event": dict(sorted(by_event.items())),
        "runs": details,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", help="OWNER/REPO")
    parser.add_argument("sha", help="Exact 40-character commit SHA")
    args = parser.parse_args()
    if len(args.sha) != 40 or any(
        c not in "0123456789abcdef" for c in args.sha.lower()
    ):
        parser.error("sha must be a full 40-character hexadecimal commit SHA")
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        parser.error("GITHUB_TOKEN or GH_TOKEN is required")
    base = f"https://api.github.com/repos/{args.repository}"
    runs = pages(
        f"{base}/actions/runs?{urlencode({'head_sha': args.sha})}",
        token,
        max_results=1000,
    )
    runs = [run for run in runs if run["head_sha"] == args.sha]
    jobs = {
        run["id"]: pages(f"{base}/actions/runs/{run['id']}/jobs?filter=all", token)
        for run in runs
    }
    print(
        json.dumps(
            {"repository": args.repository, "sha": args.sha, **summarize(runs, jobs)},
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
