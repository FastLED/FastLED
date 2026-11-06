"""Run the real LCD driver with consistently configured production objects."""

import shutil
import sys
from pathlib import Path

import pytest
from running_process import CompletedProcess, RunningProcess


@pytest.mark.parametrize(
    "definition",
    [
        "FL_ENGINE_WORKER_CORE=256",
        "FL_ENGINE_WORKER_CORE=2",
        "FL_ENGINE_WORKER_CORE=-4",
        "FL_ENGINE_WORKER_CORE=18446744073709551613ULL",
        "FL_ENGINE_WORKER_CORE=-3.5",
        "FL_ENGINE_WORKER_CORE=fl::Priority::Auto",
        "FL_ENGINE_WORKER_PRIORITY=256",
        "FL_ENGINE_WORKER_PRIORITY=4",
        "FL_ENGINE_WORKER_PRIORITY=-1",
        "FL_ENGINE_WORKER_PRIORITY=2.5",
        "FL_ENGINE_WORKER_PRIORITY=fl::Core::Core0",
    ],
)
def test_invalid_worker_default_rejected(tmp_path: Path, definition: str) -> None:
    compiler: str | None = shutil.which("clang-tool-chain-cpp")
    if compiler is None:
        pytest.fail("clang-tool-chain-cpp required for worker-default validation")
    source: Path = tmp_path / "invalid.cpp"
    source.write_text(
        '#include "fl/channels/engine_worker_defaults.h"\n', encoding="utf-8"
    )
    result: CompletedProcess[str] = RunningProcess.run(
        [
            compiler,
            "-std=c++11",
            "-fsyntax-only",
            "-DFASTLED_STUB_IMPL=1",
            f"-D{definition}",
            "-I",
            str(Path(__file__).resolve().parents[2] / "src"),
            str(source),
        ],
        timeout=60,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    assert result.returncode != 0
    assert f"{definition.split('=', 1)[0]} must be a valid fl::" in result.stdout


@pytest.mark.skipif(
    sys.platform != "linux", reason="isolated native variant gate runs on Linux"
)
def test_engine_worker_defaults_runtime() -> None:
    result: CompletedProcess[str] = RunningProcess.run(
        ["bash", "test", "--unit", "engine_worker_defaults_runtime", "--debug"],
        cwd=Path(__file__).resolve().parents[2],
        timeout=900,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    assert result.returncode == 0, result.stdout
