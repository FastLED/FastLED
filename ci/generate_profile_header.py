"""Render the generated profile header (P3, #4037).

Two modes, per C9.1. `check` is read-only and reports drift; `write`
regenerates. Ingest of new artifacts is a maintainer action landing as an
ordinary reviewable PR -- neither mode fetches anything.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from typeguard import typechecked

from ci.color_profile_generator import (
    Admission,
    Refusal,
    admit,
    load_artifacts,
    render_header,
)


REPO_ROOT = Path(__file__).resolve().parents[1]

# The datasheets commit these artifacts were mirrored from. Bumped by the same
# maintainer PR that re-mirrors them, so the generated header always names the
# revision it can actually be reproduced from.
DATASHEETS_COMMIT = "dfb9a6457d8e2cfff6f6392f115ec52d3b204ab0"

# Frozen rather than `date.today()`. A generated file whose bytes change daily
# cannot be checked for drift, and the useful date is when the artifacts were
# taken, not when someone last ran the script.
GENERATED_ON = "2026-09-09"


@typechecked
def render(artifact_dirs: list[Path]) -> str:
    admissions: list[Admission] = []
    refusals: list[Refusal] = []
    for directory in artifact_dirs:
        for artifact in load_artifacts(directory):
            decision = admit(artifact)
            if isinstance(decision, Admission):
                admissions.append(decision)
            else:
                refusals.append(decision)
    return render_header(admissions, refusals, DATASHEETS_COMMIT, GENERATED_ON)


@typechecked
def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("check", "write"), required=True)
    parsed = parser.parse_args(argv)

    directories = [
        REPO_ROOT / "ci" / "tests" / "fixtures" / "profiles",
        REPO_ROOT / "ci" / "golden" / "profiles",
    ]
    target = REPO_ROOT / "tests" / "fl" / "gfx" / "colorimetric_response_profiles.hpp"
    rendered = render(directories)

    if parsed.mode == "write":
        target.write_text(rendered, encoding="utf-8")
        print(f"wrote {target.relative_to(REPO_ROOT)}")
        return 0

    if not target.is_file():
        print(f"missing generated header: {target.relative_to(REPO_ROOT)}")
        return 1
    if target.read_text(encoding="utf-8") != rendered:
        print(
            f"{target.relative_to(REPO_ROOT)} is stale; "
            "re-run with --mode write and review the diff"
        )
        return 1
    print(f"{target.relative_to(REPO_ROOT)} is up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
