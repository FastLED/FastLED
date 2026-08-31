"""Regression coverage for ATmega644 hardware SPI pins (FastLED #1390)."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


REPO_ROOT = Path(__file__).resolve().parents[2]
SRC = REPO_ROOT / "src"


def _compiler() -> str:
    for name in ("clang++", "clang", "g++", "gcc"):
        if compiler := shutil.which(name):
            return compiler
    pytest.skip("no host compiler available to run the preprocessor")


@pytest.mark.parametrize("suffix", ("", "A", "P", "PA"))
def test_atmega644_variant_has_hardware_spi_pins(suffix: str) -> None:
    device_macro = f"__AVR_ATmega644{suffix}__"
    source = """
#include "platforms/avr/is_avr.h"
#include "platforms/avr/atmega/common/fastpin_legacy_other.h"
"""
    command = [
        _compiler(),
        "-E",
        "-dM",
        "-x",
        "c++",
        f"-I{SRC}",
        f"-D{device_macro}",
        "-DPORTA=porta",
        "-DPORTB=portb",
        "-DPORTC=portc",
        "-DPORTD=portd",
        "-",
    ]
    result = RunningProcess.run(
        command,
        input=source,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )

    assert result.returncode == 0, f"preprocessing failed:\n{result.stderr}"
    macros: list[str] = []
    for line in result.stdout.splitlines():
        macros.append(line.rstrip())
    assert "#define FL_IS_AVR_ATMEGA" in macros
    assert "#define SPI_DATA 5" in macros
    assert "#define SPI_CLOCK 7" in macros
    assert "#define SPI_SELECT 4" in macros
    assert "#define AVR_HARDWARE_SPI 1" in macros
