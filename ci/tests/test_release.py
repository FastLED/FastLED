"""Unit tests for the release helper's pure logic (no network, no git)."""

import json
from collections.abc import Callable
from pathlib import Path

import pytest

from ci.release import (
    NotesHeading,
    Version,
    VersionSite,
    apply_version,
    check_tree,
    notes_heading,
    release_notes_section,
    run_release_version_lint,
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
    shown = f"{define // 1_000_000}.{define // 1_000 % 1_000:03d}.{define % 1_000:03d}"
    (root / "src" / "FastLED.h").write_text(
        f"#define FASTLED_VERSION {define}\n"
        f'#      pragma message "FastLED version {shown}"\n'
        f"#      warning FastLED version {shown}  (Not really a warning)\n"
    )
    (root / "release_notes.md").write_text(f"\n{heading}\n====\n")
    (root / "docs").mkdir()
    (root / "docs" / "Doxyfile").write_text(f"PROJECT_NUMBER = {props}\n")


def test_tree_sites_and_notes_heading_are_parsed(tmp_path: Path) -> None:
    _write_tree(tmp_path, "3.10.5", "3.10.5", 3010005, "FastLED 3.10.6 (Next Release)")
    assert {s.value for s in tree_version_sites(tmp_path)} == {"3.10.5"}
    assert notes_heading(tmp_path) == NotesHeading("3.10.6", True)


def test_tree_sites_expose_drift(tmp_path: Path) -> None:
    _write_tree(tmp_path, "3.10.4", "3.10.3", 3010004, "FastLED 3.10.4")
    values = {s.path: s.value for s in tree_version_sites(tmp_path)}
    assert values["library.json"] == "3.10.3"
    assert values["src/FastLED.h"] == "3.10.4"
    assert values["src/FastLED.h (FASTLED_SHOW_VERSION)"] == "3.10.4"
    assert values["docs/Doxyfile"] == "3.10.4"


def test_lint_stage_accepts_steady_state_and_a_release_pr(tmp_path: Path) -> None:
    # No git repo under tmp_path, so there are no tags: only agreement is checked.
    _write_tree(tmp_path, "3.10.5", "3.10.5", 3010005, "FastLED 3.10.6 (Next Release)")
    assert run_release_version_lint(tmp_path)


def test_lint_stage_rejects_drift(tmp_path: Path) -> None:
    _write_tree(tmp_path, "3.10.4", "3.10.3", 3010004, "FastLED 3.10.4")
    assert not run_release_version_lint(tmp_path)


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


class _FakeResult:
    def __init__(self, returncode: int, stderr: str = "", stdout: str = "") -> None:
        self.returncode = returncode
        self.stderr = stderr
        self.stdout = stdout


def _fake_gh(result: _FakeResult) -> Callable[..., _FakeResult]:
    def run(*_args: object, **_kwargs: object) -> _FakeResult:
        return result

    return run


def test_github_release_present_missing_and_query_failure(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    import ci.release as release

    tag = Version.parse("3.10.5")
    monkeypatch.setattr(release.RunningProcess, "run", _fake_gh(_FakeResult(0)))
    assert release.github_has_release(tag, tmp_path) is True

    monkeypatch.setattr(
        release.RunningProcess, "run", _fake_gh(_FakeResult(1, "release not found"))
    )
    assert release.github_has_release(tag, tmp_path) is False

    # Not logged in / no network is not "missing": it must not send anyone off
    # to create a release that exists.
    monkeypatch.setattr(
        release.RunningProcess,
        "run",
        _fake_gh(
            _FakeResult(4, "To get started with GitHub CLI, please run: gh auth login")
        ),
    )
    with pytest.raises(release.GitHubQueryError):
        release.github_has_release(tag, tmp_path)


def test_git_env_drops_repository_overrides(monkeypatch: pytest.MonkeyPatch) -> None:
    import ci.release as release

    monkeypatch.setenv("GIT_DIR", "/elsewhere/.git")
    monkeypatch.setenv("GIT_COMMON_DIR", "/elsewhere/.git")
    monkeypatch.setenv("GIT_AUTHOR_NAME", "kept")
    env = release._git_env()
    assert "GIT_DIR" not in env
    assert "GIT_COMMON_DIR" not in env
    assert env["GIT_AUTHOR_NAME"] == "kept"


def test_notes_heading_needs_its_underline(tmp_path: Path) -> None:
    # Prose mentioning a version before the real heading must not be read
    # as the heading.
    (tmp_path / "release_notes.md").write_text(
        "FastLED 9.9.9 is mentioned here in passing.\n\n"
        "FastLED 3.10.6 (Next Release)\n==============\n"
    )
    heading = notes_heading(tmp_path)
    assert heading.version == "3.10.6"
    assert heading.is_next_release


NOTES = """

FastLED 3.10.6 (Next Release)
==============
  * new thing

FastLED 3.10.5
==============
  * old thing
"""


def _write_full_tree(root: Path, notes: str) -> None:
    _write_tree(root, "3.10.5", "3.10.5", 3010005, "unused")
    (root / "library.json").write_text(
        '{\n    "name": "FastLED",\n    "version": "3.10.5",\n    "x": 1\n}\n'
    )
    (root / "release_notes.md").write_text(notes)


def test_release_notes_section_stops_at_the_next_heading() -> None:
    assert release_notes_section(NOTES, Version.parse("3.10.6")) == "  * new thing"
    assert release_notes_section(NOTES, Version.parse("3.10.5")) == "  * old thing"
    assert release_notes_section(NOTES, Version.parse("3.9.0")) is None


def test_apply_version_makes_the_tree_a_valid_release(tmp_path: Path) -> None:
    _write_full_tree(tmp_path, NOTES)
    apply_version(tmp_path, Version.parse("3.10.6"))
    sites = tree_version_sites(tmp_path)
    assert {s.value for s in sites} == {"3.10.6"}
    assert notes_heading(tmp_path) == NotesHeading("3.10.6", False)
    assert check_tree(sites, notes_heading(tmp_path), TAG, True) == []
    # Only the version changed in the manifest.
    assert json.loads((tmp_path / "library.json").read_text())["x"] == 1


def test_apply_version_renumbers_the_heading_for_a_minor_bump(tmp_path: Path) -> None:
    _write_full_tree(tmp_path, NOTES)
    apply_version(tmp_path, Version.parse("3.11.0"))
    assert notes_heading(tmp_path) == NotesHeading("3.11.0", False)
    assert "3.011.000" in (tmp_path / "src" / "FastLED.h").read_text()


def test_apply_version_requires_the_displayed_version_string(tmp_path: Path) -> None:
    _write_full_tree(tmp_path, NOTES)
    header_path = tmp_path / "src" / "FastLED.h"
    original = header_path.read_text()
    header_path.write_text(original.replace("FastLED version", "FastLED release"))

    with pytest.raises(ValueError, match="displayed version string"):
        apply_version(tmp_path, Version.parse("3.10.6"))

    assert (tmp_path / "library.properties").read_text() == (
        "name=FastLED\nversion=3.10.5\n"
    )


def test_prepare_pr_requires_master(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    import ci.release as release

    monkeypatch.setattr(release, "release_tags", lambda _root, merged_only: [TAG])

    def fake_git(args: list[str], _root: Path) -> str:
        if args == ["status", "--porcelain"]:
            return ""
        if args == ["branch", "--show-current"]:
            return "feature\n"
        raise AssertionError(f"unexpected git call: {args}")

    monkeypatch.setattr(release, "_git", fake_git)
    assert release.cmd_prepare(tmp_path, "patch", True) == 1


def test_prepare_pr_validates_all_rewrites_before_creating_branch(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    import ci.release as release

    _write_full_tree(tmp_path, NOTES)
    header_path = tmp_path / "src" / "FastLED.h"
    header_path.write_text(
        header_path.read_text().replace("FastLED version", "FastLED release")
    )
    monkeypatch.setattr(release, "release_tags", lambda _root, merged_only: [TAG])
    calls: list[list[str]] = []

    def fake_git(args: list[str], _root: Path) -> str:
        calls.append(args)
        if args == ["status", "--porcelain"]:
            return ""
        if args == ["branch", "--show-current"]:
            return "master\n"
        if args == ["fetch", "origin", "master", "--tags"]:
            return ""
        if args in (["rev-parse", "HEAD"], ["rev-parse", "origin/master"]):
            return "abc123\n"
        if args == ["branch", "--list", "release/3.10.6"]:
            return ""
        raise AssertionError(f"unexpected git call: {args}")

    monkeypatch.setattr(release, "_git", fake_git)
    assert release.cmd_prepare(tmp_path, "patch", True) == 1
    assert not any(args[:2] == ["checkout", "-b"] for args in calls)


@pytest.mark.parametrize(
    "notes",
    [
        "\nFastLED 3.10.5\n====\n  * old thing\n",  # no upcoming section
        "\nFastLED 3.10.6 (Next Release)\n====\n\nFastLED 3.10.5\n====\n  * old\n",  # empty
    ],
)
def test_apply_version_refuses_without_notes_and_changes_nothing(
    tmp_path: Path, notes: str
) -> None:
    _write_full_tree(tmp_path, notes)
    with pytest.raises(ValueError):
        apply_version(tmp_path, Version.parse("3.10.6"))
    assert {s.value for s in tree_version_sites(tmp_path)} == {"3.10.5"}
