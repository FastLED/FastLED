import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent


def run_command(cmd, shell: bool = False, check=None, capture_output: bool = False):
    check = check if check is not None else check

    result = subprocess.run(
        cmd, capture_output=capture_output, text=True, shell=shell, check=check
    )
    return result.stdout.strip() if capture_output else None


def parse_args():
    parser = argparse.ArgumentParser(
        description="Check compiled program size for a board"
    )
    parser.add_argument("board", help="Board name")
    parser.add_argument(
        "--max-size", type=int, required=False, help="Maximum allowed size"
    )
    parser.add_argument(
        "--no-build",
        action="store_true",
        help="Skip compilation and check existing build",
    )
    args, unknown = parser.parse_known_args()

    if unknown:
        print("Ignoring unknown arguments:")
        for arg in unknown:
            print(f"  {arg}")

    return args


def main():
    os.chdir(str(PROJECT_ROOT))
    args = parse_args()

    if not args.no_build:
        cmd_list = ["uv", "run", "ci/ci-compile.py", args.board, "--examples", "Blink"]
        run_command(cmd_list, shell=True, capture_output=True)

    output = run_command(
        ["uv", "run", "ci/compiled_size.py", "--board", args.board],
        capture_output=True,
    )
    size_match = re.search(r": *(\d+)", output)

    if not size_match:
        print("Error: Unable to extract size from output")
        print(f"Output: {output}")
        sys.exit(1)

    size = int(size_match.group(1))

    if args.max_size is not None:
        max_size = args.max_size
        if size > max_size:
            print(f"{args.board} size {size} is greater than max size {max_size}")
            print("::error::Compiled size exceeds maximum allowed size")
            sys.exit(1)
        else:
            print(f"{args.board} size {size} is within the limit of {max_size}")
    else:
        print(f"{args.board} size: {size}")


if __name__ == "__main__":
    main()
