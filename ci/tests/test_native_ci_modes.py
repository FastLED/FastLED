"""The routine native smoke inventory must never silently become a full sweep."""

from pathlib import Path

import yaml

from ci.native_ci import CPP_SMOKE, PY_SMOKE


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"


def workflow(name: str) -> dict:
    return yaml.load((WORKFLOWS / name).read_text(), Loader=yaml.BaseLoader)


def test_smoke_manifest_sources_exist_and_is_small() -> None:
    assert 1 <= len(CPP_SMOKE) <= 5
    assert 1 <= len(PY_SMOKE) <= 8
    for target, source in CPP_SMOKE.items():
        assert target
        assert (ROOT / source).is_file(), target
    for source in PY_SMOKE:
        assert (ROOT / source).is_file(), source
    assert "fastled_core" in CPP_SMOKE
    assert "channel_driver_uart" in CPP_SMOKE
    assert "ci/tests/test_color_reference_corpus.py" in PY_SMOKE


def test_full_label_retriggers_exact_sha_native_suites() -> None:
    for filename in (
        "unit_test_linux.yml",
        "unit_test_windows.yml",
        "example_test_linux.yml",
        "example_test_windows.yml",
    ):
        data = workflow(filename)
        assert {"labeled", "unlabeled", "synchronize"} <= set(
            data["on"]["pull_request"]["types"]
        )
    for filename in ("unit_test_linux.yml", "unit_test_windows.yml"):
        data = workflow(filename)
        assert "full-suite" in data["jobs"]["test"]["with"]
    windows_unit = workflow("unit_test_windows.yml")
    assert "ci-full" in windows_unit["jobs"]["test"]["with"]["full-suite"]
    assert "ci-full" in workflow("example_test_windows.yml")["jobs"]["test"]["if"]


def test_reusable_unit_template_has_distinct_full_and_smoke_paths() -> None:
    source = (WORKFLOWS / "template_unit_test.yml").read_text()
    assert "full-suite" in source
    assert "bash ci-native py" in source
    assert "bash ci-native cpp" in source
    assert "uv run test.py --py --clean" in source
    assert "./test --clang --no-parallel --unit" in source
    assert "github.event.pull_request.head.sha || github.sha" in source


def test_esp32s3_full_sweep_has_bounded_timeout_headroom() -> None:
    template = workflow("build_template.yml")
    esp32s3 = workflow("build_esp32s3.yml")
    assert (
        template["on"]["workflow_call"]["inputs"]["timeout-minutes"]["default"] == "45"
    )
    assert (
        template["jobs"]["build"]["timeout-minutes"] == "${{ inputs.timeout-minutes }}"
    )
    assert esp32s3["jobs"]["build"]["with"]["timeout-minutes"] == "60"
