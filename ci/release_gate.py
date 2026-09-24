"""Dispatch and verify the exact-SHA full CI evidence before a release.

Run from the repository root through ``uv run python ci/release_gate.py``.
The selector catalog is the source of truth for board and long-test workflows.
"""

import argparse
import itertools
import json
import os
import re
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

import yaml

from ci.ci_labels import board_jobs, test_jobs


NATIVE_WORKFLOWS = (
    "unit_test_linux.yml",
    "unit_test_windows.yml",
    "unit_test_macos.yml",
    "example_test_linux.yml",
    "example_test_windows.yml",
    "example_test_macos.yml",
)
SHA = re.compile(r"[0-9a-f]{40}\Z")
WORKFLOWS = Path(__file__).resolve().parent.parent / ".github" / "workflows"


def _job_names(filename: str, job_ids: set[str]) -> set[str]:
    """Resolve GitHub's job display names from top-level IDs and matrices."""
    data = yaml.load((WORKFLOWS / filename).read_text(), Loader=yaml.BaseLoader)
    names = set()
    for job_id in job_ids:
        job = data["jobs"][job_id]
        prefix = job.get("name", job_id)
        matrix = job.get("strategy", {}).get("matrix", {})
        if "include" in matrix:
            rows = matrix["include"]
        elif matrix:
            axes = [values for key, values in matrix.items() if key != "exclude"]
            rows = [dict(enumerate(values)) for values in itertools.product(*axes)]
        else:
            rows = [{}]
        child = ""
        if "uses" in job:
            called = WORKFLOWS / Path(job["uses"]).name
            called_data = yaml.load(called.read_text(), Loader=yaml.BaseLoader)
            called_jobs = called_data["jobs"]
            if len(called_jobs) != 1:
                raise ValueError(f"{filename}/{job_id} calls a multi-job workflow")
            called_id, called_job = next(iter(called_jobs.items()))
            child = " / " + called_job.get("name", called_id)
        for row in rows:
            suffix = (
                f" ({', '.join(str(value) for value in row.values())})" if row else ""
            )
            names.add(f"{prefix}{suffix}{child}")
    return names


def required_workflows() -> dict[str, set[str]]:
    """Exact selected job cells, including both hosted macOS variants."""
    selected = {**board_jobs(), **test_jobs()}
    required = {
        filename: _job_names(filename, set(jobs)) for filename, jobs in selected.items()
    }
    for filename in NATIVE_WORKFLOWS:
        required[filename] = _job_names(filename, {"test"})
    required["ci-labels.yml"] = _job_names("ci-labels.yml", {"validate"})
    return dict(sorted(required.items()))


def api(method: str, path: str, payload: dict | None = None) -> dict:
    token = os.environ["GH_TOKEN"]
    repo = os.environ["GITHUB_REPOSITORY"]
    url = f"https://api.github.com/repos/{repo}/{path}"
    body = json.dumps(payload).encode() if payload is not None else None
    request = urllib.request.Request(
        url,
        data=body,
        method=method,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": "application/json",
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        data = response.read()
    return json.loads(data) if data else {}


def master_sha() -> str:
    return api("GET", "git/ref/heads/master")["object"]["sha"]


def verify_run(sha: str, workflow: str, required_jobs: set[str]) -> int | None:
    query = urllib.parse.urlencode(
        {"event": "workflow_dispatch", "head_sha": sha, "per_page": 100}
    )
    runs = api("GET", f"actions/workflows/{workflow}/runs?{query}")["workflow_runs"]
    for run in runs:
        if (
            run["head_sha"] != sha
            or run["event"] != "workflow_dispatch"
            or run["conclusion"] != "success"
        ):
            continue
        run_id = run["id"]
        jobs = api("GET", f"actions/runs/{run_id}/jobs?per_page=100")["jobs"]
        successful = {job["name"] for job in jobs if job["conclusion"] == "success"}
        if not required_jobs <= successful:
            continue
        return run_id
    return None


def verify(sha: str) -> dict[str, int]:
    if not SHA.fullmatch(sha):
        raise ValueError("candidate must be a full lowercase commit SHA")
    evidence = {}
    missing = []
    for workflow, count in required_workflows().items():
        run_id = verify_run(sha, workflow, count)
        if run_id is None:
            missing.append(workflow)
        else:
            evidence[workflow] = run_id
    if missing:
        raise ValueError("missing exact-SHA successful full CI: " + ", ".join(missing))
    return evidence


def dispatch_full(sha: str) -> None:
    if not SHA.fullmatch(sha) or master_sha() != sha:
        raise ValueError(
            "dispatch requires the candidate SHA to be current master HEAD"
        )
    for workflow in required_workflows():
        if master_sha() != sha:
            raise ValueError("master advanced during dispatch; start a new candidate")
        api("POST", f"actions/workflows/{workflow}/dispatches", {"ref": "master"})
        print(f"dispatched {workflow}")


def require_master(sha: str, observed: str) -> None:
    if not SHA.fullmatch(sha) or observed != sha:
        raise ValueError("master advanced after full CI; refuse stale candidate")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command", choices=("dispatch-full", "verify", "require-master")
    )
    parser.add_argument("--sha", required=True)
    parser.add_argument("--observed")
    args = parser.parse_args()
    try:
        if args.command == "dispatch-full":
            dispatch_full(args.sha)
        elif args.command == "require-master":
            require_master(args.sha, args.observed or "")
        else:
            evidence = verify(args.sha)
            print(json.dumps({"sha": args.sha, "runs": evidence}, sort_keys=True))
    except (KeyError, ValueError, urllib.error.URLError) as exc:
        parser.exit(1, f"release gate: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
