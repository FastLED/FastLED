import argparse
import json
import re
import subprocess
from pathlib import Path
from typing import Any, Dict


def _create_board_info(path: Path) -> Dict[str, Any]:
    build_info = json.loads(path.read_text())
    assert build_info.keys(), f"No boards found in {build_info}"
    assert len(build_info.keys()) == 1, (
        f"Multiple boards found in {build_info}, so correct board should be specified"
    )
    return build_info[next(iter(build_info))]


def _find_build_info(board: str) -> Path:
    # Prefer PIO layout
    candidate = Path(".build") / "pio" / board / "build_info.json"
    if candidate.exists():
        return candidate
    # Fallback legacy layout
    candidate = Path(".build") / board / "build_info.json"
    if candidate.exists():
        return candidate
    raise FileNotFoundError(
        f"build_info.json not found for board '{board}' in .build/pio/{board} or .build/{board}"
    )


def _run_pio_size(build_dir: Path) -> int | None:
    try:
        # Try to compute size without building first
        result = subprocess.run(
            ["pio", "run", "-d", str(build_dir), "-t", "size"],
            capture_output=True,
            text=True,
            check=False,
        )
        output = (result.stdout or "") + "\n" + (result.stderr or "")
        m = re.search(r"Program:\s*(\d+)\s*bytes", output)
        if m:
            return int(m.group(1))
        # If size target did not yield, try a full build then retry size
        subprocess.run(
            ["pio", "run", "-d", str(build_dir)], capture_output=False, check=False
        )
        result = subprocess.run(
            ["pio", "run", "-d", str(build_dir), "-t", "size"],
            capture_output=True,
            text=True,
            check=False,
        )
        output = (result.stdout or "") + "\n" + (result.stderr or "")
        m = re.search(r"Program:\s*(\d+)\s*bytes", output)
        if m:
            return int(m.group(1))
    except Exception:
        pass
    return None


def check_firmware_size(board: str) -> int:
    build_info_json = _find_build_info(board)
    board_info = _create_board_info(build_info_json)
    assert board_info, f"Board {board} not found in {build_info_json}"
    prog_path = Path(board_info["prog_path"])  # absolute or relative path
    base_path = prog_path.parent
    suffixes = [".bin", ".hex", ".uf2"]
    firmware: Path | None = None
    for suffix in suffixes:
        candidate = base_path / f"firmware{suffix}"
        if candidate.exists():
            firmware = candidate
            break
    if firmware is None:
        # As a fallback, if firmware.* doesn't exist, use ELF size (filesize) if present
        if prog_path.exists():
            return prog_path.stat().st_size
        # As a last resort, run PlatformIO size target in the build directory
        build_dir = build_info_json.parent
        size = _run_pio_size(build_dir)
        if size is not None:
            return size
        msg = (
            ", ".join([f"firmware{suffix}" for suffix in suffixes])
            + f" not found in {base_path}"
        )
        raise FileNotFoundError(msg)
    return firmware.stat().st_size


def main(board: str):
    try:
        size = check_firmware_size(board)
        print(f"Firmware size for {board}: {size} bytes")
    except FileNotFoundError as e:
        print(f"Error: {e}")
    except json.JSONDecodeError:
        print(f"Error: Unable to parse build_info.json for {board}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Check FastLED firmware size for the specified board."
    )
    parser.add_argument(
        "--board", type=str, required=True, help="Board to check firmware size for"
    )
    args = parser.parse_args()

    main(args.board)
