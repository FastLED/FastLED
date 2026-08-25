"""Tests for resolving example names to sketch directories.

Examples live either directly under `examples/` (`examples/Blink/`) or nested
(`examples/Fx/FxCylon/`). The WASM build path previously assumed the flat
layout, so every nested sketch failed with FileNotFoundError.
"""

import pytest

from ci.wasm_build import PROJECT_ROOT, get_sketch_cache_dir, resolve_example_dir


def test_resolves_a_flat_example() -> None:
    assert resolve_example_dir("Blink") == PROJECT_ROOT / "examples" / "Blink"


def test_resolves_a_nested_example_by_bare_name() -> None:
    """The regression: `FxCylon` lives at examples/Fx/FxCylon/, not examples/FxCylon/."""
    resolved = resolve_example_dir("FxCylon")

    assert resolved == PROJECT_ROOT / "examples" / "Fx" / "FxCylon"
    assert (resolved / "FxCylon.ino").is_file()


def test_resolves_a_nested_example_by_relative_path() -> None:
    assert (
        resolve_example_dir("Fx/FxCylon")
        == PROJECT_ROOT / "examples" / "Fx" / "FxCylon"
    )


def test_flat_lookup_wins_without_a_tree_walk() -> None:
    """A directly-present example resolves without scanning, preserving old behavior."""
    assert resolve_example_dir("Blink").parent == PROJECT_ROOT / "examples"


def test_unknown_example_raises() -> None:
    with pytest.raises(FileNotFoundError, match="Sketch not found"):
        resolve_example_dir("NoSuchSketchAnywhere")


def test_directory_without_matching_ino_is_not_resolved() -> None:
    """examples/Fx/ exists but has no Fx.ino, so it must not resolve."""
    with pytest.raises(FileNotFoundError):
        resolve_example_dir("Fx")


@pytest.mark.parametrize(
    "name",
    ["FxCylon", "FxDemoReel100", "FxEngine", "FxFire2012", "FxNoiseRing"],
)
def test_every_nested_fx_example_resolves(name: str) -> None:
    """All of examples/Fx/* were broken before this fix."""
    resolved = resolve_example_dir(name)

    assert resolved.is_dir()
    assert (resolved / f"{name}.ino").is_file()


def test_relative_path_resolves_to_same_dir_as_bare_name() -> None:
    """`Fx/FxCylon` and `FxCylon` must resolve identically.

    Guards the wasm_compile path, which now forwards the full relative sketch
    path so two sketches sharing a basename stay distinguishable.
    """
    assert resolve_example_dir("Fx/FxCylon") == resolve_example_dir("FxCylon")


def test_resolved_dir_name_is_always_the_bare_sketch_name() -> None:
    """Generated filenames derive from this, so it must never contain a separator.

    Deriving from the caller's string instead would turn "Fx/FxCylon" into a
    nested wrapper path (sketch_cache_dir/Fx/FxCylon_wrapper.cpp).
    """
    for name in ("FxCylon", "Fx/FxCylon"):
        assert resolve_example_dir(name).name == "FxCylon"


def test_sketch_cache_isolated_across_backend_selector_switches() -> None:
    default = get_sketch_cache_dir("Blink")
    minimp3 = get_sketch_cache_dir("Blink", defines=["FASTLED_MP3_BACKEND_MINIMP3"])
    restored_default = get_sketch_cache_dir("Blink")

    assert minimp3 != default
    assert restored_default == default
