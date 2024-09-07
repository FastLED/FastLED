"""
Runs the compilation process for all examples on all boards in parallel.
Build artifacts are recycled within a board group so that subsequent ino
files are built faster.
"""

import argparse
import sys
import time
import warnings
from pathlib import Path

from ci.boards import BOARDS, OTHER_BOARDS
from ci.concurrent_run import ConcurrentRunArgs, concurrent_run
from ci.examples import EXAMPLES
from ci.locked_print import locked_print
from ci.project import Project, get_project

HERE = Path(__file__).parent.resolve()


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compile FastLED examples for various boards."
    )
    parser.add_argument(
        "--boards", type=str, help="Comma-separated list of boards to compile for"
    )
    parser.add_argument(
        "--examples", type=str, help="Comma-separated list of examples to compile"
    )
    parser.add_argument(
        "--skip-init", action="store_true", help="Skip the initialization step"
    )
    parser.add_argument(
        "--defines", type=str, help="Comma-separated list of compiler definitions"
    )
    parser.add_argument(
        "--extra-packages",
        type=str,
        help="Comma-separated list of extra packages to install",
    )
    parser.add_argument(
        "--build-dir", type=str, help="Override the default build directory"
    )
    parser.add_argument(
        "--no-project-options",
        action="store_true",
        help="Don't use custom project options",
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Enable interactive mode to choose a board",
    )
    # Passed by the github action to disable interactive mode.
    parser.add_argument(
        "--no-interactive", action="store_true", help="Disable interactive mode"
    )
    args = parser.parse_args()
    # if --interactive and --no-interative are both passed, --no-interactive takes precedence.
    if args.interactive and args.no_interactive:
        warnings.warn(
            "Both --interactive and --no-interactive were passed, --no-interactive takes precedence."
        )
        args.interactive = False
    return args


def remove_duplicates(items: list[str]) -> list[str]:
    seen = set()
    out = []
    for item in items:
        if item not in seen:
            seen.add(item)
            out.append(item)
    return out


def choose_board_interactively(boards: list[str]) -> list[str]:
    print("Available boards:")
    boards = remove_duplicates(sorted(boards))
    for i, board in enumerate(boards):
        print(f"[{i}]: {board}")
    print("[all]: All boards")
    out: list[str] = []
    while True:
        try:
            # choice = int(input("Enter the number of the board(s) you want to compile to: "))
            input_str = input(
                "Enter the number of the board(s) you want to compile to, or it's name(s): "
            )
            if "all" in input_str:
                return boards
            for board in input_str.split(","):
                if board == "":
                    continue
                if not board.isdigit():
                    out.append(board)  # Assume it's a board name.
                else:
                    index = int(board)  # Find the board from the index.
                    if 0 <= index < len(boards):
                        out.append(boards[index])
                    else:
                        warnings.warn(f"invalid board index: {index}, skipping")
            if not out:
                print("Please try again.")
                continue
            return out
        except ValueError:
            print("Invalid input. Please enter a number.")


def create_concurrent_run_args(args: argparse.Namespace) -> ConcurrentRunArgs:
    skip_init = args.skip_init
    if args.interactive:
        boards = choose_board_interactively(BOARDS + OTHER_BOARDS)
    else:
        boards = args.boards.split(",") if args.boards else BOARDS
    projects: list[Project] = []
    for board in boards:
        projects.append(get_project(board, no_project_options=args.no_project_options))
    examples = args.examples.split(",") if args.examples else EXAMPLES
    defines: list[str] = []
    if args.defines:
        defines.extend(args.defines.split(","))
    extra_packages: list[str] = []
    if args.extra_packages:
        extra_packages.extend(args.extra_packages.split(","))
    build_dir = args.build_dir
    extra_scripts = "pre:lib/ci/ci-flags.py"
    out: ConcurrentRunArgs = ConcurrentRunArgs(
        projects=projects,
        examples=examples,
        skip_init=skip_init,
        defines=defines,
        extra_packages=extra_packages,
        build_dir=build_dir,
        extra_scripts=extra_scripts,
        cwd=str(HERE.parent),
        board_dir=(HERE / "boards").absolute().as_posix(),
    )
    return out


def main() -> int:
    """Main function."""
    args = parse_args()
    # Set the working directory to the script's parent directory.
    run_args = create_concurrent_run_args(args)
    start_time = time.time()
    rtn = concurrent_run(args=run_args)
    time_taken = time.strftime("%Mm:%Ss", time.gmtime(time.time() - start_time))
    locked_print(f"Compilation finished in {time_taken}.")
    return rtn


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(1)
