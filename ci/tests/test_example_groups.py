from pathlib import Path

import pytest

from ci.examples.example_groups import GROUPS, select_examples


ROOT = Path(__file__).resolve().parents[2]
EXPECTED_GROUPS = {
    "Basic",
    "Classic",
    "Advanced",
    "Fx",
    "Experimental",
    "AutoResearch",
}


def test_groups_cover_each_example_once_by_relative_parent_path() -> None:
    assert set(GROUPS) == EXPECTED_GROUPS

    grouped = [example for examples in GROUPS.values() for example in examples]
    discovered = [
        sketch.parent.relative_to(ROOT / "examples").as_posix()
        for sketch in (ROOT / "examples").rglob("*.ino")
    ]

    assert sorted(grouped) == sorted(discovered)
    assert len(grouped) == len(set(grouped))


@pytest.mark.parametrize("group", sorted(EXPECTED_GROUPS))
def test_select_examples_returns_only_requested_group(group: str) -> None:
    assert set(select_examples([group])) == set(GROUPS[group])


def test_select_examples_combines_requested_groups_without_duplicates() -> None:
    selected = select_examples(["Basic", "Classic"])
    assert set(selected) == set(GROUPS["Basic"]) | set(GROUPS["Classic"])
    assert len(selected) == len(set(selected))
