"""Regression test for Renesas boards with an unusable Arduino I2S header."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

import pytest


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def test_renesas_guard_skips_poisonous_i2s_header(tmp_path: Path) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
    if compiler is None:
        pytest.skip("No C++ preprocessor is available")

    include_dir = tmp_path / "include"
    include_dir.mkdir()
    (include_dir / "Arduino.h").write_text("#pragma once\n", encoding="utf-8")
    (include_dir / "I2S.h").write_text(
        '#error "Renesas I2S.h must not be included"\n',
        encoding="utf-8",
    )

    source = tmp_path / "renesas_i2s_guard.cpp"
    source.write_text(
        """
#include "platforms/arduino/audio_input.hpp"

#if ARDUINO_I2S_FULLY_SUPPORTED
#error "Renesas I2S support must remain disabled"
#endif
""",
        encoding="utf-8",
    )

    subprocess.run(
        [
            compiler,
            "-E",
            "-x",
            "c++",
            "-DARDUINO_ARCH_RENESAS",
            f"-I{include_dir}",
            f"-I{PROJECT_ROOT / 'src'}",
            str(source),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
