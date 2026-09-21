"""Regression coverage for the fbuild Python ABI baseline."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

FIXED_PYTHON_WORKFLOWS = (
    ".github/workflows/avr8js_docker_template.yml",
    ".github/workflows/build_clone_and_compile.yml",
    ".github/workflows/build_esp_extra_libs.yml",
    ".github/workflows/build_linux.yml",
    ".github/workflows/build_template.yml",
    ".github/workflows/build_template_binary_size.yml",
    ".github/workflows/build_wasm.yml",
    ".github/workflows/build_wasm_compilers.yml",
    ".github/workflows/header-perf.yml",
    ".github/workflows/iwyu.yml",
    ".github/workflows/mp3_conformance.yml",
    ".github/workflows/mp3_cpu_audit.yml",
    ".github/workflows/mp3_memory_audit.yml",
    ".github/workflows/project_automation.yml",
    ".github/workflows/project_drift_sync.yml",
    ".github/workflows/qemu_template.yml",
    ".github/workflows/template_example_test.yml",
    ".github/workflows/template_unit_test.yml",
)


def test_fbuild_locking_uses_the_py310_abi_baseline() -> None:
    """FastLED must resolve fbuild against its public abi3-py310 API floor."""
    manifest = (ROOT / "pyproject.toml").read_text(encoding="utf-8")
    assert 'requires-python = ">=3.10"' in manifest
    assert 'pythonVersion = "3.10"' in manifest

    for workflow in FIXED_PYTHON_WORKFLOWS:
        contents = (ROOT / workflow).read_text(encoding="utf-8")
        pinned_versions = re.findall(
            r"(?:python-version:\s*[\x27\"]|echo \"|echo \'\')(3\.\d+)",
            contents,
        )
        assert pinned_versions, f"{workflow} must pin a Python version"
        assert set(pinned_versions) == {"3.10"}, (
            f"{workflow} must only pin the Python 3.10 ABI baseline"
        )
