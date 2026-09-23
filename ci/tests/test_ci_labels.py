"""Regression checks for the generated board CI selector."""

import copy
from pathlib import Path

import pytest
import yaml

from ci.ci_labels import ROOT, board_jobs, catalog, expand, rendered, select
from ci.ci_labels import test_jobs as registered_test_jobs


JOBS = board_jobs() | registered_test_jobs()
LABELS = catalog(JOBS)


def pr_event(labels: list[str], *, fork: bool = False) -> dict:
    return {
        "repository": {"full_name": "FastLED/FastLED"},
        "pull_request": {
            "head": {
                "sha": "a" * 40,
                "repo": {"full_name": "outside/fork" if fork else "FastLED/FastLED"},
            },
            "labels": [{"name": name} for name in labels],
        },
    }


def test_default_and_label_removal_do_not_run_boards() -> None:
    event = pr_event([])
    assert select("pull_request", event, LABELS).mode == "minimal"
    assert select("pull_request", event, LABELS).cells == []
    selected = select("pull_request", pr_event(["ci-platform:esp*"]), LABELS)
    assert selected.sha == "a" * 40
    assert selected.cells
    assert select("pull_request", event, LABELS).cells == []
    assert select("push", {"after": "b" * 40}, LABELS).cells == []


def test_full_covers_every_board_job_and_overrides_selectors() -> None:
    full = select("pull_request", pr_event(["ci-full", "ci-platform:teensy41"]), LABELS)
    assert full.mode == "full"
    assert set(full.cells) == {
        f"{workflow}/{job}" for workflow, entries in JOBS.items() for job in entries
    }
    assert select("workflow_dispatch", {"after": "a" * 40}, LABELS).cells == []


def test_prefix_exact_companion_and_union() -> None:
    esp = set(expand("ci-platform:esp*", LABELS))
    teen = set(expand("ci-platform:teensy41", LABELS))
    assert "build_esp32s3.yml/build" in esp
    assert "check_esp32_size.yml/build" in esp
    assert "build_teensy41.yml/build" in teen
    assert "check_teensy41_size.yml/build" in teen
    union = select(
        "pull_request", pr_event(["ci-platform:esp*", "ci-platform:teensy41"]), LABELS
    )
    assert set(union.cells) == esp | teen


def test_test_family_selector_is_separate_from_board_selector() -> None:
    qemu = set(expand("ci-test:qemu*", LABELS))
    assert "qemu_esp32c3_test.yml/esp32c3_qemu_test" in qemu
    assert "qemu_esp32s3_test.yml/esp32s3_qemu_lcd" in qemu
    assert qemu <= set(expand("ci-platform:esp*", LABELS))
    assert "bloat_regression_esp32s3.yml/bloat_regression" in expand(
        "ci-platform:esp32s3", LABELS
    )
    assert "avr8js_uno_test.yml/uno_avr8js_test" in expand("ci-platform:uno", LABELS)
    assert "build_wasm.yml/build" in expand("ci-platform:wasm", LABELS)
    selected = select(
        "pull_request", pr_event(["ci-test:qemu*", "ci-platform:teensy41"]), LABELS
    )
    assert set(selected.cells) == qemu | set(expand("ci-platform:teensy41", LABELS))


def test_invalid_labels_fail_and_forks_are_blocked() -> None:
    with pytest.raises(ValueError, match="unknown CI labels"):
        select("pull_request", pr_event(["ci-platform:imaginary*"]), LABELS)
    with pytest.raises(ValueError, match="unknown or empty"):
        expand("ci-platform:imaginary", LABELS)
    assert (
        select("pull_request", pr_event(["ci-full"], fork=True), LABELS).mode
        == "fork-blocked"
    )
    assert select("pull_request", pr_event(["ci-full"], fork=True), LABELS).cells == []


def test_generated_guards_and_exact_sha_checkout_do_not_drift() -> None:
    for workflow, entries in JOBS.items():
        path = ROOT / ".github" / "workflows" / workflow
        source = path.read_text(encoding="utf-8")
        assert rendered(path, entries, LABELS) == source, workflow
        data = yaml.load(source, Loader=yaml.BaseLoader)
        assert {"labeled", "unlabeled", "synchronize"} <= set(
            data["on"]["pull_request"]["types"]
        )
    for name in (
        "build_template.yml",
        "build_template_binary_size.yml",
        "template_unit_test.yml",
        "template_example_test.yml",
        "qemu_template.yml",
        "avr8js_docker_template.yml",
    ):
        text = (ROOT / ".github" / "workflows" / name).read_text(encoding="utf-8")
        assert "ref: ${{ github.event.pull_request.head.sha || github.sha }}" in text


def test_registry_change_requires_regenerating_guards(tmp_path: Path) -> None:
    original = ROOT / ".github" / "workflows" / "build_teensy41.yml"
    target = tmp_path / original.name
    target.write_text(
        original.read_text(encoding="utf-8").replace(
            "ci-platform:teensy*", "ci-platform:wrong*"
        ),
        encoding="utf-8",
    )
    assert rendered(
        target, copy.deepcopy(JOBS[original.name]), LABELS
    ) != target.read_text(encoding="utf-8")
