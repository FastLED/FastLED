import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent

IS_GITHUB = "GITHUB_ACTIONS" in os.environ


def run_command(cmd, shell: bool = False, check=None, capture_output: bool = False):
    check = check if check is not None else check
    if shell:
        cmd = subprocess.list2cmdline(cmd)

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
    parser.add_argument(
        "--example",
        default="Blink",
        help="Example to compile (default: Blink)",
    )

    # Parse known args first
    args, unknown = parser.parse_known_args()

    # Add remaining arguments as extra_args
    args.extra_args = unknown

    return args


def main():
    os.chdir(str(PROJECT_ROOT))
    args = parse_args()

    if not args.no_build:
        cmd_list = [
            "uv",
            "run",
            "ci/ci-compile.py",
            args.board,
            "--examples",
            args.example,
        ] + args.extra_args
        try:
            run_command(cmd_list, shell=True, capture_output=IS_GITHUB, check=True)
        except subprocess.CalledProcessError:
            run_command(cmd_list, shell=True, capture_output=False, check=True)

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

    if args.max_size is not None and args.max_size > 0:
        max_size = args.max_size
        if size > max_size:
            print(f"{args.board} size {size} is greater than max size {max_size}")
            print("::error::Compiled size exceeds maximum allowed size")
            sys.exit(1)
        else:
            print(f"{args.board} size {size} is within the limit of {max_size}")
    else:
        if not args.max_size:
            print("Warning: No max size specified")
        elif args.max_size <= 0:
            print("Warning: max size was <= 0 so no check was performed")
        print(f"{args.board} size: {size}")


if __name__ == "__main__":
    main()
