"""Unit tests for the release helper's pure logic (no network, no git)."""

import json
from pathlib import Path

import pytest

from ci.release import (
    NotesHeading,
    Version,
    VersionSite,
    check_tree,
    notes_heading,
    tree_version_sites,
)


def test_version_round_trips_and_encodes() -> None:
    v = Version.parse("3.10.6")
    assert str(v) == "3.10.6"
    assert v.as_int() == 3010006


@pytest.mark.parametrize("bad", ["3.10", "v3.10.6", "3.10.6-rc1", ""])
def test_version_rejects_non_release_names(bad: str) -> None:
    with pytest.raises(ValueError):
        Version.parse(bad)


def test_versions_sort_numerically() -> None:
    assert Version.parse("3.9.20") < Version.parse("3.10.0")


def test_next_steps_are_exactly_one_increment() -> None:
    steps = {str(v) for v in Version.parse("3.10.5").next_steps()}
    assert steps == {"3.10.6", "3.11.0", "4.0.0"}


def _write_tree(
    root: Path, props: str, manifest: str, define: int, heading: str
) -> None:
    (root / "src").mkdir()
    (root / "library.properties").write_text(f"name=FastLED\nversion={props}\n")
    (root / "library.json").write_text(json.dumps({"version": manifest}))
    (root / "src" / "FastLED.h").write_text(f"#define FASTLED_VERSION {define}\n")
    (root / "release_notes.md").write_text(f"\n{heading}\n====\n")


def test_tree_sites_and_notes_heading_are_parsed(tmp_path: Path) -> None:
    _write_tree(tmp_path, "3.10.5", "3.10.5", 3010005, "FastLED 3.10.6 (Next Release)")
    assert {s.value for s in tree_version_sites(tmp_path)} == {"3.10.5"}
    assert notes_heading(tmp_path) == NotesHeading("3.10.6", True)


def test_tree_sites_expose_drift(tmp_path: Path) -> None:
    _write_tree(tmp_path, "3.10.4", "3.10.3", 3010004, "FastLED 3.10.4")
    values = {s.path: s.value for s in tree_version_sites(tmp_path)}
    assert values["library.json"] == "3.10.3"
    assert values["src/FastLED.h"] == "3.10.4"


def _sites(version: str) -> list[VersionSite]:
    return [VersionSite(p, version) for p in ("library.properties", "library.json")]


TAG = Version.parse("3.10.5")


def test_steady_state_passes_when_tree_matches_the_tag() -> None:
    assert check_tree(_sites("3.10.5"), NotesHeading("3.10.6", True), TAG, False) == []
    assert check_tree(_sites("3.10.5"), NotesHeading("3.10.5", False), TAG, False) == []


def test_steady_state_rejects_a_version_the_crawler_would_publish() -> None:
    # What #4443 put on master: 3.10.6 with no 3.10.6 tag.
    problems = check_tree(_sites("3.10.6"), NotesHeading("3.10.6", True), TAG, False)
    assert len(problems) == 1 and "crawler" in problems[0]


def test_disagreeing_sites_fail_first() -> None:
    sites = [
        VersionSite("library.properties", "3.10.4"),
        VersionSite("library.json", "3.10.3"),
    ]
    assert check_tree(sites, NotesHeading("3.10.4", False), TAG, False) == [
        "version strings disagree"
    ]


def test_release_pr_must_step_once_with_final_notes() -> None:
    assert check_tree(_sites("3.10.6"), NotesHeading("3.10.6", False), TAG, True) == []
    # Skipped a version.
    assert check_tree(_sites("3.10.7"), NotesHeading("3.10.7", False), TAG, True)
    # Notes still marked as upcoming.
    assert check_tree(_sites("3.10.6"), NotesHeading("3.10.6", True), TAG, True)
    # Not bumped at all.
    assert check_tree(_sites("3.10.5"), NotesHeading("3.10.5", False), TAG, True)


def test_no_reachable_tags_skips_the_tag_comparison() -> None:
    assert (
        check_tree(_sites("3.10.6"), NotesHeading("3.10.6", False), None, False) == []
    )
