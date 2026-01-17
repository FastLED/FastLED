import argparse
import json
import re
from pathlib import Path
from typing import Any

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
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
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        pass
    return None


def check_firmware_size(board: str, example: str | None = None) -> int:
    build_info_json = _find_build_info(board, example)
    board_info = _create_board_info(build_info_json)
    assert board_info, f"Board {board} not found in {build_info_json}"

    # PRIORITY 1: Use PlatformIO's size command for accurate flash usage
    # This is essential for AVR boards where .hex files are ASCII format
    # and their file size is much larger than actual flash usage
    build_dir = build_info_json.parent
    size = _run_pio_size(build_dir)
    if size is not None:
        return size

    # PRIORITY 2: Only use .bin or .uf2 files (which have accurate file sizes)
    # DO NOT use .hex files - they're ASCII Intel HEX format
    prog_path = Path(board_info["prog_path"])
    base_path = prog_path.parent
    suffixes = [".bin", ".uf2"]
    for suffix in suffixes:
        candidate = base_path / f"firmware{suffix}"
        if candidate.exists():
            return candidate.stat().st_size

    # Unable to determine size accurately
    raise FileNotFoundError(
        f"Unable to determine firmware size for {board}. "
        f"PlatformIO size command failed and no .bin/.uf2 file found in {base_path}"
    )


def main(board: str, example: str | None = None):
    try:
        size = check_firmware_size(board, example)
        print(f"Firmware size for {board}: {size} bytes")
    except FileNotFoundError as e:
        print(f"Error: {e}")
    except json.JSONDecodeError:
        print(f"Error: Unable to parse build_info.json for {board}")
        handle_keyboard_interrupt_properly()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
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
