"""Compile-time regression contract for the ESP8266 clockless wait time."""

import re
import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "src" / "platforms" / "esp" / "8266" / "clockless_esp8266.h"
TEMPLATE_DEFAULT = re.compile(r"int WAIT_TIME = (\d+)")


def _preprocessed_wait_time(override: int | None = None) -> int:
    compiler = shutil.which("clang++") or shutil.which("g++") or shutil.which("c++")
    if compiler is None:
        pytest.skip("No C++ preprocessor is available")

    command = [
        compiler,
        "-E",
        "-P",
        "-x",
        "c++",
        f"-I{ROOT / 'src'}",
        "-DF_CPU=80000000",
    ]
    if override is not None:
        command.append(f"-DFASTLED_ESP8266_CLOCKLESS_WAIT_TIME={override}")
    command.append(str(HEADER))

    result = RunningProcess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=120,
    )
    assert result.returncode == 0, f"preprocessing failed:\n{result.stderr}"
    match = TEMPLATE_DEFAULT.search(result.stdout)
    assert match is not None, "ClocklessController WAIT_TIME default was not found"
    return int(match.group(1))


def test_esp8266_clockless_wait_time_default_is_backward_compatible() -> None:
    assert _preprocessed_wait_time() == 85


def test_esp8266_clockless_wait_time_accepts_sketch_override() -> None:
    assert _preprocessed_wait_time(10) == 10
