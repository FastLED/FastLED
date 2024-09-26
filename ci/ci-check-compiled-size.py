import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent


def run_command(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stdout.strip()


def parse_args():
    parser = argparse.ArgumentParser(
        description="Check compiled program size for a board"
    )
    parser.add_argument("board", help="Board name")
    parser.add_argument(
        "--max-size", type=int, required=False, help="Maximum allowed size"
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

    output = run_command(["uv", "run", "ci/compiled_size.py", "--board", args.board])
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
