from ci.util.global_interrupt_handler import handle_keyboard_interrupt


#!/usr/bin/env python3
"""
CI Build Scanner — lists all builds and tests with status, URLs, and error logs for failures.

Auto-discovers workflows from .github/workflows/ matching build_*.yml,
unit_test_*.yml, and example_test_*.yml so that adding a new workflow
automatically adds it to the report.

Usage:
    uv run ci/tools/gh/ci_builds.py                  # list all builds
    uv run ci/tools/gh/ci_builds.py --failing         # only failing builds
    uv run ci/tools/gh/ci_builds.py --errors          # failing + fetch error logs
    uv run ci/tools/gh/ci_builds.py --board uno       # filter to specific board(s)
    uv run ci/tools/gh/ci_builds.py --board uno,esp32s3
"""

import argparse
import json
import re
import sys
import threading
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Optional

from running_process import PIPE, RunningProcess


# ---------------------------------------------------------------------------
# Priority list: builds are sorted so these appear first (in order)
# ---------------------------------------------------------------------------
PRIORITY_BOARDS = [
    "unit_test",
    "example_test",
    "uno",
    "attiny85",
    "esp32s3",
    "esp32c6",
    "teensy41",
]

# Workflow files that are templates / meta-builds, not individual board builds
_EXCLUDE_PATTERNS = [
    "build_template",
    "build_clone_and_compile",
    "build_esp_extra_libs",
    "build_wasm_compilers",
    "template_unit_test",
    "template_example_test",
]

# Glob patterns for workflow discovery (in addition to build_*.yml)
_WORKFLOW_GLOBS = [
    "build_*.yml",
    "unit_test_*.yml",
    "example_test_*.yml",
]


@dataclass
class BuildInfo:
    """Information about a single CI build."""

    name: str  # workflow name (from YAML `name:` field)
    workflow_file: str  # e.g. build_uno.yml
    args: str  # board arg passed to build_template
    run_id: Optional[str] = None
    conclusion: Optional[str] = None  # success / failure / cancelled / None
    status: Optional[str] = None  # completed / in_progress / queued
    branch: Optional[str] = None
    url: Optional[str] = None
    errors: list[str] = field(default_factory=lambda: list[str]())

    @property
    def is_failing(self) -> bool:
        return self.conclusion in ("failure", "cancelled", "timed_out")

    @property
    def priority_index(self) -> int:
        """Lower = higher priority.  Boards not in the list get a large number."""
        name_lower = self.name.lower()
        file_lower = self.workflow_file.lower()
        for i, board in enumerate(PRIORITY_BOARDS):
            if board in name_lower or board in file_lower:
                return i
        return len(PRIORITY_BOARDS) + 1000

    def to_dict(self) -> dict[str, Any]:
        """Serialize to dict, including computed properties."""
        d = asdict(self)
        d["is_failing"] = self.is_failing
        d["priority_index"] = self.priority_index
        return d


def _get_repo() -> str:
    try:
        result = RunningProcess.run(
            ["gh", "repo", "view", "--json", "nameWithOwner"],
            stdout=PIPE,
            stderr=PIPE,
            text=True,
            check=True,
            timeout=10,
            encoding="utf-8",
            errors="replace",
        )
        data = json.loads(result.stdout)
        return data["nameWithOwner"]
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return "FastLED/FastLED"


def discover_builds(repo_root: Path) -> list[BuildInfo]:
    """Discover all build and test workflow files and extract metadata."""
    workflows_dir = repo_root / ".github" / "workflows"
    builds: list[BuildInfo] = []
    seen_files: set[str] = set()

    for glob_pattern in _WORKFLOW_GLOBS:
        for wf_path in sorted(workflows_dir.glob(glob_pattern)):
            filename = wf_path.name

            # Deduplicate across glob patterns
            if filename in seen_files:
                continue
            seen_files.add(filename)

            # Skip templates and meta-builds
            if any(pat in filename for pat in _EXCLUDE_PATTERNS):
                continue

            # Read the workflow to get the name and args
            text = wf_path.read_text(encoding="utf-8")

            # Extract `name:` (first occurrence, top-level)
            name_match = re.search(r"^name:\s*(.+)$", text, re.MULTILINE)
            name = name_match.group(1).strip() if name_match else filename

            # Extract args passed to build_template
            args_match = re.search(r'args:\s*["\']?([^"\'"\n]+)', text)
            args = args_match.group(1).strip() if args_match else ""

            builds.append(
                BuildInfo(
                    name=name,
                    workflow_file=filename,
                    args=args,
                )
            )

    return builds


def _fetch_latest_run(workflow_file: str) -> Optional[dict[str, Any]]:
    """Fetch the latest run for a workflow file."""
    try:
        result = RunningProcess.run(
            [
                "gh",
                "run",
                "list",
                "--workflow",
                workflow_file,
                "--json",
                "databaseId,status,conclusion,headBranch,url",
                "--limit",
                "1",
            ],
            stdout=PIPE,
            stderr=PIPE,
            text=True,
            check=True,
            timeout=30,
            encoding="utf-8",
            errors="replace",
        )
        runs = json.loads(result.stdout)
        return runs[0] if runs else None
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return None


def fetch_statuses(builds: list[BuildInfo], max_concurrent: int = 10) -> None:
    """Populate status fields for all builds (concurrent)."""

    def _worker(build: BuildInfo) -> None:
        run = _fetch_latest_run(build.workflow_file)
        if run:
            build.run_id = str(run.get("databaseId", ""))
            build.conclusion = run.get("conclusion")
            build.status = run.get("status")
            build.branch = run.get("headBranch")
            build.url = run.get("url", "")

    threads: list[threading.Thread] = []
    for build in builds:
        t = threading.Thread(target=_worker, args=(build,))
        t.start()
        threads.append(t)
        # Throttle
        while len([t for t in threads if t.is_alive()]) >= max_concurrent:
            threads[0].join(timeout=0.1)
            threads = [t for t in threads if t.is_alive()]

    for t in threads:
        t.join()


def _fetch_error_logs(build: BuildInfo, repo: str) -> None:
    """Fetch error context from the failed run's logs."""
    if not build.run_id:
        return

    try:
        # Get failed jobs
        result = RunningProcess.run(
            ["gh", "run", "view", build.run_id, "--json", "jobs"],
            stdout=PIPE,
            stderr=PIPE,
            text=True,
            check=True,
            timeout=30,
            encoding="utf-8",
            errors="replace",
        )
        data = json.loads(result.stdout)
        jobs = data.get("jobs", [])

        for job in jobs:
            if job.get("conclusion") not in ("failure", "cancelled", "timed_out"):
                continue

            job_id = str(job.get("databaseId", ""))
            if not job_id:
                continue

            # Fetch logs via API
            api_path = f"/repos/{repo}/actions/jobs/{job_id}/logs"
            log_result = RunningProcess.run(
                ["gh", "api", api_path],
                stdout=PIPE,
                stderr=PIPE,
                text=True,
                check=False,
                timeout=60,
                encoding="utf-8",
                errors="replace",
            )
            if log_result.returncode != 0:
                continue

            lines = log_result.stdout.splitlines()
            # Extract lines containing "error" or "fatal" with context
            for i, line in enumerate(lines):
                lower = line.lower()
                if "error:" in lower or "fatal error" in lower:
                    start = max(0, i - 3)
                    end = min(len(lines), i + 4)
                    block = "\n".join(lines[start:end])
                    build.errors.append(block)
                    if len(build.errors) >= 5:
                        return

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        build.errors.append(f"(error fetching logs: {e})")


def fetch_errors(builds: list[BuildInfo], max_concurrent: int = 5) -> None:
    """Fetch error logs for all failing builds (concurrent)."""
    repo = _get_repo()
    failing = [b for b in builds if b.is_failing]

    threads: list[threading.Thread] = []
    for build in failing:
        t = threading.Thread(target=_fetch_error_logs, args=(build, repo))
        t.start()
        threads.append(t)
        while len([t for t in threads if t.is_alive()]) >= max_concurrent:
            threads[0].join(timeout=0.1)
            threads = [t for t in threads if t.is_alive()]

    for t in threads:
        t.join()


def sort_builds(builds: list[BuildInfo]) -> list[BuildInfo]:
    """Sort: failing first (by priority), then passing (by priority)."""
    failing = sorted(
        [b for b in builds if b.is_failing],
        key=lambda b: (b.priority_index, b.name),
    )
    passing = sorted(
        [b for b in builds if not b.is_failing],
        key=lambda b: (b.priority_index, b.name),
    )
    return failing + passing


def print_report(builds: list[BuildInfo], show_errors: bool = False) -> None:
    """Print a human-readable report."""
    failing = [b for b in builds if b.is_failing]
    passing = [b for b in builds if not b.is_failing and b.conclusion == "success"]
    other = [b for b in builds if not b.is_failing and b.conclusion != "success"]

    total = len(builds)
    print(
        f"\nCI Build Report  ({len(failing)} failing / {len(passing)} passing / {total} total)"
    )
    print("=" * 90)

    if failing:
        print(f"\nFAILING ({len(failing)}):")
        print("-" * 90)
        for b in failing:
            prio = ""
            if b.priority_index < len(PRIORITY_BOARDS):
                prio = " [PRIORITY]"
            url_str = b.url or ""
            print(f"  FAIL  {b.name:<35} {b.workflow_file:<40} {url_str}")
            if prio:
                print(f"        {prio}")

            if show_errors and b.errors:
                print(f"        --- Error context ({len(b.errors)} block(s)) ---")
                for err_block in b.errors:
                    for line in err_block.split("\n"):
                        print(f"        | {line}")
                    print(f"        {'.' * 40}")
                print()

    if passing:
        print(f"\nPASSING ({len(passing)}):")
        print("-" * 90)
        for b in passing:
            url_str = b.url or ""
            print(f"  OK    {b.name:<35} {b.workflow_file:<40} {url_str}")

    if other:
        print(f"\nOTHER ({len(other)}):")
        print("-" * 90)
        for b in other:
            status = b.conclusion or b.status or "unknown"
            url_str = b.url or ""
            print(f"  {status:<6} {b.name:<35} {b.workflow_file:<40} {url_str}")

    print("\n" + "=" * 90)

    # Summary for agent consumption
    if failing:
        print("\nPriority fix order:")
        for i, b in enumerate(failing, 1):
            prio_tag = (
                " ** PRIORITY **" if b.priority_index < len(PRIORITY_BOARDS) else ""
            )
            print(f"  {i}. {b.name} ({b.workflow_file}){prio_tag}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Scan CI builds — list status, URLs, and errors",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    uv run ci/tools/gh/ci_builds.py                  # list all builds
    uv run ci/tools/gh/ci_builds.py --failing         # only failing
    uv run ci/tools/gh/ci_builds.py --errors          # failing + error logs
    uv run ci/tools/gh/ci_builds.py --board uno,esp32s3
        """,
    )
    parser.add_argument(
        "--failing",
        action="store_true",
        help="Show only failing builds",
    )
    parser.add_argument(
        "--errors",
        action="store_true",
        help="Fetch and display error logs for failing builds",
    )
    parser.add_argument(
        "--board",
        type=str,
        default="",
        help="Comma-separated board names to filter (substring match)",
    )
    parser.add_argument(
        "--json-output",
        action="store_true",
        help="Output as JSON for programmatic consumption",
    )

    args = parser.parse_args()

    # Find repo root
    repo_root = Path(__file__).resolve().parents[3]

    print("Discovering builds...", file=sys.stderr)
    builds = discover_builds(repo_root)
    print(f"Found {len(builds)} build(s)", file=sys.stderr)

    # Filter by board name if requested
    if args.board:
        filters = [f.strip().lower() for f in args.board.split(",")]
        builds = [
            b
            for b in builds
            if any(
                f in b.name.lower()
                or f in b.args.lower()
                or f in b.workflow_file.lower()
                for f in filters
            )
        ]
        print(f"Filtered to {len(builds)} build(s)", file=sys.stderr)

    # Fetch statuses
    print("Fetching build statuses...", file=sys.stderr)
    fetch_statuses(builds)

    # Filter to failing only if requested
    if args.failing or args.errors:
        builds = [b for b in builds if b.is_failing]

    # Sort by priority
    builds = sort_builds(builds)

    # Fetch error logs if requested
    if args.errors:
        print("Fetching error logs for failing builds...", file=sys.stderr)
        fetch_errors(builds)

    # Output
    if args.json_output:
        out = [b.to_dict() for b in builds]
        print(json.dumps(out, indent=2))
    else:
        print_report(builds, show_errors=args.errors)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"\nError: {e}", file=sys.stderr)
        sys.exit(1)
