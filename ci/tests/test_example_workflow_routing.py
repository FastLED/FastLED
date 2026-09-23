"""Keep the live example smoke gate separate from the full nightly sweep."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"


def test_live_example_workflows_select_compile_tests() -> None:
    template = (WORKFLOWS / "template_example_test.yml").read_text(encoding="utf-8")
    assert '--example-group "$FASTLED_EXAMPLE_GROUP"' in template
    assert "Run Example Blink" not in template
    assert "Run All Examples" not in template

    for os_name in ("linux", "macos", "windows"):
        caller = (WORKFLOWS / f"example_test_{os_name}.yml").read_text(encoding="utf-8")
        assert "example-group: CompileTests" in caller
        assert "pull_request:" in caller


def test_nightly_and_full_sanitizer_select_full_examples() -> None:
    nightly = (WORKFLOWS / "nightly-examples.yml").read_text(encoding="utf-8")
    assert "schedule:" in nightly
    assert "workflow_dispatch:" in nightly
    assert "example-group: Nightly" in nightly

    full_sanitizer = (WORKFLOWS / "full_sanitizer_linux.yml").read_text(
        encoding="utf-8"
    )
    assert "example-group: Nightly" in full_sanitizer


def test_readme_examples_badge_reports_nightly_workflow() -> None:
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert "nightly-examples.yml/badge.svg?event=schedule" in readme
    assert "Nightly Examples (Linux)" in readme
