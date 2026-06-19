"""Tests for the fbuild-aware path inside `ci/compiled_size.py`.

The size-check CI workflow drives `ci/compiled_size.py` to read the
firmware size that the just-finished `ci.ci-compile` invocation produced.
fbuild is the default backend, but the historical `_run_pio_size` priority
caused the size measurement to fall through to a *fresh* PlatformIO
compile — which re-links without fbuild's `.eh_frame` stripping and
inflates the reported size by ~169 KB on esp32dev. These tests pin the
new priority order so a future refactor can't silently break it again.
"""

from pathlib import Path
from typing import Any

import pytest

from ci.compiled_size import (
    _find_fbuild_elf,
    _parse_size_tool_text,
)


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


def test_find_fbuild_elf_probes_modern_layout_without_env_segment(
    tmp_path: Path,
) -> None:
    """Current fbuild layout: `<build_dir>/.fbuild/build/release/firmware.elf`.

    Older fbuild releases used `<env>/release/firmware.elf`. The probe
    must accept the modern layout so the override and downstream size
    measurement keep working after fbuild's path simplification.
    """
    fbuild_dir = tmp_path / ".fbuild" / "build" / "release"
    elf = _make_fake_elf(fbuild_dir / "firmware.elf")

    board_info = {"prog_path": "stale.pio/build/lpc/firmware.elf"}
    # board_info has no .fbuild prog_path AND no env_name keyed entry,
    # so the function should still find the standalone fbuild ELF.
    assert _find_fbuild_elf(board_info, tmp_path) == elf


def test_find_fbuild_elf_returns_none_when_no_fbuild_artifact(tmp_path: Path) -> None:
    """A pure PlatformIO build leaves `.fbuild/` absent — return None so the
    caller falls through to `_run_pio_size`.
    """
    board_info = {"prog_path": str(tmp_path / ".pio" / "build" / "x" / "firmware.elf")}
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


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
