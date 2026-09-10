"""The freshness checker reports status and never fails (C9.4, #4037).

Two properties carry the design, and both are easy to lose to a refactor:

* it always exits 0 -- findings are information for a maintainer, and ingest
  is a deliberate reviewed act, so nothing here may gate a commit; and
* offline is a *reportable state*, not a silent pass. Saying "up to date"
  when nothing was actually checked is the answer that would mislead.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from ci.color_profile_freshness import (
    Finding,
    FreshnessReport,
    RemoteArtifact,
    compare,
    main,
    render_json,
    render_text,
    run,
)
from ci.color_profile_generator import load_artifacts


REPO_ROOT = Path(__file__).resolve().parents[2]
MIRRORED = REPO_ROOT / "ci" / "golden" / "profiles"


def _mirrored() -> list[object]:
    return load_artifacts(MIRRORED)


def _as_remote(schema_version: str) -> list[RemoteArtifact]:
    entries: list[RemoteArtifact] = []
    for artifact in _mirrored():
        entries.append(
            RemoteArtifact(artifact.profile_id, schema_version, artifact.source_name)
        )
    return entries


class TestTheFixtureIsUseful(unittest.TestCase):
    def test_there_is_something_mirrored_to_compare(
        self: "TestTheFixtureIsUseful",
    ) -> None:
        # Every case below is quantified over this set; an empty one would
        # make "no findings" mean nothing.
        self.assertGreaterEqual(len(_mirrored()), 2)


class TestComparison(unittest.TestCase):
    def test_an_identical_index_reports_nothing(self: "TestComparison") -> None:
        self.assertEqual(compare(_mirrored(), _as_remote("1.0")), [])

    def test_a_newer_report_for_a_mirrored_part_is_reported(
        self: "TestComparison",
    ) -> None:
        remote = _as_remote("1.0")
        first = remote[0]
        identity = first.profile_id.rsplit("/", 1)[0]
        remote.append(RemoteArtifact(f"{identity}/datasheet-r2", "1.0", "r2.json"))
        findings = compare(_mirrored(), remote)
        self.assertEqual([f.kind for f in findings], ["newer_report"])
        self.assertIn("datasheet-r2", findings[0].profile_id)

    def test_a_new_part_is_distinguished_from_a_newer_report(
        self: "TestComparison",
    ) -> None:
        # Different kinds because they call for different actions: a new
        # report on a part we carry may want re-mirroring, a wholly new part
        # is a catalogue decision.
        remote = _as_remote("1.0")
        remote.append(RemoteArtifact("sk6812/5050/warm/datasheet-r1", "1.0", "s.json"))
        findings = compare(_mirrored(), remote)
        self.assertEqual([f.kind for f in findings], ["new_part"])

    def test_an_artifact_that_vanished_upstream_is_reported(
        self: "TestComparison",
    ) -> None:
        remote = _as_remote("1.0")
        dropped = remote.pop()
        findings = compare(_mirrored(), remote)
        self.assertEqual([f.kind for f in findings], ["withdrawn"])
        self.assertEqual(findings[0].profile_id, dropped.profile_id)

    def test_schema_drift_is_reported_for_an_artifact_we_already_carry(
        self: "TestComparison",
    ) -> None:
        findings = compare(_mirrored(), _as_remote("2.0"))
        self.assertTrue(findings)
        self.assertTrue(all(f.kind == "schema_drift" for f in findings))

    def test_findings_are_ordered_deterministically(self: "TestComparison") -> None:
        # The scheduled workflow writes these into a standing issue; an
        # unstable order would churn that issue on every run.
        remote = _as_remote("1.0")
        remote.append(RemoteArtifact("zz/5050/none/r1", "1.0", "z.json"))
        remote.append(RemoteArtifact("aa/5050/none/r1", "1.0", "a.json"))
        first = compare(_mirrored(), remote)
        remote.reverse()
        self.assertEqual(first, compare(_mirrored(), remote))


class TestOfflineIsAState(unittest.TestCase):
    def test_a_transport_failure_is_reported_not_raised(
        self: "TestOfflineIsAState",
    ) -> None:
        def _explode() -> list[RemoteArtifact]:
            raise OSError("network is unreachable")

        report = run(_explode, MIRRORED)
        self.assertFalse(report.checked)
        self.assertIn("network is unreachable", report.reason)
        self.assertEqual(report.findings, ())

    def test_offline_does_not_render_as_up_to_date(
        self: "TestOfflineIsAState",
    ) -> None:
        # The failure mode worth guarding: an unchecked run that reads as a
        # clean bill of health.
        offline = FreshnessReport(False, "could not reach anything", ())
        rendered = render_text(offline)
        self.assertIn("could not check", rendered)
        self.assertNotIn("up to date", rendered)

        clean = FreshnessReport(True, "compared", ())
        self.assertIn("up to date", render_text(clean))


class TestAlwaysExitsZero(unittest.TestCase):
    def _run_main(self: "TestAlwaysExitsZero", argv: list[str]) -> int:
        return main(argv)

    def test_exit_is_zero_even_with_findings(self: "TestAlwaysExitsZero") -> None:
        import ci.color_profile_freshness as module

        original = module.fetch_remote_index
        try:
            module.fetch_remote_index = lambda: [  # type: ignore[assignment]
                RemoteArtifact("brand/new/part/r1", "1.0", "n.json")
            ]
            self.assertEqual(self._run_main([]), 0)
            self.assertEqual(self._run_main(["--json"]), 0)
        finally:
            module.fetch_remote_index = original

    def test_exit_is_zero_when_the_network_fails(self: "TestAlwaysExitsZero") -> None:
        import ci.color_profile_freshness as module

        def _explode() -> list[RemoteArtifact]:
            raise OSError("no route to host")

        original = module.fetch_remote_index
        try:
            module.fetch_remote_index = _explode  # type: ignore[assignment]
            self.assertEqual(self._run_main([]), 0)
        finally:
            module.fetch_remote_index = original


class TestRendering(unittest.TestCase):
    def test_json_carries_every_finding(self: "TestRendering") -> None:
        report = FreshnessReport(
            True, "compared", (Finding("new_part", "a/b/c/r1", "detail here"),)
        )
        payload = json.loads(render_json(report))
        self.assertTrue(payload["checked"])
        self.assertEqual(payload["findings"][0]["kind"], "new_part")
        self.assertEqual(payload["findings"][0]["profile_id"], "a/b/c/r1")

    def test_the_text_report_says_findings_are_not_errors(
        self: "TestRendering",
    ) -> None:
        # Agents are told elsewhere that these are status; the report itself
        # has to say so too, since that is what someone actually reads.
        report = FreshnessReport(
            True, "compared", (Finding("new_part", "a/b/c/r1", "detail"),)
        )
        self.assertIn("status, not errors", render_text(report))


if __name__ == "__main__":
    unittest.main()


class TestTheDocsStateTheExemption(unittest.TestCase):
    """C9.6 is a policy claim, so it is pinned rather than trusted.

    An agent doc that quietly loses "status, not errors" turns every weekly
    finding into something the fix-all-errors rule says to act on
    immediately -- which is precisely the automatic ingest C9.1 forbids.
    """

    def test_the_agents_doc_says_findings_are_not_errors(
        self: "TestTheDocsStateTheExemption",
    ) -> None:
        doc = (REPO_ROOT / "agents" / "docs" / "color-profile-artifacts.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("status, not errors", doc)
        self.assertIn("exempt", doc.lower())
        self.assertIn("always exits 0", doc)

    def test_the_task_table_points_at_it(
        self: "TestTheDocsStateTheExemption",
    ) -> None:
        claude = (REPO_ROOT / "CLAUDE.md").read_text(encoding="utf-8")
        self.assertIn("agents/docs/color-profile-artifacts.md", claude)
        self.assertIn("status, not errors", claude)

    def test_the_workflow_is_not_a_commit_gate(
        self: "TestTheDocsStateTheExemption",
    ) -> None:
        import yaml

        path = REPO_ROOT / ".github" / "workflows" / "color_profile_freshness.yml"
        workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
        # `on` parses as the boolean True in YAML 1.1, which is why this
        # looks up both spellings rather than assuming one.
        triggers = workflow.get("on", workflow.get(True))
        self.assertIn("schedule", triggers)
        self.assertNotIn("push", triggers)
        self.assertNotIn("pull_request", triggers)
