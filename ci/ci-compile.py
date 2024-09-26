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

from ci.boards import Board, get_board
from ci.concurrent_run import ConcurrentRunArgs, concurrent_run
from ci.locked_print import locked_print

HERE = Path(__file__).parent.resolve()

LIBS = ["src", "ci"]

# Default boards to compile for. You can use boards not defined here but
# if the board isn't part of the officially supported platformio boards then
# you will need to add the board to the ~/.platformio/platforms directory.
# prior to running this script. This happens automatically as of 2024-08-20
# with the github workflow scripts.
DEFAULT_BOARDS_NAMES = [
    "uno",  # Build is faster if this is first, because it's used for global init.
    "esp32dev",
    "esp01",  # ESP8266
    "esp32-c3-devkitm-1",
    "attiny85",
    "ATtiny1616",
    "esp32-c6-devkitc-1",
    "esp32-s3-devkitc-1",
    "yun",
    "digix",
    "teensy30",
    "teensy41",
    "adafruit_feather_nrf52840_sense",
    "xiaoblesense_adafruit",
    "rpipico",
    "rpipico2",
    "uno_r4_wifi",
    "esp32dev_i2s",
    "esp32rmt_51",
    "esp32dev_idf44",
    "bluepill",
]

OTHER_BOARDS_NAMES = [
    "nano_every",
    "esp32-c2-devkitm-1",
]

# Examples to compile.
DEFAULT_EXAMPLES = [
    "Apa102HD",
    "Apa102HDOverride",
    "Blink",
    "ColorPalette",
    "ColorTemperature",
    "Cylon",
    "DemoReel100",
    "Fire2012",
    "FirstLight",
    "Multiple/MultipleStripsInOneArray",
    "Multiple/ArrayOfLedArrays",
    "Noise",
    "NoisePlayground",
    "NoisePlusPalette",
    "Pacifica",
    "Pride2015",
    "RGBCalibrate",
    "RGBSetDemo",
    "RGBW",
    "RGBWEmulated",
    "TwinkleFox",
    "XYMatrix",
]


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compile FastLED examples for various boards."
    )
    # parser.add_argument(
    #     "--boards", type=str, help="Comma-separated list of boards to compile for"
    # )
    # needs to be a positional argument instead
    parser.add_argument(
        "boards",
        type=str,
        help="Comma-separated list of boards to compile for",
        nargs="?",
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
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose output"
    )
    parser.add_argument(
        "--supported-boards",
        action="store_true",
        help="Print the list of supported boards and exit",
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


def resolve_example_path(example: str) -> Path:
    example_path = HERE.parent / "examples" / example
    if not example_path.exists():
        raise FileNotFoundError(f"Example '{example}' not found at '{example_path}'")
    return example_path


def create_concurrent_run_args(args: argparse.Namespace) -> ConcurrentRunArgs:
    skip_init = args.skip_init
    if args.interactive:
        boards = choose_board_interactively(DEFAULT_BOARDS_NAMES + OTHER_BOARDS_NAMES)
    else:
        boards = args.boards.split(",") if args.boards else DEFAULT_BOARDS_NAMES
    projects: list[Board] = []
    for board in boards:
        projects.append(get_board(board, no_project_options=args.no_project_options))
    examples = args.examples.split(",") if args.examples else DEFAULT_EXAMPLES
    examples_paths = [resolve_example_path(example) for example in examples]
    defines: list[str] = []
    if args.defines:
        defines.extend(args.defines.split(","))
    extra_packages: list[str] = []
    if args.extra_packages:
        extra_packages.extend(args.extra_packages.split(","))
    build_dir = args.build_dir
    extra_scripts = "pre:lib/ci/ci-flags.py"
    verbose = args.verbose
    out: ConcurrentRunArgs = ConcurrentRunArgs(
        projects=projects,
        examples=examples_paths,
        skip_init=skip_init,
        defines=defines,
        extra_packages=extra_packages,
        libs=LIBS,
        build_dir=build_dir,
        extra_scripts=extra_scripts,
        cwd=str(HERE.parent),
        board_dir=(HERE / "boards").absolute().as_posix(),
        build_flags=["-Wl,-Map,firmware.map"],
        verbose=verbose,
    )
    return out


def main() -> int:
    """Main function."""
    args = parse_args()
    if args.supported_boards:
        print(",".join(DEFAULT_BOARDS_NAMES))
        return 0
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
