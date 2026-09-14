"""The reduced PR example set and the fixes that shipped with it (#4415).

- ``smoke`` resolves to the manifest plus examples the PR changed.
- The stub build accepts the ``mem``/``plat`` filter aliases.
- The serial fbuild loop stops once ``max_failures`` builds have failed.
"""

from __future__ import annotations

from concurrent.futures import Future
from pathlib import Path
from types import SimpleNamespace

import pytest

from ci.compiler import smoke_examples
from ci.compiler.board_compiler import BoardCompiler
from ci.compiler.compiler import SketchResult
from ci.compiler.smoke_examples import example_for_path, resolve_smoke_examples
from ci.meson.discover_examples_all import should_skip_for_stub
from ci.standardized_smoke_sketches import load_smoke_sketches


PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
EXAMPLES_DIR = PROJECT_ROOT / "examples"


# --- changed-path mapping ----------------------------------------------------


def test_sketch_file_maps_to_its_example() -> None:
    assert example_for_path("examples/Blink/Blink.ino", EXAMPLES_DIR) == "Blink"


def test_nested_example_maps_to_full_relative_name() -> None:
    assert (
        example_for_path("examples/Fx/FxSdCard/FxSdCard.ino", EXAMPLES_DIR)
        == "Fx/FxSdCard"
    )


def test_helper_source_maps_to_owning_sketch() -> None:
    changed = "examples/AutoResearch/AutoResearchMathExp.cpp"
    assert example_for_path(changed, EXAMPLES_DIR) == "AutoResearch"


def test_paths_outside_a_sketch_map_to_none() -> None:
    assert example_for_path("examples/README.md", EXAMPLES_DIR) is None
    assert example_for_path("src/FastLED.h", EXAMPLES_DIR) is None


# --- resolution ----------------------------------------------------------------


def test_without_base_ref_smoke_is_the_manifest(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("GITHUB_BASE_REF", raising=False)
    monkeypatch.delenv("FASTLED_SMOKE_BASE_REF", raising=False)
    assert resolve_smoke_examples(PROJECT_ROOT) == load_smoke_sketches(PROJECT_ROOT)


def test_changed_examples_are_appended_once(monkeypatch: pytest.MonkeyPatch) -> None:
    def fake_changed(base_ref: str, project_root: Path) -> list[str]:
        assert base_ref == "master"
        return ["Blink", "Fx/FxWater", "Fx/FxWater"]

    monkeypatch.setattr(smoke_examples, "changed_examples", fake_changed)
    examples = resolve_smoke_examples(PROJECT_ROOT, base_ref="master")
    manifest = load_smoke_sketches(PROJECT_ROOT)
    assert examples[: len(manifest)] == manifest
    assert examples[len(manifest) :] == ["Fx/FxWater"]


def test_base_ref_env_precedence(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("GITHUB_BASE_REF", "master")
    monkeypatch.setenv("FASTLED_SMOKE_BASE_REF", "release")
    assert smoke_examples.base_ref_from_env() == "release"
    monkeypatch.delenv("FASTLED_SMOKE_BASE_REF")
    assert smoke_examples.base_ref_from_env() == "master"


# --- stub filter aliases --------------------------------------------------------


@pytest.mark.parametrize(
    "flt",
    ["(mem is large)", "(memory is large)", "(mem is large) and (board is not uno)"],
)
def test_memory_filters_do_not_skip_stub(flt: str) -> None:
    skip, _ = should_skip_for_stub(flt)
    assert skip is False


def test_platform_alias_is_recognised() -> None:
    assert should_skip_for_stub("(plat is esp32)")[0] is True
    assert should_skip_for_stub("(plat is native)")[0] is False


# --- failure threshold in the serial fbuild loop --------------------------------


def _fake_compiler(outcomes: dict[str, bool]) -> tuple[SimpleNamespace, list[str]]:
    built: list[str] = []

    def build_with_fbuild(example: str) -> SketchResult:
        built.append(example)
        return SketchResult(
            success=outcomes[example], output="", build_dir=Path("."), example=example
        )

    fake = SimpleNamespace(
        initialized=True,
        build_dir=Path("."),
        _restage_example=lambda example: SimpleNamespace(success=True),
        _build_with_fbuild=build_with_fbuild,
        platform_lock=SimpleNamespace(lock_file_path="x.lock", release=lambda: None),
    )
    return fake, built


def test_serial_loop_stops_at_failure_threshold() -> None:
    outcomes = {"A": False, "B": True, "C": False, "D": False, "E": True}
    fake, built = _fake_compiler(outcomes)
    futures: list[Future[SketchResult]] = BoardCompiler._build_fbuild_sync(
        fake,  # type: ignore[arg-type]
        list(outcomes),
        max_failures=2,
    )
    assert built == ["A", "B", "C"]
    assert [f.result().success for f in futures] == [False, True, False]


def test_serial_loop_without_threshold_builds_everything() -> None:
    outcomes = {"A": False, "B": False, "C": False}
    fake, built = _fake_compiler(outcomes)
    BoardCompiler._build_fbuild_sync(fake, list(outcomes))  # type: ignore[arg-type]
    assert built == ["A", "B", "C"]
