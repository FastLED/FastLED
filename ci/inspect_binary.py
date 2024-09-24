import argparse
import json
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent


def cpp_filt(cpp_filt_path: str | Path, stdout: str) -> str:
    p = Path(cpp_filt_path)
    if not p.exists():
        raise FileNotFoundError(f"cppfilt not found at '{p}'")
    command = [str(p), "-t"]
    print(f"Running command: {' '.join(command)}")
    result = subprocess.run(
        command,
        input=stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Error running command: {result.stderr}")
    return result.stdout


def dump_symbols(firmware_path: Path, objdump_path: Path) -> str:
    command = f"{objdump_path} -t {firmware_path}"
    result = subprocess.run(
        command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Error running command: {result.stderr}")
    return result.stdout


def dump_sections_size(firmware_path: Path, size_path: Path) -> str:
    command = f"{size_path} {firmware_path}"
    result = subprocess.run(
        command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Error running command: {result.stderr}")
    return result.stdout


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect a compiled binary")
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root_build_dir = Path(".build")

    # Find the first board directory
    board_dirs = [d for d in root_build_dir.iterdir() if d.is_dir()]
    if not board_dirs:
        print("No board directories found in .build")
        return 1

    # display all the boards to the user and ask them to select which one they want by number
    print("Available boards:")
    for i, board_dir in enumerate(board_dirs):
        print(f"[{i}]: {board_dir.name}")

    if args.first:
        which = 0
    else:
        which = int(input("Enter the number of the board you want to inspect: "))

    board_dir = board_dirs[which]
    board = board_dir.name

    build_info_json = board_dir / "build_info.json"
    build_info = json.loads(build_info_json.read_text())
    board_info = build_info[board]

    firmware_path = Path(board_info["prog_path"])
    objdump_path = Path(board_info["aliases"]["objdump"])
    cpp_filt_path = Path(board_info["aliases"]["c++filt"])

    print(f"Dumping sections size for {board} firmware: {firmware_path}")
    try:
        size_path = Path(board_info["aliases"]["size"])
        sections_size = dump_sections_size(firmware_path, size_path)
        print(sections_size)
    except Exception as e:
        print(f"Error while dumping sections size: {e}")

    print(f"Dumping symbols for {board} firmware: {firmware_path}")
    try:
        symbols = dump_symbols(firmware_path, objdump_path)
        symbols = cpp_filt(cpp_filt_path, symbols)
        print(symbols)
    except Exception as e:
        print(f"Error while dumping symbols: {e}")

    print(f"Dumping map file for {board} firmware: {firmware_path}")
    map_path = board_dir / "firmware.map"
    if map_path.exists():
        print(map_path.read_text())
    else:
        print(f"Map file not found at {map_path}")

    return 0


if __name__ == "__main__":
    main()
