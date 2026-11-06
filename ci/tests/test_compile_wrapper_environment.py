"""Real compile-wrapper provisioning controls; no compiler or installer runs."""

import os
import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
    "profile",
    ["explicit-existing", "local-existing", "local-missing", "explicit-missing"],
)
def test_compile_wrapper_respects_selected_provisioned_environment(
    tmp_path: Path, profile: str
) -> None:
    bash = shutil.which("bash")
    if bash is None:
        pytest.skip("requires an available Bash to execute the actual copied wrapper")
    project = tmp_path / "copied project"
    project.mkdir()
    shutil.copy2(ROOT / "compile", project / "compile")
    helper = project / "ci/util/keep_editable_fbuild.sh"
    helper.parent.mkdir(parents=True)
    shutil.copy2(ROOT / "ci/util/keep_editable_fbuild.sh", helper)
    tools = tmp_path / "mock tools"
    tools.mkdir()
    trace = tmp_path / "uv-invocation.txt"
    installed = tmp_path / "install-attempt.txt"
    uv = tools / "uv"
    uv.write_bytes(
        b'#!/usr/bin/env bash\nprintf "%s\\n" "${UV_PROJECT_ENVIRONMENT-}" "$@" > "$UV_TRACE"\n'
    )
    uv.chmod(0o755)
    install = project / "install"
    install.write_bytes(
        b'#!/usr/bin/env bash\nprintf "installer-must-not-run\\n" > "$INSTALL_TRACE"\nexit 47\n'
    )
    install.chmod(0o755)
    external = tmp_path / "already provisioned environment"
    if profile == "explicit-existing":
        external.mkdir()
        (external / "pyvenv.cfg").write_bytes(
            b"# mock provisioning-precondition fixture\n"
        )
    if profile in {"local-existing", "explicit-missing"}:
        (project / ".venv").mkdir()
        (project / ".venv/pyvenv.cfg").write_bytes(
            b"# mock provisioning-precondition fixture\n"
        )
    env = dict(os.environ)
    env.pop("UV_PROJECT_ENVIRONMENT", None)
    env.update(
        PATH=tools.as_posix() + os.pathsep + env.get("PATH", os.defpath),
        UV_TRACE=trace.as_posix(),
        INSTALL_TRACE=installed.as_posix(),
    )
    if profile.startswith("explicit"):
        env["UV_PROJECT_ENVIRONMENT"] = external.as_posix()
    foreign = tmp_path / "foreign cwd"
    foreign.mkdir()
    result = RunningProcess.run(
        [
            bash,
            (project / "compile").as_posix(),
            "rp2040",
            "--examples",
            "AutoResearch",
        ],
        cwd=foreign,
        env=env,
        check=False,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
        timeout=30,
    )
    if profile == "explicit-missing":
        assert result.returncode != 0
        assert "UV_PROJECT_ENVIRONMENT" in result.stdout + str(
            getattr(result, "stderr", "") or ""
        )
        assert not installed.exists(), (
            "missing explicit target cannot install into a different local venv"
        )
        assert not trace.exists(), "invalid explicit environment must reject before uv"
    elif profile == "local-missing":
        assert result.returncode == 47
        assert installed.is_file(), "default local provisioning remains available"
        assert not trace.exists()
    else:
        assert result.returncode == 0
        assert not installed.exists(), (
            "an existing selected environment needs no provisioning"
        )
        args = trace.read_text().splitlines()
        assert args[0] == (
            external.as_posix() if profile == "explicit-existing" else ""
        )
        assert args[1:] == [
            "run",
            "ci/ci-compile.py",
            "rp2040",
            "--examples",
            "AutoResearch",
        ]


@pytest.mark.parametrize(
    "profile",
    [
        "external-editable",
        "external-clean-local-editable",
        "relative-external-editable",
    ],
)
def test_compile_wrapper_editable_guard_uses_only_selected_environment(
    tmp_path: Path, profile: str
) -> None:
    bash = shutil.which("bash")
    if bash is None:
        pytest.skip("requires an available Bash to execute the actual copied helper")
    project = tmp_path / "copied project"
    project.mkdir()
    shutil.copy2(ROOT / "compile", project / "compile")
    helper = project / "ci/util/keep_editable_fbuild.sh"
    helper.parent.mkdir(parents=True)
    shutil.copy2(ROOT / "ci/util/keep_editable_fbuild.sh", helper)
    selected = (
        project / "external environment"
        if profile.startswith("relative")
        else tmp_path / "external environment"
    )
    selected.mkdir()
    editable = (
        project / ".venv" if profile == "external-clean-local-editable" else selected
    )
    metadata = (
        editable
        / "lib/python3.12/site-packages/fbuild-2.5.52.dist-info/direct_url.json"
    )
    metadata.parent.mkdir(parents=True)
    metadata.write_bytes(
        b'{"url":"file:///fixture/fbuild","dir_info":{"editable":true}}\n'
    )
    tools = tmp_path / "mock tools"
    tools.mkdir()
    trace = tmp_path / "uv-selected-environment.txt"
    uv = tools / "uv"
    uv.write_bytes(
        b'#!/usr/bin/env bash\nprintf "%s\\n" "${UV_NO_SYNC-}" "${UV_PROJECT_ENVIRONMENT-}" > "$UV_TRACE"\n'
    )
    uv.chmod(0o755)
    # A wrong install cannot silently satisfy this test.
    install = project / "install"
    install.write_bytes(b"#!/usr/bin/env bash\nexit 47\n")
    install.chmod(0o755)
    env = dict(os.environ)
    env.pop("UV_NO_SYNC", None)
    env["UV_PROJECT_ENVIRONMENT"] = (
        "external environment"
        if profile.startswith("relative")
        else selected.as_posix()
    )
    env.update(
        PATH=tools.as_posix() + os.pathsep + env.get("PATH", os.defpath),
        UV_TRACE=trace.as_posix(),
    )
    foreign = tmp_path / "foreign cwd"
    foreign.mkdir()
    result = RunningProcess.run(
        [
            bash,
            (project / "compile").as_posix(),
            "rp2040",
            "--examples",
            "AutoResearch",
        ],
        cwd=foreign,
        env=env,
        check=False,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
        timeout=30,
    )
    assert result.returncode == 0
    flags = trace.read_text().splitlines()
    assert flags[0] == ("" if profile == "external-clean-local-editable" else "1")
    assert flags[1] == env["UV_PROJECT_ENVIRONMENT"]
