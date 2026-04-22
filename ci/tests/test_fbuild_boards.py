"""Tests for fbuild board routing decisions."""

import tomllib
from pathlib import Path

from ci.compiler.fbuild_boards import FBUILD_BOARDS


def test_teensy_4x_uses_fbuild() -> None:
    assert "teensy40" in FBUILD_BOARDS
    assert "teensy41" in FBUILD_BOARDS


def test_known_fbuild_boards_remain_enabled() -> None:
    assert "uno" in FBUILD_BOARDS
    assert "esp32dev" in FBUILD_BOARDS


def test_fbuild_dependency_requires_teensy_toolchain_fix() -> None:
    pyproject = Path(__file__).resolve().parents[2] / "pyproject.toml"
    dependencies = tomllib.loads(pyproject.read_text())["project"]["dependencies"]

    assert "fbuild>=2.2.1" in dependencies
