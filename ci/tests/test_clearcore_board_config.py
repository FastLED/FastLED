"""Regression contracts for the ClearCore SAME53 compile badge."""

from pathlib import Path

from ci.boards import create_board


REPO_ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = REPO_ROOT / ".github" / "workflows" / "build_clearcore.yml"


def test_clearcore_board_profile_selects_the_fbuild_native_target() -> None:
    board = create_board("clearcore")

    assert board.board_name == "clearcore"
    assert board.platform == "atmelsam"
    assert board.framework == "arduino"
    assert board.defines == ["FASTLED_USES_ARDUINO_AUDIO_INPUT=0"]

    ini = board.to_platformio_ini()
    assert "[env:clearcore]" in ini
    assert "platform = atmelsam" in ini
    assert "board = clearcore" in ini
    assert "-DFASTLED_USES_ARDUINO_AUDIO_INPUT=0" in ini


def test_clearcore_badge_runs_a_real_blink_compile() -> None:
    workflow = WORKFLOW.read_text(encoding="utf-8")

    assert "uses: ./.github/workflows/build_template.yml" in workflow
    assert "args: clearcore Blink" in workflow
    assert 'fbuild_version: "2.5.19"' in workflow
    assert "prefer_setup_fbuild: true" in workflow
    assert "workflow_dispatch:" in workflow
