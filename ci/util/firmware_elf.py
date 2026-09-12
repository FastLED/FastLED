"""Locate the firmware ELF a build actually produced.

Two build backends write to one board directory and they do not agree on where
the ELF goes. PlatformIO leaves it at `prog_path` from `build_info.json`
(`<build_dir>/.pio/build/<env>/firmware.elf`); fbuild writes under
`<build_dir>/.fbuild/build/...` and leaves `prog_path` pointing at the
PlatformIO location or at a `.bin`.

`ci/compiled_size.py` learned this and got it right. The binary-size
diagnostics did not: `ci/inspect_binary.py`, `ci/inspect_elf.py` and
`ci/util/symbol_analysis.py` each read `board_info["prog_path"]` directly, so
on an fbuild board every `readelf`/`nm`/`objdump` call ran against a path that
does not exist and exited 1. The symbol report came out as

    Found 0 total symbols using enhanced analysis
      Total symbol size: 0 bytes (0.0 KB)

and, because those steps run `continue-on-error: true`, nothing said so. The
esp32dev size gate has been red since FastLED#4387 with no usable symbol data
to find the regression with (FastLED#4402). `bash bloat` had the same defect
and it was fixed for that tool alone in FastLED#4386; this is the shared
version, so the next tool to need it does not reinvent it a fourth time.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any


def find_fbuild_elf(board_info: dict[str, Any], build_dir: Path) -> Path | None:
    """Locate the ELF that fbuild produced for this build, if any.

    Checks (in order):
      1. `prog_path` from `build_info.json` if it lives under a `.fbuild/`
         directory -- covers builds where the metadata already points there.
         Normalised to `.elf`: fbuild emits `prog_path` as `.bin`.
      2. A recursive glob under `<build_dir>/.fbuild/build/**/firmware.elf`,
         covering both fbuild layouts:
           - `<build_dir>/.fbuild/build/release/firmware.elf`
           - `<build_dir>/.fbuild/build/<env>/release/firmware.elf`

    Returns None when no fbuild artifact is present, which is the signal that
    the build was driven by PlatformIO and `prog_path` is authoritative.
    """

    prog_path_raw = board_info.get("prog_path")
    if isinstance(prog_path_raw, str) and prog_path_raw:
        prog_path = Path(prog_path_raw)
        if ".fbuild" in prog_path.parts:
            elf_candidate = prog_path.with_suffix(".elf")
            if elf_candidate.exists():
                return elf_candidate

    fbuild_root = build_dir / ".fbuild" / "build"
    if not fbuild_root.is_dir():
        return None

    candidates: list[Path] = []
    for candidate in fbuild_root.glob("**/firmware.elf"):
        if candidate.is_file():
            candidates.append(candidate)
    if not candidates:
        return None
    # Newest wins. Two layouts can coexist under one board directory, and a
    # fixed preference reports on whichever one is stale -- the rule
    # `ci/bloat.py` settled on in FastLED#4386.
    return max(candidates, key=lambda p: p.stat().st_mtime)


def resolve_firmware_elf(board_info: dict[str, Any], build_dir: Path) -> Path | None:
    """The ELF a diagnostic should read, fbuild artifact first.

    Falls back to `prog_path` so PlatformIO-driven builds keep working
    unchanged, and returns None when neither exists rather than handing back a
    path that every tool downstream will fail on one at a time.
    """

    fbuild_elf = find_fbuild_elf(board_info, build_dir)
    if fbuild_elf is not None:
        return fbuild_elf

    prog_path_raw = board_info.get("prog_path")
    if not isinstance(prog_path_raw, str) or not prog_path_raw:
        return None

    # The `.elf` sibling first when `prog_path` is not already one. Every
    # caller here runs `readelf`/`nm`/`objdump`, so an existing `.bin` next to
    # an existing `.elf` is the wrong answer even though the path resolves --
    # it would swap one silent empty report for another.
    prog_path = Path(prog_path_raw)
    candidates: list[Path] = []
    if prog_path.suffix != ".elf":
        candidates.append(prog_path.with_suffix(".elf"))
    candidates.append(prog_path)
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None
