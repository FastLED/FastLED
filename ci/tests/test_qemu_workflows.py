"""Regression tests for the native-fbuild QEMU badge workflows."""

from pathlib import Path

import pytest

from ci.compiler.argument_parser import CompilationArgumentParser


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"


def test_qemu_template_uses_fbuild_native_runner_only() -> None:
    template = (WORKFLOWS / "qemu_template.yml").read_text(encoding="utf-8")

    assert "ci/stage_fbuild_project.py" in template
    assert "fbuild test-emu" in template
    assert "--emulator qemu" in template
    assert '--halt-on-success "$SUCCESS_PATTERN"' in template
    assert "TEST_SUITE_COMPLETE: FAIL" in template
    assert "QEMU_LCD_CLOCKLESS_REGISTRATION: FAIL" in template
    assert 'EXTRA_DEFINE_ARGS=(--define "$EXTRA_DEFINE")' in template
    assert '--board "$BOARD"' in template
    assert '--example "$EXAMPLE"' in template
    assert "--environment ${{ inputs.platform }}" not in template
    assert "--board ${{ inputs.platform }}" not in template
    assert "--example ${{ inputs.sketch }}" not in template
    assert "grep -c '${{ inputs.success_pattern }}'" not in template
    assert 'SKETCH_NAME="${{ inputs.sketch }}"' not in template
    assert "ci-compile.py" not in template
    assert "merged-bin" not in template
    assert "docker" not in template.lower()
    assert not (WORKFLOWS / "qemu_docker_template.yml").exists()


def test_all_five_badge_legs_use_native_template() -> None:
    callers = {
        "qemu_esp32dev_test.yml": ["BlinkParallel"],
        "qemu_esp32c3_test.yml": ["BlinkParallel", "Test"],
        "qemu_esp32s3_test.yml": [
            "BlinkParallel",
            "SpecialDrivers/ESP/DriverTest",
        ],
    }

    for workflow_name, sketches in callers.items():
        workflow = (WORKFLOWS / workflow_name).read_text(encoding="utf-8")
        assert "uses: ./.github/workflows/qemu_template.yml" in workflow
        assert "pull_request:" in workflow
        assert "permissions:\n  contents: read" in workflow
        for sketch in sketches:
            assert sketch in workflow

    all_callers = "\n".join(
        (WORKFLOWS / workflow_name).read_text(encoding="utf-8")
        for workflow_name in callers
    )
    assert "Initialized 4 LED strips with 256 LEDs each" in all_callers
    assert "Test loop!" in all_callers
    assert "QEMU_LCD_CLOCKLESS_REGISTRATION: PASS" in all_callers
    assert "FL_QEMU_VALIDATE_LCD_CLOCKLESS" in all_callers

    driver_test = (
        ROOT / "examples" / "SpecialDrivers" / "ESP" / "DriverTest" / "DriverTest.ino"
    ).read_text(encoding="utf-8")
    assert "FastLED.enableAllDrivers();" in driver_test
    assert "setExclusiveDriver<fl::Bus::FLEX_IO" not in driver_test


def test_retired_merged_bin_flag_is_rejected() -> None:
    parser = CompilationArgumentParser(ROOT)

    with pytest.raises(SystemExit):
        parser.parse(["esp32dev", "Blink", "--merged-bin"])
