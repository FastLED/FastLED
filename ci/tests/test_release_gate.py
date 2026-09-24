"""The release gate must fail closed before a tag can be created."""

from pathlib import Path

import pytest
import yaml

from ci import release_gate


SHA = "a" * 40


def test_catalog_includes_board_tests_and_hosted_macos() -> None:
    required = release_gate.required_workflows()
    assert "build.yml" in required
    assert "qemu_esp32c3_test.yml" in required
    assert "build_wasm.yml" in required
    assert "ci-labels.yml" in required
    assert required["unit_test_macos.yml"] == 2
    assert required["example_test_macos.yml"] == 2
    assert "unit_test_linux.yml" in required
    assert "unit_test_windows.yml" in required


def test_every_release_cell_can_be_dispatched() -> None:
    workflows = Path(__file__).resolve().parents[2] / ".github/workflows"
    for filename in release_gate.required_workflows():
        data = yaml.load((workflows / filename).read_text(), Loader=yaml.BaseLoader)
        assert "workflow_dispatch" in data["on"], filename


def test_run_requires_exact_sha_successful_dispatch_and_all_jobs(monkeypatch) -> None:
    run = {
        "head_sha": SHA,
        "event": "workflow_dispatch",
        "conclusion": "success",
        "id": 17,
    }
    jobs = {"jobs": [{"name": "build", "conclusion": "success"}]}

    def fake_api(_method, path, _payload=None):
        return {"workflow_runs": [run]} if "/runs?" in path else jobs

    monkeypatch.setattr(release_gate, "api", fake_api)
    assert release_gate.verify_run(SHA, "build_uno.yml", 1) == 17
    for change in (
        {"head_sha": "b" * 40},
        {"event": "pull_request"},
        {"conclusion": "failure"},
    ):
        run.update(change)
        assert release_gate.verify_run(SHA, "build_uno.yml", 1) is None
        run.update(
            {"head_sha": SHA, "event": "workflow_dispatch", "conclusion": "success"}
        )
    jobs["jobs"][0]["conclusion"] = "skipped"
    assert release_gate.verify_run(SHA, "build_uno.yml", 1) is None
    jobs["jobs"] = []
    assert release_gate.verify_run(SHA, "build_uno.yml", 1) is None


def test_macos_requires_both_hosted_architectures(monkeypatch) -> None:
    jobs = {
        "jobs": [
            {"name": "test / build (macos-15-intel)", "conclusion": "success"},
            {"name": "test / build (macos-15)", "conclusion": "success"},
        ]
    }

    def fake_api(_method, path, _payload=None):
        if "/runs?" in path:
            return {
                "workflow_runs": [
                    {
                        "head_sha": SHA,
                        "event": "workflow_dispatch",
                        "conclusion": "success",
                        "id": 18,
                    }
                ]
            }
        return jobs

    monkeypatch.setattr(release_gate, "api", fake_api)
    assert release_gate.verify_run(SHA, "unit_test_macos.yml", 2) == 18
    jobs["jobs"][1]["name"] = "test / build (macos-15-intel)"
    assert release_gate.verify_run(SHA, "unit_test_macos.yml", 2) is None


def test_missing_workflow_fails_entire_release(monkeypatch) -> None:
    monkeypatch.setattr(
        release_gate, "required_workflows", lambda: {"build_uno.yml": 1}
    )
    monkeypatch.setattr(release_gate, "verify_run", lambda *_args: None)
    with pytest.raises(ValueError, match="missing exact-SHA successful full CI"):
        release_gate.verify(SHA)


def test_dispatch_requires_candidate_at_current_master(monkeypatch) -> None:
    calls = []
    monkeypatch.setattr(release_gate, "master_sha", lambda: "b" * 40)
    monkeypatch.setattr(release_gate, "api", lambda *args: calls.append(args))
    with pytest.raises(ValueError, match="current master HEAD"):
        release_gate.dispatch_full(SHA)
    assert calls == []


def test_release_workflow_is_manual_and_gate_precedes_writes() -> None:
    path = Path(__file__).resolve().parents[2] / ".github/workflows/release.yml"
    source = path.read_text()
    data = yaml.load(source, Loader=yaml.BaseLoader)
    assert set(data["on"]) == {"workflow_dispatch"}
    assert {"candidate_sha", "dry_run"} <= set(
        data["on"]["workflow_dispatch"]["inputs"]
    )
    steps = data["jobs"]["release"]["steps"]
    gate_index = next(
        i
        for i, step in enumerate(steps)
        if step.get("name") == "Verify exact candidate full CI"
    )
    for i, step in enumerate(steps):
        if "gh release create" in step.get("run", "") or "gh workflow run" in step.get(
            "run", ""
        ):
            assert i > gate_index
            assert "inputs.dry_run == false" in step["if"]
    assert "Require candidate is current master" in [
        step.get("name") for step in steps[:gate_index]
    ]
    assert "git/ref/heads/master" in source
    assert '--target "$CANDIDATE_SHA"' in source
