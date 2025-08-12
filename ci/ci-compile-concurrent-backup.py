#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""
BACKUP: Original concurrent build system - REPLACED by pio ci approach

This file is a backup of the original ci-compile.py that used concurrent builds.
It has been replaced by a simpler pio ci approach in the main ci-compile.py.

The original system used ThreadPoolExecutor to run multiple board compilations
in parallel with build directory creation and artifact recycling.

The new system uses pio ci command directly for each board/example combination,
which is simpler and more straightforward.

This backup is kept for reference and potential future needs.
"""

import argparse
import concurrent.futures
import io
import multiprocessing
import os
import subprocess
import sys
import time
import warnings
from pathlib import Path
from typing import List, Set

from ci.boards import Board, create_board  # type: ignore
from ci.util.concurrent_run import ConcurrentRunArgs, concurrent_run
from ci.util.locked_print import locked_print


HERE = Path(__file__).parent.resolve()

LIBS = ["src"]
EXTRA_LIBS = [
    "https://github.com/me-no-dev/ESPAsyncWebServer.git",
    "ArduinoOTA",
    "SD",
    "FS",
    "ESPmDNS",
    "WiFi",
    "WebSockets",
]
BUILD_FLAGS = ["-Wl,-Map,firmware.map", "-fopt-info-all=optimization_report.txt"]

# Default boards to compile for. You can use boards not defined here but
# if the board isn't part of the officially supported platformio boards then
# you will need to add the board to the ~/.platformio/platforms directory.
# prior to running this script. This happens automatically as of 2024-08-20
# with the github workflow scripts.
DEFAULT_BOARDS_NAMES = [
    "apollo3_red",
    "apollo3_thing_explorable",
    "web",  # work in progress
    "uno",  # Build is faster if this is first, because it's used for global init.
    "esp32dev",
    "esp01",  # ESP8266
    "esp32c3",
    "attiny85",
    "ATtiny1616",
    "esp32c6",
    "esp32s3",
    "esp32p4",
    "yun",
    "digix",
    "teensylc",
    "teensy30",
    "teensy31",
    "teensy41",
    "adafruit_feather_nrf52840_sense",
    "xiaoblesense_adafruit",
    "rpipico",
    "rpipico2",
    "uno_r4_wifi",
    "esp32rmt_51",
    "esp32dev_idf44",
    "bluepill",
    "esp32rmt_51",
    "giga_r1",
    "sparkfun_xrp_controller",
]

OTHER_BOARDS_NAMES = [
    "nano_every",
    "esp32-c2-devkitm-1",
]

# Examples to compile.
DEFAULT_EXAMPLES = [
    "Animartrix",
    "Apa102",
    "Apa102HD",
    "Apa102HDOverride",
    "Audio",
    "Blink",
    "Blur",
    "Chromancer",
    "ColorPalette",
    "ColorTemperature",
    "Corkscrew",
    "Cylon",
    "DemoReel100",
    # "Downscale",
    "FestivalStick",
    "FirstLight",
    "Fire2012",
    "Multiple/MultipleStripsInOneArray",
    "Multiple/ArrayOfLedArrays",
    "Noise",
    "NoisePlayground",
    "NoisePlusPalette",
    # "LuminescentGrand",
    "Pacifica",
    "Pride2015",
    "RGBCalibrate",
    "RGBSetDemo",
    "RGBW",
    "Overclock",
    "RGBWEmulated",
    "TwinkleFox",
    "XYMatrix",
    "FireMatrix",
    "FireCylinder",
    "FxGfx2Video",
    "FxSdCard",
    "FxCylon",
    "FxDemoReel100",
    "FxTwinkleFox",
    "FxFire2012",
    "FxNoisePlusPalette",
    "FxPacifica",
    "FxEngine",
    "WS2816",
]

EXTRA_EXAMPLES: dict[Board, list[str]] = {
    # ESP32DEV: ["EspI2SDemo"],
    # ESP32_S3_DEVKITC_1: ["EspS3I2SDemo"],
}


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
        "positional_examples",
        type=str,
        help="Examples to compile (positional arguments after board name)",
        nargs="*",
    )
    parser.add_argument(
        "--examples", type=str, help="Comma-separated list of examples to compile"
    )
    parser.add_argument(
        "--exclude-examples", type=str, help="Examples that should be excluded"
    )
    parser.add_argument(
        "--skip-init", action="store_true", help="Skip the initialization step"
    )
    parser.add_argument(
        "--defines", type=str, help="Comma-separated list of compiler definitions"
    )
    parser.add_argument("--customsdk", type=str, help="custom_sdkconfig project option")
    parser.add_argument(
        "--extra-packages",
        type=str,
        help="Comma-separated list of extra packages to install",
    )
    parser.add_argument(
        "--add-extra-esp32-libs",
        action="store_true",
        help="Add extra libraries to the libraries list to check against compiler errors.",
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
    parser.add_argument(
        "--symbols",
        action="store_true",
        help="Run symbol analysis on compiled output",
    )
    parser.add_argument(
        "--allsrc",
        action="store_true",
        help="Enable all-source build (adds FASTLED_ALL_SRC=1 define)",
    )
    parser.add_argument(
        "--no-allsrc",
        action="store_true",
        help="Disable all-source build (adds FASTLED_ALL_SRC=0 define)",
    )
    try:
        args = parser.parse_intermixed_args()
        unknown = []
    except SystemExit:
        # If parse_intermixed_args fails, fall back to parse_known_args
        args, unknown = parser.parse_known_args()

    # Handle unknown arguments intelligently - treat non-flag arguments as examples
    unknown_examples = []
    unknown_flags = []
    for arg in unknown:
        if arg.startswith("-"):
            unknown_flags.append(arg)
        else:
            unknown_examples.append(arg)

    # Add unknown examples to positional_examples
    if unknown_examples:
        if not hasattr(args, "positional_examples") or args.positional_examples is None:
            args.positional_examples = []
        args.positional_examples.extend(unknown_examples)

    # Only warn about actual unknown flags, not examples
    if unknown_flags:
        warnings.warn(f"Unknown arguments: {unknown_flags}")

    # Check for FASTLED_CI_NO_INTERACTIVE environment variable
    # This allows test.py and other scripts to force non-interactive mode
    if os.environ.get("FASTLED_CI_NO_INTERACTIVE") == "true":
        args.interactive = False
        args.no_interactive = True

    # if --interactive and --no-interative are both passed, --no-interactive takes precedence.
    if args.interactive and args.no_interactive:
        warnings.warn(
            "Both --interactive and --no-interactive were passed, --no-interactive takes precedence."
        )
        args.interactive = False

    # Validate that --allsrc and --no-allsrc are not both specified
    if args.allsrc and args.no_allsrc:
        warnings.warn(
            "Both --allsrc and --no-allsrc were passed, this is contradictory. Please specify only one."
        )
        sys.exit(1)  # Exit with error

    return args


def remove_duplicates(items: List[str]) -> List[str]:
    seen: Set[str] = set()
    out: List[str] = []
    for item in items:
        if item not in seen:
            seen.add(item)
            out.append(item)
    return out


def choose_board_interactively(boards: List[str]) -> List[str]:
    print("Available boards:")
    boards = remove_duplicates(sorted(boards))
    for i, board in enumerate(boards):
        print(f"[{i}]: {board}")
    print("[all]: All boards")
    out: List[str] = []
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
        projects.append(create_board(board, no_project_options=args.no_project_options))
    extra_examples: dict[Board, list[Path]] = {}
    # Handle both positional and named examples
    if args.positional_examples:
        # Convert positional examples, handling both "examples/Blink" and "Blink" formats
        examples = []
        for example in args.positional_examples:
            # Remove "examples/" prefix if present
            if example.startswith("examples/"):
                example = example[len("examples/") :]
            examples.append(example)
    elif args.examples:
        examples = args.examples.split(",")
    else:
        examples = DEFAULT_EXAMPLES
        # Only add extra examples when using defaults
        for b, _examples in EXTRA_EXAMPLES.items():
            resolved_examples = [resolve_example_path(example) for example in _examples]
            extra_examples[b] = resolved_examples
    examples_paths = [resolve_example_path(example) for example in examples]
    # now process example exclusions.
    if args.exclude_examples:
        exclude_examples = args.exclude_examples.split(",")
        examples_paths = [
            example
            for example in examples_paths
            if example.name not in exclude_examples
        ]
        for exclude in exclude_examples:
            examples.remove(exclude)
    customsdk = args.customsdk
    defines: list[str] = []
    if args.defines:
        defines.extend(args.defines.split(","))
    # Add FASTLED_ALL_SRC define when --allsrc or --no-allsrc flag is specified
    if args.allsrc:
        defines.append("FASTLED_ALL_SRC=1")
    elif args.no_allsrc:
        defines.append("FASTLED_ALL_SRC=0")
    extra_packages: list[str] = []
    if args.extra_packages:
        extra_packages.extend(args.extra_packages.split(","))
    build_dir = args.build_dir
    extra_scripts = "pre:lib/ci/util-flags.py"
    verbose = args.verbose

    out: ConcurrentRunArgs = ConcurrentRunArgs(
        projects=projects,
        examples=examples_paths,
        skip_init=skip_init,
        defines=defines,
        customsdk=customsdk,
        extra_packages=extra_packages,
        libs=LIBS,
        build_dir=build_dir,
        extra_scripts=extra_scripts,
        cwd=str(HERE.parent),
        board_dir=(HERE / "boards").absolute().as_posix(),
        build_flags=BUILD_FLAGS,
        verbose=verbose,
        extra_examples=extra_examples,
        symbols=args.symbols,
    )
    return out


def main() -> int:
    """Main function."""
    args = parse_args()
    if args.supported_boards:
        print(",".join(DEFAULT_BOARDS_NAMES))
        return 0
    if args.add_extra_esp32_libs:
        LIBS.extend(EXTRA_LIBS)

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
