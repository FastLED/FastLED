"""Regression tests for boards with an unusable Arduino I2S header."""

from __future__ import annotations

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def _assert_guard_skips_poisonous_i2s_header(
    tmp_path: Path, platform: str, define: str
) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
    if compiler is None:
        pytest.skip("No C++ preprocessor is available")

    include_dir = tmp_path / "include"
    include_dir.mkdir()
    (include_dir / "Arduino.h").write_text("#pragma once\n", encoding="utf-8")
    (include_dir / "I2S.h").write_text(
        f'#error "{platform} I2S.h must not be included"\n',
        encoding="utf-8",
    )

    source = tmp_path / f"{platform.lower()}_i2s_guard.cpp"
    source.write_text(
        f"""
#include "platforms/arduino/audio_input.hpp"

#if ARDUINO_I2S_FULLY_SUPPORTED
#error "{platform} generic Arduino I2S support must remain disabled"
#endif
""",
        encoding="utf-8",
    )

    proc = RunningProcess.run(
        [
            compiler,
            "-E",
            "-x",
            "c++",
            f"-D{define}",
            f"-I{include_dir}",
            f"-I{PROJECT_ROOT / 'src'}",
            str(source),
        ],
        check=False,
        capture_output=True,
        text=True,
        timeout=120,
    )
    assert proc.returncode == 0, (
        f"{platform} preprocessing failed with exit code {proc.returncode}:\n"
        f"{proc.stdout}"
    )


def test_renesas_guard_skips_poisonous_i2s_header(tmp_path: Path) -> None:
    _assert_guard_skips_poisonous_i2s_header(
        tmp_path, "Renesas", "ARDUINO_ARCH_RENESAS"
    )


def test_samd51_guard_skips_poisonous_i2s_header(tmp_path: Path) -> None:
    """#4030: SAMD51 must not include Adafruit's incompatible I2S library."""
    _assert_guard_skips_poisonous_i2s_header(tmp_path, "SAMD51", "__SAMD51J19A__")
