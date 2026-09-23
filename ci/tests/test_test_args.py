import pytest

from ci.util import test_args
from ci.util.test_args import parse_args


def test_tiny_layout_query_bypasses_host_dll_smart_selection() -> None:
    parsed = parse_args(["--unit", "color_profile_tiny_layout"])
    assert parsed.test == "color_profile_tiny_layout"
    assert parsed.cpp
    assert parsed.unit


def test_debug_thin_shorthand_matches_named_build_mode() -> None:
    shorthand = parse_args(["--unit", "--debug-thin"])
    named = parse_args(["--unit", "--build-mode", "debug-thin"])
    assert shorthand.build_mode == named.build_mode == "debug-thin"
    assert not shorthand.debug


@pytest.mark.parametrize("other", [["--debug"], ["--build-mode", "quick"]])
def test_debug_thin_rejects_conflicting_modes(other: list[str]) -> None:
    with pytest.raises(SystemExit):
        parse_args(["--debug-thin", *other])


def test_debug_thin_is_explained_in_help(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit) as exc:
        parse_args(["--help"])
    assert exc.value.code == 0
    assert "--debug-thin" in capsys.readouterr().out


@pytest.mark.parametrize("mode", ["--debug-thin", "--build-mode"])
def test_debug_thin_rejects_other_hosts(
    monkeypatch: pytest.MonkeyPatch, mode: str
) -> None:
    monkeypatch.setattr(test_args.sys, "platform", "darwin")
    argv = ["--unit", mode]
    if mode == "--build-mode":
        argv.append("debug-thin")
    with pytest.raises(SystemExit):
        parse_args(argv)


def test_debug_thin_rejects_emulator_runs() -> None:
    with pytest.raises(SystemExit):
        parse_args(["--debug-thin", "--run", "uno"])
