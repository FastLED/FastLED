"""Unit tests for the fbuild prog_path staleness override (#2852)."""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any

from ci.boards import create_board
from ci.compiler.build_config import _override_prog_path_for_fbuild


def _touch(path: Path, mtime: float) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x7fELF")
    os.utime(path, (mtime, mtime))
    return path


def _build_data(prog_path: str | None) -> dict[str, dict[str, Any]]:
    inner: dict[str, Any] = {}
    if prog_path is not None:
        inner["prog_path"] = prog_path
    return {"esp32dev": inner}


def test_picks_release_when_only_release_exists(tmp_path: Path) -> None:
    """`fbuild build` (default) writes release/ — must be picked up."""
    release_elf = _touch(
        tmp_path / ".fbuild" / "build" / "esp32dev" / "release" / "firmware.elf",
        mtime=2000.0,
    )
    data = _build_data(
        prog_path=str(tmp_path / ".pio" / "build" / "esp32dev" / "firmware.elf")
    )
    _override_prog_path_for_fbuild(tmp_path, data, create_board("esp32dev"))

    assert data["esp32dev"]["prog_path"] == str(release_elf.resolve())


def test_picks_debug_when_only_debug_exists(tmp_path: Path) -> None:
    """`fbuild build --quick` writes debug/ — must be picked up (#2852)."""
    debug_elf = _touch(
        tmp_path / ".fbuild" / "build" / "esp32dev" / "debug" / "firmware.elf",
        mtime=2000.0,
    )
    data = _build_data(
        prog_path=str(tmp_path / ".pio" / "build" / "esp32dev" / "firmware.elf")
    )
    _override_prog_path_for_fbuild(tmp_path, data, create_board("esp32dev"))

    assert data["esp32dev"]["prog_path"] == str(debug_elf.resolve())


def test_picks_newer_when_both_exist(tmp_path: Path) -> None:
    """When both release/ and debug/ exist, pick the newer one (#2852)."""
    _touch(
        tmp_path / ".fbuild" / "build" / "esp32dev" / "release" / "firmware.elf",
        mtime=1000.0,
    )
    debug_elf = _touch(
        tmp_path / ".fbuild" / "build" / "esp32dev" / "debug" / "firmware.elf",
        mtime=2000.0,
    )
    data = _build_data(
        prog_path=str(tmp_path / ".pio" / "build" / "esp32dev" / "firmware.elf")
    )
    _override_prog_path_for_fbuild(tmp_path, data, create_board("esp32dev"))

    assert data["esp32dev"]["prog_path"] == str(debug_elf.resolve())


def test_no_op_when_fbuild_root_missing(tmp_path: Path) -> None:
    """Pure PlatformIO builds (no .fbuild/) must leave prog_path alone."""
    original = str(tmp_path / ".pio" / "build" / "esp32dev" / "firmware.elf")
    data = _build_data(prog_path=original)
    _override_prog_path_for_fbuild(tmp_path, data, create_board("esp32dev"))

    assert data["esp32dev"]["prog_path"] == original


def test_no_op_when_pio_elf_is_newer(tmp_path: Path) -> None:
    """If the user just ran `pio run` after an old fbuild, PIO wins."""
    _touch(
        tmp_path / ".fbuild" / "build" / "esp32dev" / "release" / "firmware.elf",
        mtime=1000.0,
    )
    pio_elf = _touch(
        tmp_path / ".pio" / "build" / "esp32dev" / "firmware.elf",
        mtime=2000.0,
    )
    data = _build_data(prog_path=str(pio_elf))
    _override_prog_path_for_fbuild(tmp_path, data, create_board("esp32dev"))

    assert data["esp32dev"]["prog_path"] == str(pio_elf)


def test_no_op_when_neither_fbuild_elf_exists(tmp_path: Path) -> None:
    """An empty .fbuild/build/<env>/ tree must not override prog_path."""
    (tmp_path / ".fbuild" / "build" / "esp32dev").mkdir(parents=True)
    original = str(tmp_path / ".pio" / "build" / "esp32dev" / "firmware.elf")
    data = _build_data(prog_path=original)
    _override_prog_path_for_fbuild(tmp_path, data, create_board("esp32dev"))

    assert data["esp32dev"]["prog_path"] == original
