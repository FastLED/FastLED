"""Tests for the fbuild-aware path inside `ci/compiled_size.py`.

The size-check CI workflow drives `ci/compiled_size.py` to read the
firmware size that the just-finished `ci.ci-compile` invocation produced.
fbuild is the only backend; a historical fallback used to re-run a
different compile and report a binary linked without fbuild's `.eh_frame`
stripping, inflating the reported size by ~169 KB on esp32dev. These tests
pin the priority order (ELF via the `size` alias first) so a future
refactor can't silently break it again.
"""

from pathlib import Path
from typing import Any

import pytest

from ci.compiled_size import (
    _find_size_tool,
    _parse_size_tool_text,
    main,
)
from ci.util.firmware_elf import find_fbuild_elf as _find_fbuild_elf


def test_parse_size_tool_text_berkeley_format() -> None:
    """The standard `size` (Berkeley) output format must give text + data."""
    output = (
        "   text\t   data\t    bss\t    dec\t    hex\tfilename\n"
        "  46516\t    288\t   9832\t  56636\t   dd3c\tfirmware.elf\n"
    )
    assert _parse_size_tool_text(output) == 46516 + 288


def test_parse_size_tool_text_handles_empty_output() -> None:
    """Malformed / empty size output must not raise; it returns None."""
    assert _parse_size_tool_text("") is None
    assert _parse_size_tool_text("error: no input file\n") is None


def test_find_size_tool_uses_aliases_block() -> None:
    board_info = {"aliases": {"size": "/toolchain/bin/xtensa-size"}}
    assert _find_size_tool(board_info) == Path("/toolchain/bin/xtensa-size")


def test_find_size_tool_prefers_legacy_top_level_key() -> None:
    board_info = {
        "size_path": "/legacy/size",
        "aliases": {"size": "/aliases/size"},
    }
    assert _find_size_tool(board_info) == Path("/legacy/size")


def _make_fake_elf(p: Path) -> Path:
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(b"\x7fELF")  # minimum-ish ELF header so `.exists()` is true
    return p


def test_find_fbuild_elf_prefers_prog_path_when_under_fbuild(tmp_path: Path) -> None:
    """When build_info.json's prog_path lives under .fbuild/, that is the
    ELF the override (or fbuild's own metadata emitter) chose — honour it
    even when the file ends in .bin (fbuild) or .elf (override).
    """
    fbuild_dir = tmp_path / ".fbuild" / "build" / "release"
    elf = _make_fake_elf(fbuild_dir / "firmware.elf")
    _make_fake_elf(fbuild_dir / "firmware.bin")  # the .bin sibling

    board_info: dict[str, Any] = {
        "prog_path": str(fbuild_dir / "firmware.bin"),  # what fbuild writes
        "lpc845brk": {},
    }
    assert _find_fbuild_elf(board_info, tmp_path) == elf


def test_find_fbuild_elf_probes_arm_layout_without_env_segment(
    tmp_path: Path,
) -> None:
    """`bash compile <arm-board>` path: `<build_dir>/.fbuild/build/release/firmware.elf`."""
    fbuild_dir = tmp_path / ".fbuild" / "build" / "release"
    elf = _make_fake_elf(fbuild_dir / "firmware.elf")

    board_info = {"prog_path": "stale/build/lpc/firmware.elf"}
    assert _find_fbuild_elf(board_info, tmp_path) == elf


def test_find_fbuild_elf_probes_layout_with_env_segment(
    tmp_path: Path,
) -> None:
    """ESP32 / Arduino path: the ELF lands at
    `<build_dir>/.fbuild/build/<env>/release/firmware.elf`. This is the
    layout that broke the first attempt at this fix when the probe only
    looked at `<build_dir>/.fbuild/build/release/firmware.elf`.
    """
    fbuild_dir = tmp_path / ".fbuild" / "build" / "esp32dev" / "release"
    elf = _make_fake_elf(fbuild_dir / "firmware.elf")

    board_info = {"prog_path": "stale/build/esp32dev/firmware.elf"}
    assert _find_fbuild_elf(board_info, tmp_path) == elf


def test_find_fbuild_elf_returns_none_when_no_fbuild_artifact(tmp_path: Path) -> None:
    """No `.fbuild/` tree at all — return None so the caller falls through
    to `prog_size`.
    """
    board_info = {"prog_path": str(tmp_path / "build" / "x" / "firmware.elf")}
    assert _find_fbuild_elf(board_info, tmp_path) is None


def test_find_fbuild_elf_picks_newer_of_release_and_debug(tmp_path: Path) -> None:
    """fbuild --quick lands in debug/, fbuild --release in release/. When
    both exist we measure whichever the user just produced (newer mtime).
    """
    fbuild_root = tmp_path / ".fbuild" / "build"
    release_elf = _make_fake_elf(fbuild_root / "release" / "firmware.elf")
    debug_elf = _make_fake_elf(fbuild_root / "debug" / "firmware.elf")

    # Make debug newer than release.
    import os
    import time

    older = time.time() - 60
    os.utime(release_elf, (older, older))

    board_info = {"prog_path": "irrelevant"}
    assert _find_fbuild_elf(board_info, tmp_path) == debug_elf


def test_main_fails_when_board_metadata_is_missing(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    monkeypatch.chdir(tmp_path)

    assert main("missing-board") != 0
    assert (
        "build_info.json not found for board 'missing-board'" in capsys.readouterr().out
    )


def test_main_fails_when_board_metadata_is_malformed(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    build_dir = tmp_path / ".build" / "fbuild" / "broken-board"
    build_dir.mkdir(parents=True)
    (build_dir / "build_info.json").write_text("{not json", encoding="utf-8")
    monkeypatch.chdir(tmp_path)

    assert main("broken-board") != 0
    assert "Unable to parse build_info.json for broken-board" in capsys.readouterr().out


def test_main_reports_successful_measurement(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    monkeypatch.setattr(
        "ci.compiled_size.check_firmware_size", lambda board, example: 42
    )

    assert main("working-board") == 0
    assert "Firmware size for working-board: 42 bytes" in capsys.readouterr().out


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
