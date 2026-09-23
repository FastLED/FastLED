"""Contract for the native, highly amalgamated compile-tests gate."""

import re
from pathlib import Path

import pytest

from ci.util.test_args import parse_args


ROOT = Path(__file__).resolve().parents[2]
GATE_DIR = ROOT / "ci/meson/compile_tests"


def test_compile_tests_has_one_native_translation_unit() -> None:
    sources = sorted(GATE_DIR.glob("*.cpp"))
    assert [source.name for source in sources] == ["compile_tests.cpp"]

    source = sources[0].read_text(encoding="utf-8")
    for public_api in (
        "FastLED.h",
        "CRGB",
        "CHSV",
        "fill_solid",
        "fill_rainbow",
        "ColorFromPalette",
        "beatsin8",
        "inoise8",
        "FastLED.addLeds",
        "FastLED.show",
    ):
        assert public_api in source
    assert "examples/Blink/Blink.ino" in source


def test_compile_tests_is_a_single_native_meson_target() -> None:
    root_meson = (ROOT / "meson.build").read_text(encoding="utf-8")
    gate_meson = (GATE_DIR / "meson.build").read_text(encoding="utf-8")

    assert "subdir('ci/meson/compile_tests')" in root_meson
    assert re.search(r"executable\s*\(\s*['\"]compile-tests['\"]", gate_meson)
    assert "compile_tests.cpp" in gate_meson
    assert "shared_library(" not in gate_meson
    assert "example_dlls" not in gate_meson


def test_group_selects_examples_without_named_sketches() -> None:
    args = parse_args(["--example-group", "CompileTests"])
    assert args.examples == []
    assert args.example_group == "CompileTests"


def test_group_rejects_named_sketches() -> None:
    with pytest.raises(SystemExit):
        parse_args(["--examples", "Blink", "--example-group", "CompileTests"])


def test_host_group_rejects_board_only_autoresearch(
    capsys: pytest.CaptureFixture[str],
) -> None:
    with pytest.raises(SystemExit):
        parse_args(["--example-group", "AutoResearch"])
    assert "AutoResearch runs on ESP32-S3" in capsys.readouterr().err
