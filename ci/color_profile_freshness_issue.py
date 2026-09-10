"""Keep one standing issue current with the freshness report (C9.5, #4037).

One issue, updated in place. A checker that opens a new issue per run turns a
weekly informational check into a stream of notifications, and the first thing
anyone does with that is mute it -- at which point the check has stopped
working while still appearing to run.

Never fails the workflow. This reports status.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from typeguard import typechecked


ISSUE_TITLE = "Colour-profile artifact freshness (automated, informational)"
ISSUE_LABEL = "status: triage"
MARKER = "<!-- color-profile-freshness -->"


@typechecked
def build_body(report_text: str) -> str:
    return "\n".join(
        [
            MARKER,
            "## Colour-profile artifact freshness",
            "",
            "Automated weekly check of `ci/golden/profiles/` against "
            "[FastLED/datasheets](https://github.com/FastLED/datasheets). "
            "**Findings here are status, not errors** -- they are exempt from "
            "the fix-all-errors policy, and ingest happens only on an explicit "
            "maintainer request, as a reviewed PR (C9.1).",
            "",
            "```",
            report_text.strip(),
            "```",
            "",
            "Re-run: Actions -> *color profile freshness* -> Run workflow.",
            "Ingest: `uv run python -m ci.generate_profile_header --mode write`, "
            "then open a PR.",
            "",
            "See `agents/docs/color-profile-artifacts.md`.",
        ]
    )


@typechecked
def find_existing(repo: str) -> int | None:
    # Raw `subprocess`: `gh` writes progress to stderr, and
    # `RunningProcess` merges it into the stdout this parses.
    result = subprocess.run(  # noqa: SRC001
        [
            "gh",
            "issue",
            "list",
            "--repo",
            repo,
            "--state",
            "open",
            "--search",
            ISSUE_TITLE,
            "--json",
            "number,title",
            "--limit",
            "20",
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=120,
    )
    if result.returncode != 0:
        return None
    import json

    for record in json.loads(result.stdout or "[]"):
        if record.get("title") == ISSUE_TITLE:
            return int(record["number"])
    return None


@typechecked
def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--repo", required=True)
    parsed = parser.parse_args(argv)

    repo = parsed.repo
    body = build_body(parsed.report.read_text(encoding="utf-8"))

    number = find_existing(repo)
    if number is None:
        # Raw `subprocess`: `gh` writes progress to stderr, and
        # `RunningProcess` merges it into the stdout this parses.
        subprocess.run(  # noqa: SRC001
            [
                "gh",
                "issue",
                "create",
                "--repo",
                repo,
                "--title",
                ISSUE_TITLE,
                "--body",
                body,
                "--label",
                ISSUE_LABEL,
            ],
            check=False,
            timeout=120,
        )
    else:
        subprocess.run(  # noqa: SRC001
            ["gh", "issue", "edit", str(number), "--repo", repo, "--body", body],
            check=False,
            timeout=120,
        )
    # Informational by construction: never fail the workflow.
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
