"""Is the mirrored profile set behind upstream? (C9.4, P3, #4037)

Deliberately *not* part of `bash lint`. This reaches the network, and a check
that can fail because a DNS server blinked has no business gating a commit.

Two properties follow from that and are load-bearing rather than incidental:

* **It always exits 0.** Everything it finds is status, not error. A newer
  report upstream is information for a maintainer to act on when they choose;
  ingest is a deliberate, reviewed act (C9.1) and never something a checker
  triggers.
* **Offline is a reportable state, not a failure.** With no network it says it
  could not check and exits 0, rather than claiming everything is current --
  which is the answer that would actually mislead.

The comparison is pure and takes the remote index as an argument, so the
interesting half runs against a fixture with no network at all.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

from typeguard import typechecked

from ci.color_profile_generator import SUPPORTED_SCHEMA_MAJOR, Artifact, load_artifacts
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


REPO_ROOT = Path(__file__).resolve().parents[1]
MIRRORED_DIR = REPO_ROOT / "ci" / "golden" / "profiles"

DATASHEETS_REPO = "FastLED/datasheets"
DATASHEETS_PATH = "measured-profiles"


@typechecked
@dataclass(frozen=True, slots=True)
class RemoteArtifact:
    """One artifact as it exists upstream."""

    profile_id: str
    schema_version: str
    file_name: str


@typechecked
@dataclass(frozen=True, slots=True)
class Finding:
    """One thing a maintainer might want to know. Never an error."""

    kind: str
    profile_id: str
    detail: str


@typechecked
@dataclass(frozen=True, slots=True)
class FreshnessReport:
    checked: bool
    reason: str
    findings: tuple[Finding, ...]


@typechecked
def identity_of(profile_id: str) -> str:
    """A profile's identity without its report ID.

    Two reports on one part are different profiles but the same *thing*, so
    this is what "is there a newer report for something we already carry"
    compares on.
    """

    segments = profile_id.split("/")
    return "/".join(segments[:-1]) if len(segments) > 1 else profile_id


@typechecked
def report_of(profile_id: str) -> str:
    segments = profile_id.split("/")
    return segments[-1] if len(segments) > 1 else ""


@typechecked
def compare(local: list[Artifact], remote: list[RemoteArtifact]) -> list[Finding]:
    """Everything worth reporting, given both sides. No I/O."""

    findings: list[Finding] = []

    local_ids = set()
    local_by_identity: dict[str, set[str]] = {}
    for artifact in local:
        local_ids.add(artifact.profile_id)
        local_by_identity.setdefault(identity_of(artifact.profile_id), set()).add(
            report_of(artifact.profile_id)
        )

    remote_ids = set()
    for entry in remote:
        remote_ids.add(entry.profile_id)
        identity = identity_of(entry.profile_id)
        if entry.profile_id in local_ids:
            major = entry.schema_version.split(".")[0]
            if not major.isdigit() or int(major) != SUPPORTED_SCHEMA_MAJOR:
                findings.append(
                    Finding(
                        "schema_drift",
                        entry.profile_id,
                        f"upstream schema_version {entry.schema_version} is outside "
                        f"the {SUPPORTED_SCHEMA_MAJOR}.x this generator reads",
                    )
                )
            continue
        if identity in local_by_identity:
            findings.append(
                Finding(
                    "newer_report",
                    entry.profile_id,
                    f"upstream has report {report_of(entry.profile_id)!r} for a part "
                    f"mirrored at {sorted(local_by_identity[identity])}",
                )
            )
        else:
            findings.append(
                Finding(
                    "new_part",
                    entry.profile_id,
                    "upstream carries a part with no mirrored artifact",
                )
            )

    for artifact in local:
        if artifact.profile_id not in remote_ids:
            findings.append(
                Finding(
                    "withdrawn",
                    artifact.profile_id,
                    "mirrored here but no longer present upstream; artifacts are "
                    "append-only, so this is worth a look rather than a re-mirror",
                )
            )

    findings.sort(key=lambda f: (f.kind, f.profile_id))
    return findings


@typechecked
def fetch_remote_index() -> list[RemoteArtifact]:
    """The upstream artifact list, over the network.

    Separated from `compare` so the comparison is testable with no network,
    and so a transport failure has exactly one place to come from.
    """

    import subprocess

    # Raw `subprocess` rather than `RunningProcess`: this parses the child's
    # stdout as JSON and, below, as base64. `RunningProcess` merges stderr
    # into that stream and strips trailing newlines, either of which corrupts
    # the parse. Explicit encoding because text mode otherwise falls back to
    # the locale codec.
    listing = subprocess.run(  # noqa: SRC001
        ["gh", "api", f"repos/{DATASHEETS_REPO}/contents/{DATASHEETS_PATH}"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=True,
        timeout=60,
    )
    entries: list[RemoteArtifact] = []
    for record in json.loads(listing.stdout):
        name = record.get("name", "")
        if not name.endswith(".profile.json"):
            continue
        blob = subprocess.run(  # noqa: SRC001
            [
                "gh",
                "api",
                f"repos/{DATASHEETS_REPO}/contents/{DATASHEETS_PATH}/{name}",
                "-q",
                ".content",
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=True,
            timeout=60,
        )
        import base64

        payload = json.loads(base64.b64decode(blob.stdout))
        entries.append(
            RemoteArtifact(
                profile_id=str(payload.get("profile_id", "")),
                schema_version=str(payload.get("schema_version", "")),
                file_name=name,
            )
        )
    return entries


@typechecked
def run(fetch: Callable[[], list[RemoteArtifact]], local_dir: Path) -> FreshnessReport:
    """Fetch, compare, and never raise for a transport problem."""

    local = load_artifacts(local_dir)
    try:
        remote = fetch()
    except KeyboardInterrupt as interrupt:
        # Explicit, and not merely to satisfy KBI001: an interrupt is the one
        # thing here that must *not* be reported as "could not check". The
        # user asked to stop, which is not a statement about upstream.
        handle_keyboard_interrupt(interrupt)
        raise
    except Exception as error:  # noqa: BLE001 - any transport failure is a status
        return FreshnessReport(
            checked=False,
            reason=f"could not reach {DATASHEETS_REPO}: {error}",
            findings=(),
        )
    return FreshnessReport(
        True, "compared against upstream", tuple(compare(local, remote))
    )


@typechecked
def render_text(report: FreshnessReport) -> str:
    if not report.checked:
        return f"could not check: {report.reason}"
    if not report.findings:
        return "up to date: every mirrored artifact matches upstream"
    lines = [f"{len(report.findings)} finding(s) -- status, not errors:"]
    for finding in report.findings:
        lines.append(f"  [{finding.kind}] {finding.profile_id}")
        lines.append(f"      {finding.detail}")
    lines.append("")
    lines.append(
        "Ingest is a maintainer action and lands as a reviewed PR "
        "(uv run python -m ci.generate_profile_header --mode write)."
    )
    return "\n".join(lines)


@typechecked
def render_json(report: FreshnessReport) -> str:
    findings: list[dict[str, str]] = []
    for finding in report.findings:
        findings.append(
            {
                "kind": finding.kind,
                "profile_id": finding.profile_id,
                "detail": finding.detail,
            }
        )
    return json.dumps(
        {"checked": report.checked, "reason": report.reason, "findings": findings},
        indent=2,
    )


@typechecked
def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    parsed = parser.parse_args(argv)

    report = run(fetch_remote_index, MIRRORED_DIR)
    print(render_json(report) if parsed.json else render_text(report))
    # Always zero. Findings are status, and this must never gate anything.
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
