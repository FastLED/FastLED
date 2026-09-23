"""Guard the target split in the Linux core-sanitizer Meson preset."""

from pathlib import Path

import pytest

from ci.meson.streaming_runner import _make_streaming_env


def test_debug_thin_keeps_core_instrumented_and_consumers_lean() -> None:
    # FastLED #4535: keep the shared-library/consumer instrumentation split.
    native = (Path(__file__).parents[2] / "ci/meson/native/meson.build").read_text()

    assert "fastled_core_compile_args = base_compile_args" in native
    assert "if build_mode == 'debug-thin'" in native
    assert "'-fsanitize=address', '-fsanitize=undefined'" in native
    assert "cpp_args: fastled_core_compile_args + ['-DFASTLED_BUILDING_DLL']" in native
    assert "unit_test_compile_args = base_compile_args + test_specific_args" in native
    assert "example_compile_args = base_compile_args" in native
    assert "dll_link_args = core_link_args + sanitizer_link_args" in native
    assert "if build_mode == 'debug' or build_mode == 'debug-thin'" in native


def test_debug_thin_has_a_distinct_meson_mode_and_linux_guard() -> None:
    root = Path(__file__).parents[2]
    options = (root / "meson.options").read_text()
    project = (root / "meson.build").read_text()

    assert "'debug-thin'" in options
    assert (
        "build_mode == 'debug-thin' and (meson.is_cross_build() or host_machine.system() != 'linux')"
        in project
    )


def test_streamed_debug_thin_tests_get_sanitizer_headroom(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("FASTLED_TEST_TIMEOUT", raising=False)
    source = Path("/source")
    build = Path("/build")

    assert (
        _make_streaming_env(source, build, "debug-thin")["FASTLED_TEST_TIMEOUT"] == "60"
    )
    assert _make_streaming_env(source, build, "debug")["FASTLED_TEST_TIMEOUT"] == "60"
    assert "FASTLED_TEST_TIMEOUT" not in _make_streaming_env(source, build, "quick")
