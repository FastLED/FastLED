"""Report the flash footprint of an fbuild-compiled example.

Reads the `build_info[_<example>].json` that `bash compile <board>` writes
next to the fbuild outputs, runs the cross-toolchain `size` tool (from the
`aliases` block) on the firmware ELF, and prints text+data bytes. Falls
back to `prog_size` from the metadata, then to the `.bin`/`.uf2` file size.
"""

import argparse
import json
import re
from pathlib import Path
from typing import Any

from running_process import PIPE, RunningProcess

from ci.util.firmware_elf import find_fbuild_elf
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# Board build directory root used by `bash compile`; mirrors
# `ci/compiler/path_manager.py::FastLEDPaths.build_dir`.
BOARD_BUILD_SUBDIR = "fbuild"


def _create_board_info(path: Path) -> dict[str, Any]:
    build_info = json.loads(path.read_text())
    assert build_info.keys(), f"No boards found in {build_info}"
    assert len(build_info.keys()) == 1, (
        f"Multiple boards found in {build_info}, so correct board should be specified"
    )
    return build_info[next(iter(build_info))]


def _find_build_info(board: str, example: str | None = None) -> Path:
    """Find build_info.json for a board, with optional example-specific search.

    Args:
        board: Board name
        example: Optional example name to search for build_info_{example}.json first

    Returns:
        Path to build_info.json file

    Raises:
        FileNotFoundError: If no build_info file found
    """
    board_dirs = [
        Path(".build") / BOARD_BUILD_SUBDIR / board,
        Path(".build") / board,
    ]
    names: list[str] = []
    if example:
        names.append(f"build_info_{example}.json")
    names.append("build_info.json")

    candidates = [d / name for name in names for d in board_dirs]
    for candidate in candidates:
        if candidate.exists():
            return candidate

    tried = ", ".join(str(c) for c in candidates)
    raise FileNotFoundError(
        f"build_info.json not found for board '{board}' (tried {tried})"
    )


def _parse_size_tool_text(output: str) -> int | None:
    """Parse a `size` tool Berkeley-format header into text + data flash usage."""
    # Format: "   text	   data	    bss	    dec	    hex	filename"
    m = re.search(r"^\s*(\d+)\s+(\d+)\s+\d+\s+\d+\s+\w+\s+", output, re.MULTILINE)
    if m:
        return int(m.group(1)) + int(m.group(2))
    return None


def _run_size_on_elf(size_tool: Path, elf: Path) -> int | None:
    """Run the cross-toolchain `size` tool on an ELF and return text+data bytes."""
    try:
        result = RunningProcess.run(
            [str(size_tool), str(elf)],
            stdout=PIPE,
            stderr=PIPE,
            text=True,
            check=False,
            encoding="utf-8",
            errors="replace",
        )
    except (FileNotFoundError, OSError, RuntimeError):
        return None
    output = (result.stdout or "") + "\n" + (result.stderr or "")
    return _parse_size_tool_text(output)


def _find_size_tool(board_info: dict[str, Any]) -> Path | None:
    """Return the cross-toolchain size executable from build metadata.

    fbuild's build_info exposes tool paths under ``aliases`` (a value is
    ``null`` when the toolchain has no such tool). Some older metadata
    producers emitted a top-level ``size_path`` key, so retain that
    compatibility path first.
    """
    size_path = board_info.get("size_path")
    if isinstance(size_path, str) and size_path:
        return Path(size_path)

    aliases = board_info.get("aliases")
    if isinstance(aliases, dict):
        alias = aliases.get("size")
        if isinstance(alias, str) and alias:
            return Path(alias)
    return None


def check_firmware_size(board: str, example: str | None = None) -> int:
    build_info_json = _find_build_info(board, example)
    board_info = _create_board_info(build_info_json)
    assert board_info, f"Board {board} not found in {build_info_json}"

    build_dir = build_info_json.parent

    # PRIORITY 1: measure the fbuild ELF directly with the cross-toolchain
    # `size` tool. This is the binary the build produced, with fbuild's link
    # flags (`-Wl,--gc-sections`, `.eh_frame` stripping) applied.
    fbuild_elf = find_fbuild_elf(board_info, build_dir)
    size_tool = _find_size_tool(board_info)
    if fbuild_elf is not None and size_tool is not None:
        size = _run_size_on_elf(size_tool, fbuild_elf)
        if size is not None:
            print(
                f"[compiled_size] measured fbuild ELF: {fbuild_elf} "
                f"(text+data={size} B)"
            )
            return size
        print(
            f"[compiled_size] WARNING: found fbuild ELF {fbuild_elf} "
            f"but size tool {str(size_tool)!r} returned no parsable output; "
            f"falling through to prog_size."
        )
    elif fbuild_elf is None:
        print(
            f"[compiled_size] no fbuild ELF found under {build_dir / '.fbuild' / 'build'}; "
            f"falling through to prog_size."
        )
    else:
        print(
            "[compiled_size] build_info has no `size` alias for this toolchain; "
            "falling through to prog_size."
        )

    # PRIORITY 2: the size fbuild recorded when it linked the firmware.
    prog_size = board_info.get("prog_size")
    if isinstance(prog_size, int) and prog_size > 0:
        print(f"[compiled_size] using prog_size from {build_info_json}: {prog_size} B")
        return prog_size

    # PRIORITY 3: Fall back to .bin or .uf2 file size. Never .hex.
    prog_path_raw = board_info.get("prog_path")
    if not isinstance(prog_path_raw, str) or not prog_path_raw:
        raise FileNotFoundError(
            f"Unable to determine firmware size for {board}: build_info has "
            f"neither a usable `size` alias, `prog_size` nor `prog_path`."
        )
    base_path = Path(prog_path_raw).parent
    suffixes = [".bin", ".uf2"]
    for suffix in suffixes:
        candidate = base_path / f"firmware{suffix}"
        if candidate.exists():
            return candidate.stat().st_size

    raise FileNotFoundError(
        f"Unable to determine firmware size for {board}. "
        f"fbuild ELF probe and prog_size both failed and no "
        f".bin/.uf2 file found in {base_path}"
    )


def main(board: str, example: str | None = None) -> int:
    try:
        size = check_firmware_size(board, example)
        print(f"Firmware size for {board}: {size} bytes")
        return 0
    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1
    except json.JSONDecodeError:
        print(f"Error: Unable to parse build_info.json for {board}")
        return 1
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        return 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Check FastLED firmware size for the specified board."
    )
    parser.add_argument(
        "--board", type=str, required=True, help="Board to check firmware size for"
    )
    parser.add_argument(
        "--example", type=str, help="Example name (looks for build_info_{example}.json)"
    )
    args = parser.parse_args()

    raise SystemExit(main(args.board, args.example))
