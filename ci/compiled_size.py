import argparse
import json
import re
import subprocess
from pathlib import Path
from typing import Any

from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.pio_runner import run_pio_command


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
    candidates: list[Path] = []

    # If example provided, try example-specific files first
    if example:
        candidates.extend(
            [
                Path(".build") / "pio" / board / f"build_info_{example}.json",
                Path(".build") / board / f"build_info_{example}.json",
            ]
        )

    # Always try generic build_info.json as fallback
    candidates.extend(
        [
            Path(".build") / "pio" / board / "build_info.json",
            Path(".build") / board / "build_info.json",
        ]
    )

    for candidate in candidates:
        if candidate.exists():
            return candidate

    # Generate helpful error message
    if example:
        raise FileNotFoundError(
            f"build_info.json not found for board '{board}' "
            f"(tried build_info_{example}.json and build_info.json in .build/pio/{board} and .build/{board})"
        )
    else:
        raise FileNotFoundError(
            f"build_info.json not found for board '{board}' in .build/pio/{board} or .build/{board}"
        )


def _run_pio_size(build_dir: Path) -> int | None:
    try:
        # Try to compute size without building first
        # Uses run_pio_command for proper process tracking and atexit cleanup
        result = run_pio_command(
            ["pio", "run", "-d", str(build_dir), "-t", "size"],
            capture_output=True,
        )
        output = (result.stdout or "") + "\n" + (result.stderr or "")

        # Try AVR format first: "Program: XXXXX bytes"
        m = re.search(r"Program:\s*(\d+)\s*bytes", output)
        if m:
            return int(m.group(1))

        # Try ARM toolchain format: "text    data    bss    dec    hex filename"
        # Flash usage = text + data (initialized code and data in flash)
        m = re.search(r"^\s*(\d+)\s+(\d+)\s+\d+\s+\d+\s+\w+\s+", output, re.MULTILINE)
        if m:
            text = int(m.group(1))
            data = int(m.group(2))
            return text + data

        # Try teensy_size format (Teensy 4.x boards): "teensy_size:   FLASH: code:XXX, data:YYY, headers:ZZZ"
        # Flash usage = code + data + headers
        m = re.search(
            r"teensy_size:\s+FLASH:\s+code:(\d+),\s+data:(\d+),\s+headers:(\d+)", output
        )
        if m:
            code = int(m.group(1))
            data = int(m.group(2))
            headers = int(m.group(3))
            return code + data + headers

        # If size target did not yield, try a full build then retry size
        run_pio_command(
            ["pio", "run", "-d", str(build_dir)],
            capture_output=False,
        )
        result = run_pio_command(
            ["pio", "run", "-d", str(build_dir), "-t", "size"],
            capture_output=True,
        )
        output = (result.stdout or "") + "\n" + (result.stderr or "")

        # Try AVR format first: "Program: XXXXX bytes"
        m = re.search(r"Program:\s*(\d+)\s*bytes", output)
        if m:
            return int(m.group(1))

        # Try ARM toolchain format: "text    data    bss    dec    hex filename"
        # Flash usage = text + data (initialized code and data in flash)
        m = re.search(r"^\s*(\d+)\s+(\d+)\s+\d+\s+\d+\s+\w+\s+", output, re.MULTILINE)
        if m:
            text = int(m.group(1))
            data = int(m.group(2))
            return text + data

        # Try teensy_size format (Teensy 4.x boards): "teensy_size:   FLASH: code:XXX, data:YYY, headers:ZZZ"
        # Flash usage = code + data + headers
        m = re.search(
            r"teensy_size:\s+FLASH:\s+code:(\d+),\s+data:(\d+),\s+headers:(\d+)", output
        )
        if m:
            code = int(m.group(1))
            data = int(m.group(2))
            headers = int(m.group(3))
            return code + data + headers
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        pass
    return None


def _parse_size_tool_text(output: str) -> int | None:
    """Parse a `size` tool Berkeley-format header into text + data flash usage."""
    # Format: "   text	   data	    bss	    dec	    hex	filename"
    m = re.search(r"^\s*(\d+)\s+(\d+)\s+\d+\s+\d+\s+\w+\s+", output, re.MULTILINE)
    if m:
        return int(m.group(1)) + int(m.group(2))
    return None


def _run_size_on_elf(size_tool: Path, elf: Path) -> int | None:
    """Run the cross-toolchain `size` tool on an ELF and return text+data bytes.

    Used to read the fbuild-produced firmware directly, bypassing
    `pio run -t size` (which triggers a fresh PlatformIO compile and reports
    PIO's binary instead of fbuild's). PIO's binary on ESP32 carries
    `.eh_frame` and other libstdc++ machinery that fbuild's link strips,
    so the two diverge by ~169 KB on esp32dev — measuring the wrong one
    inflates the budget check (FastLED/FastLED#3258 fix).
    """
    try:
        result = subprocess.run(
            [str(size_tool), str(elf)],
            capture_output=True,
            text=True,
            check=False,
        )
    except (FileNotFoundError, OSError):
        return None
    output = (result.stdout or "") + "\n" + (result.stderr or "")
    return _parse_size_tool_text(output)


def _find_fbuild_elf(board_info: dict[str, Any], build_dir: Path) -> Path | None:
    """Locate the ELF that fbuild produced for this build, if any.

    Checks (in order):
      1. `prog_path` from `build_info.json` if it lives under a `.fbuild/`
         directory — covers builds where `_override_prog_path_for_fbuild`
         already rewrote the metadata (or where fbuild emitted the
         metadata itself).
      2. A recursive glob under `<build_dir>/.fbuild/build/**/firmware.elf`
         covering both fbuild layouts:
           - `<build_dir>/.fbuild/build/release/firmware.elf` (used by
             `bash compile <arm-board>` on the standalone driver path)
           - `<build_dir>/.fbuild/build/<env>/release/firmware.elf` (used
             by `PioCompiler._build_fbuild_sync` on the PIO-wrapper path
             that handles ESP32 boards) — see `ci/compiler/pio.py`
             `_artifacts_dir`.

    Returns the resolved ELF path, or None if no fbuild artifact is present
    (i.e. the build was driven by PlatformIO and the old `_run_pio_size`
    path should win).
    """
    prog_path_raw = board_info.get("prog_path")
    if isinstance(prog_path_raw, str) and prog_path_raw:
        prog_path = Path(prog_path_raw)
        if ".fbuild" in prog_path.parts:
            # prog_path may end in .bin (what fbuild emits as `prog_path`)
            # or .elf (what `_override_prog_path_for_fbuild` writes). The
            # size tool only takes ELF, so normalise to .elf.
            elf_candidate = prog_path.with_suffix(".elf")
            if elf_candidate.exists():
                return elf_candidate

    fbuild_root = build_dir / ".fbuild" / "build"
    if not fbuild_root.is_dir():
        return None

    # Glob both `release/firmware.elf` and `<env>/release/firmware.elf` plus
    # their `debug/` siblings. Picking the newest mtime handles the case
    # where a `--release` build was followed by `--quick`.
    candidates: list[Path] = list(fbuild_root.glob("**/firmware.elf"))
    if not candidates:
        return None
    return max(candidates, key=lambda p: p.stat().st_mtime)


def check_firmware_size(board: str, example: str | None = None) -> int:
    build_info_json = _find_build_info(board, example)
    board_info = _create_board_info(build_info_json)
    assert board_info, f"Board {board} not found in {build_info_json}"

    build_dir = build_info_json.parent

    # PRIORITY 1: When fbuild produced the binary, measure IT directly via
    # the cross-toolchain `size` tool — `pio run -t size` would (a) trigger
    # a fresh PlatformIO compile that bypasses fbuild's tighter link flags
    # (e.g. `-Wl,--gc-sections`, `.eh_frame` stripping) and (b) report the
    # PIO binary which is ~169 KB larger on esp32dev. Without this branch
    # the CI size-check measures the wrong build.
    fbuild_elf = _find_fbuild_elf(board_info, build_dir)
    size_tool = board_info.get("size_path")
    if fbuild_elf is not None and isinstance(size_tool, str) and size_tool:
        size = _run_size_on_elf(Path(size_tool), fbuild_elf)
        if size is not None:
            print(
                f"[compiled_size] measured fbuild ELF: {fbuild_elf} "
                f"(text+data={size} B)"
            )
            return size
        print(
            f"[compiled_size] WARNING: found fbuild ELF {fbuild_elf} "
            f"but size tool {size_tool!r} returned no parsable output; "
            f"falling through to pio size."
        )
    elif fbuild_elf is None:
        print(
            f"[compiled_size] no fbuild ELF found under {build_dir / '.fbuild' / 'build'}; "
            f"falling through to pio size."
        )

    # PRIORITY 2: PlatformIO's size command. This was previously priority 1;
    # AVR boards still need it because `.hex` file sizes are ASCII-inflated
    # and the toolchain `size` tool path isn't always in build_info.json.
    size = _run_pio_size(build_dir)
    if size is not None:
        return size

    # PRIORITY 3: Fall back to .bin or .uf2 file size. Never .hex.
    prog_path = Path(board_info["prog_path"])
    base_path = prog_path.parent
    suffixes = [".bin", ".uf2"]
    for suffix in suffixes:
        candidate = base_path / f"firmware{suffix}"
        if candidate.exists():
            return candidate.stat().st_size

    raise FileNotFoundError(
        f"Unable to determine firmware size for {board}. "
        f"fbuild ELF probe + PlatformIO size command both failed and no "
        f".bin/.uf2 file found in {base_path}"
    )


def main(board: str, example: str | None = None):
    try:
        size = check_firmware_size(board, example)
        print(f"Firmware size for {board}: {size} bytes")
    except FileNotFoundError as e:
        print(f"Error: {e}")
    except json.JSONDecodeError:
        print(f"Error: Unable to parse build_info.json for {board}")
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"An unexpected error occurred: {e}")


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

    main(args.board, args.example)
