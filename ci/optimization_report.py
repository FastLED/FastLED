import argparse
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert a binary file to ELF using map file."
    )
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    parser.add_argument("--cwd", type=Path, help="Custom working directory")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.cwd:
        root_build_dir = args.cwd / ".build"
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
    # build_info_json = board_dir / "build_info.json"
    optimization_report = board_dir / "optimization_report.txt"
    text = optimization_report.read_text(encoding="utf-8")
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
