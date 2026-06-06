from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

from ci.boards import Board
from ci.compiler.pio import PioCompiler
from ci.util import cpu_count as cpu_count_module
from ci.util import fbuild_runner


def test_cpu_count_no_longer_caps_on_github_actions(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("GITHUB_ACTIONS", "true")
    monkeypatch.delenv("NO_PARALLEL", raising=False)
    monkeypatch.setattr(cpu_count_module.os, "cpu_count", lambda: 8)

    assert cpu_count_module.cpu_count() == 8


def test_run_fbuild_ci_uses_expected_command_and_parses_results(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    output = "\n".join(
        [
            "compile-many results:",
            "  [stage1] OK (1.25s) C:\\tmp\\sketch1  log=C:\\tmp\\sketch1\\compile_many.log  built",
            "  [stage2] FAIL (0.75s) C:\\tmp\\sketch2  log=-  build error",
        ]
    )

    class FakeRunningProcess:
        last_cmd: list[str] | None = None
        last_env: dict[str, str] | None = None

        def __init__(
            self,
            cmd: list[str],
            timeout: int,
            auto_run: bool,
            capture: bool,
            env: dict[str, str] | None = None,
        ) -> None:
            assert auto_run is False
            assert capture is True
            assert timeout == 1800
            FakeRunningProcess.last_cmd = cmd
            FakeRunningProcess.last_env = env
            self.stdout = output
            self.returncode = 0

        def start(self) -> None:
            pass

        def wait(self, echo: bool) -> int:
            assert echo is True
            return 0

    monkeypatch.setattr(fbuild_runner, "get_fbuild_executable", lambda: "fbuild.exe")
    monkeypatch.setattr(fbuild_runner, "cpu_count", lambda: 8)
    monkeypatch.setenv("GITHUB_ACTIONS", "true")
    monkeypatch.delenv("FASTLED_FRAMEWORK_JOBS", raising=False)
    monkeypatch.delenv("FASTLED_SKETCH_JOBS", raising=False)
    monkeypatch.setattr("running_process.RunningProcess", FakeRunningProcess)

    result = fbuild_runner.run_fbuild_ci(
        board="uno",
        sketch_project_dirs=[Path("sketch1"), Path("sketch2")],
        verbose=True,
        timeout=1800,
        quiet=False,
        log_file=None,
    )

    assert result.success is True
    # #2853: ensure the env passed to RunningProcess has the venv's
    # Scripts dir prepended to PATH so fbuild's internal helper-tool
    # lookups (zccache etc.) don't race a stale system-PATH copy.
    assert FakeRunningProcess.last_env is not None
    venv_scripts = str(Path(sys.executable).resolve().parent)
    actual_path = FakeRunningProcess.last_env.get("PATH", "")
    assert actual_path.split(os.pathsep, 1)[0].lower() == venv_scripts.lower()
    assert FakeRunningProcess.last_cmd == [
        "fbuild.exe",
        "ci",
        "--board",
        "uno",
        "--framework-jobs",
        "1",
        "--sketch-jobs",
        "2",
        "-v",
        "sketch1",
        "sketch2",
    ]
    assert len(result.sketch_results) == 2
    assert result.sketch_results[0].success is True
    assert result.sketch_results[0].stage == "stage1"
    assert result.sketch_results[0].sketch_dir == Path("C:\\tmp\\sketch1")
    assert result.sketch_results[1].success is False
    assert result.sketch_results[1].log_path is None
    assert result.sketch_results[1].message == "build error"


def test_run_fbuild_ci_fails_when_output_is_partially_unparseable(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    class FakeRunningProcess:
        def __init__(
            self,
            cmd: list[str],
            timeout: int,
            auto_run: bool,
            capture: bool,
            env: dict[str, str] | None = None,
        ) -> None:
            assert cmd[1] == "ci"
            assert timeout == 1800
            assert auto_run is False
            assert capture is True
            # #2853: env should be present and have venv Scripts on PATH.
            assert env is not None
            self.stdout = "\n".join(
                [
                    "compile-many results:",
                    "  [stage1] OK (1.25s) C:\\tmp\\sketch1  log=-  built",
                    "  weird output format",
                ]
            )
            self.returncode = 0

        def start(self) -> None:
            pass

        def wait(self, echo: bool) -> int:
            assert echo is True
            return 0

    monkeypatch.setattr(fbuild_runner, "get_fbuild_executable", lambda: "fbuild.exe")
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)
    monkeypatch.delenv("FASTLED_FRAMEWORK_JOBS", raising=False)
    monkeypatch.delenv("FASTLED_SKETCH_JOBS", raising=False)
    monkeypatch.setattr("running_process.RunningProcess", FakeRunningProcess)

    result = fbuild_runner.run_fbuild_ci(
        board="uno",
        sketch_project_dirs=[Path("sketch1"), Path("sketch2")],
        verbose=False,
        timeout=1800,
        quiet=False,
        log_file=None,
    )

    assert result.success is False
    assert result.returncode == 0
    assert result.sketch_results == []
    assert "weird output format" in result.output


def test_pio_compiler_build_prefers_ci_results(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    compiler = PioCompiler(
        board=Board(board_name="uno"),
        verbose=False,
        global_cache_dir=tmp_path / "cache",
        use_fbuild=True,
    )
    compiler.build_dir = tmp_path / "board"
    compiler.build_dir.mkdir(parents=True, exist_ok=True)

    root_project = compiler.build_dir
    other_project = compiler.build_dir / "compile_many" / "Demo"
    root_project.mkdir(parents=True, exist_ok=True)
    other_project.mkdir(parents=True, exist_ok=True)

    root_log = root_project / "compile_many.log"
    root_log.write_text("root ok", encoding="utf-8")
    other_log = other_project / "compile_many.log"
    other_log.write_text("demo ok", encoding="utf-8")

    # compile-many is gated behind FASTLED_USE_FBUILD_CI in
    # PioCompiler._build_fbuild (FastLED/fbuild#335). Opt in here so the
    # ci-results path actually runs in this test; without the env var
    # the dispatch falls through to the legacy serial loop and the
    # mocked run_fbuild_ci below would never be called.
    monkeypatch.setenv("FASTLED_USE_FBUILD_CI", "1")
    monkeypatch.setattr(fbuild_runner, "fbuild_supports_ci", lambda: True)
    monkeypatch.setattr(fbuild_runner, "fbuild_supports_compile_many", lambda: True)
    monkeypatch.setattr(
        compiler,
        "_stage_fbuild_compile_many_projects",
        lambda examples: [("Blink", root_project), ("Demo", other_project)],
    )

    def fake_run_ci(
        board: str,
        sketch_project_dirs: list[Path],
        verbose: bool,
        timeout: float,
        quiet: bool,
        log_file: object,
    ) -> fbuild_runner.FbuildCompileManyResult:
        assert board == "uno"
        assert sketch_project_dirs == [root_project, other_project]
        assert verbose is False
        assert timeout == 1800
        assert quiet is False
        assert log_file is None
        return fbuild_runner.FbuildCompileManyResult(
            success=True,
            output="ci output",
            returncode=0,
            sketch_results=[
                fbuild_runner.FbuildCompileManySketchResult(
                    sketch_dir=root_project,
                    success=True,
                    stage="stage1",
                    build_time_secs=1.0,
                    log_path=root_log,
                    message="ok",
                ),
                fbuild_runner.FbuildCompileManySketchResult(
                    sketch_dir=other_project,
                    success=True,
                    stage="stage2",
                    build_time_secs=0.5,
                    log_path=other_log,
                    message="ok",
                ),
            ],
        )

    monkeypatch.setattr(fbuild_runner, "run_fbuild_ci", fake_run_ci)

    def fake_generate_build_info(build_dir: Path, board: Board, example: str) -> bool:
        (build_dir / f"build_info_{example}.json").write_text(
            '{"ok": true}', encoding="utf-8"
        )
        return True

    monkeypatch.setattr(
        "ci.compiler.pio.generate_build_info_json_from_existing_build",
        fake_generate_build_info,
    )

    releases: list[str] = []
    monkeypatch.setattr(compiler.platform_lock, "release", lambda: releases.append("x"))

    futures = compiler.build(["Blink", "Demo"])
    results = [future.result() for future in futures]

    assert [result.example for result in results] == ["Blink", "Demo"]
    assert results[0].success is True
    assert results[0].output == "root ok"
    assert results[1].success is True
    assert results[1].output == "demo ok"
    assert releases == ["x"]
    assert (compiler.build_dir / "build_info_Demo.json").exists()


def test_fbuild_supports_subcommand_uses_sub_help_form(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Probe must use ``<fbuild> SUB --help`` not ``<fbuild> help SUB``.

    The latter parses ``help`` as the subcommand and ``SUB`` as a positional
    arg, which fails clap arg validation (exit 2) on any subcommand with a
    required arg like ``--board``. Until this was fixed, compile-many
    detection always returned False and CI silently fell back to the
    legacy serial loop. Regression guard for that bug.
    """
    import subprocess

    monkeypatch.setattr(fbuild_runner, "get_fbuild_executable", lambda: "fbuild.exe")
    fbuild_runner.fbuild_supports_ci.cache_clear()
    fbuild_runner.fbuild_supports_compile_many.cache_clear()

    captured: list[list[str]] = []

    class FakeProc:
        returncode = 0

    def fake_run(cmd: list[str], **_kwargs: object) -> FakeProc:  # pyright: ignore[reportUnusedFunction]
        captured.append(cmd)
        return FakeProc()

    monkeypatch.setattr(subprocess, "run", fake_run)

    assert fbuild_runner.fbuild_supports_ci() is True
    assert fbuild_runner.fbuild_supports_compile_many() is True

    assert captured == [
        ["fbuild.exe", "ci", "--help"],
        ["fbuild.exe", "compile-many", "--help"],
    ], (
        "expected `fbuild SUB --help` (clap intercepts, rc=0); "
        f"got: {captured}. The legacy `fbuild help SUB` form parses 'help' "
        "as the subcommand and exits 2 on missing required args, which "
        "silently disables compile-many."
    )
