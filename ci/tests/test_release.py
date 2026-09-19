"""Unit tests for the release helper's pure logic (no network, no git)."""

import json
from pathlib import Path

import pytest

from ci.release import (
    AUTH_TOKEN_ENV,
    Version,
    cmd_publish,
    publish_command,
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
    # The drift that shipped: master at 3.10.4 while 3.10.5 was tagged.
    assert Version.parse("3.10.4") not in Version.parse("3.10.5").next_steps()
    assert Version.parse("3.10.7") not in Version.parse("3.10.5").next_steps()


def _write_tree(root: Path, props: str, manifest: str, define: int, notes: str) -> None:
    (root / "src").mkdir()
    (root / "library.properties").write_text(f"name=FastLED\nversion={props}\n")
    (root / "library.json").write_text(json.dumps({"version": manifest}))
    (root / "src" / "FastLED.h").write_text(f"#define FASTLED_VERSION {define}\n")
    (root / "release_notes.md").write_text(f"\nFastLED {notes} (Next Release)\n====\n")


def test_tree_version_sites_normalizes_every_location(tmp_path: Path) -> None:
    _write_tree(tmp_path, "3.10.6", "3.10.6", 3010006, "3.10.6")
    assert {s.value for s in tree_version_sites(tmp_path)} == {"3.10.6"}


def test_tree_version_sites_exposes_drift(tmp_path: Path) -> None:
    _write_tree(tmp_path, "3.10.4", "3.10.3", 3010004, "3.10.4")
    values = {s.path: s.value for s in tree_version_sites(tmp_path)}
    assert values["library.json"] == "3.10.3"
    assert values["src/FastLED.h"] == "3.10.4"


def test_publish_command_is_the_publish_subcommand_only() -> None:
    cmd = publish_command(Path("pkg.tar.gz"))
    assert cmd[cmd.index("pkg") : cmd.index("pkg") + 2] == ["pkg", "publish"]
    assert "--no-interactive" in cmd
    assert cmd[cmd.index("--owner") + 1] == "fastled"
    for banned in ("run", "test", "device", "upload"):
        assert banned not in cmd


def test_publish_rejects_a_malformed_tag() -> None:
    with pytest.raises(ValueError):
        cmd_publish(Path("."), "latest", yes=False)


def test_auth_token_env_name() -> None:
    # The tool reads this exact variable; a typo would fall back to a prompt.
    assert AUTH_TOKEN_ENV.endswith("_AUTH_TOKEN")
