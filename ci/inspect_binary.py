import argparse
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent


def cpp_filt(cpp_filt_path: str | Path, input_text: str) -> str:
    p = Path(cpp_filt_path)
    if not p.exists():
        raise FileNotFoundError(f"cppfilt not found at '{p}'")
    command = [str(p), "-t", "-n"]
    print(f"Running command: {' '.join(command)}")
    process = subprocess.Popen(
        command,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    stdout, stderr = process.communicate(input=input_text)
    if process.returncode != 0:
        raise RuntimeError(f"Error running command: {stderr}")
    return stdout


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect a compiled binary")
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    parser.add_argument("--cwd", type=str, help="Custom working directory")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.cwd:
        root_build_dir = Path(args.cwd) / ".build"
    else:
        root_build_dir = Path(".build")

    board_dirs = [d for d in root_build_dir.iterdir() if d.is_dir()]
    if not board_dirs:
        print(f"No board directories found in {root_build_dir.absolute()}")
        return 1

    print("Available boards:")
    for i, board_dir in enumerate(board_dirs):
        print(f"[{i}]: {board_dir.name}")

    which = (
        0
        if args.first
        else int(input("Enter the number of the board you want to inspect: "))
    )

    board_dir = board_dirs[which]
    board = board_dir.name

    build_info_json = board_dir / "build_info.json"
    build_info = json.loads(build_info_json.read_text())
    board_info = build_info.get(board) or build_info[next(iter(build_info))]
    elf_path = Path(board_info["prog_path"])
    # binary_path = _find_binary_path(elf_path)
    cpp_filt_path = Path(board_info["aliases"]["c++filt"])
    map_path = board_dir / "firmware.map"
    if not map_path.exists():
        map_path = elf_path.with_suffix(".map")
    # print("\n\nMap file dump:\n")
    print("\n##################################################")
    print("# Map file dump:")
    print("##################################################\n")
    text = map_path.read_text()
    dmangled_text = cpp_filt(cpp_filt_path, text)
    print(dmangled_text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
