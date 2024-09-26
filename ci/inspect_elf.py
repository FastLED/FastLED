import argparse
import json
from pathlib import Path

from ci.elf import dump_symbol_sizes

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect a compiled binary")
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    parser.add_argument("--cwd", type=str, help="Custom working directory")
    parser.add_argument("--elf", type=str, help="Path to the ELF file to inspect")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.elf:
        firmware_path = Path(args.elf)
        if not firmware_path.exists():
            print(f"ELF file not found: {firmware_path}")
            return 1

    if args.cwd:
        # os.chdir(args.cwd)
        root_build_dir = Path(args.cwd) / ".build"
    else:
        root_build_dir = Path(".build")

    # Find the first board directory
    board_dirs = [d for d in root_build_dir.iterdir() if d.is_dir()]
    if not board_dirs:
        # print("No board directories found in .build")
        print(f"No board directories found in {root_build_dir.absolute()}")
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
    board_info = build_info.get(board) or build_info[next(iter(build_info))]

    firmware_path = Path(board_info["prog_path"])
    cpp_filt_path = Path(board_info["aliases"]["c++filt"])

    print(f"Dumping symbol sizes for {board} firmware: {firmware_path}")
    try:
        nm_path = Path(board_info["aliases"]["nm"])
        symbol_sizes = dump_symbol_sizes(nm_path, cpp_filt_path, firmware_path)
        print(symbol_sizes)
    except Exception as e:
        print(f"Error while dumping symbol sizes: {e}")

    return 0


if __name__ == "__main__":
    main()
