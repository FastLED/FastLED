import os
from pathlib import Path

import pytest

from ci.examples.example_groups import GROUPS, select_examples
from ci.meson.meson_setup_execute import FASTLED_MESON_BUILD_FILES
from ci.meson.meson_setup_phases import check_meson_build_modified


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


def test_group_manifest_change_reconfigures_existing_build(tmp_path: Path) -> None:
    source_dir = tmp_path / "source"
    manifest = source_dir / "ci" / "examples" / "example_groups.py"
    for relative_path in FASTLED_MESON_BUILD_FILES:
        input_file = source_dir / relative_path
        input_file.parent.mkdir(parents=True, exist_ok=True)
        input_file.write_text("", encoding="utf-8")
        os.utime(input_file, (50, 50))

    build_dir = tmp_path / "build"
    build_dir.mkdir()
    build_ninja = build_dir / "build.ninja"
    build_ninja.write_text("", encoding="utf-8")
    os.utime(build_ninja, (100, 100))
    assert not check_meson_build_modified(source_dir, build_dir)

    os.utime(manifest, (200, 200))

    assert check_meson_build_modified(source_dir, build_dir)
    manifest.unlink()
    assert check_meson_build_modified(source_dir, build_dir)


def test_group_manifest_is_a_zccache_configure_input() -> None:
    assert "ci/examples/example_groups.py" in FASTLED_MESON_BUILD_FILES
    assert "meson.options" in FASTLED_MESON_BUILD_FILES
