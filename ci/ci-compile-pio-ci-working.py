#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""
Runs the compilation process for examples on boards using pio ci command.
This replaces the previous concurrent build system with a simpler pio ci approach.
"""

import argparse
import os
import subprocess
import sys
import time
import warnings
from pathlib import Path
from typing import List, Set

from ci.boards import Board, create_board  # type: ignore
from ci.util.locked_print import locked_print


HERE = Path(__file__).parent.resolve()

# Default boards to compile for
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
        description="Compile FastLED examples for various boards using pio ci.util."
    )
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
        "--defines", type=str, help="Comma-separated list of compiler definitions"
    )
    parser.add_argument("--customsdk", type=str, help="custom_sdkconfig project option")
    parser.add_argument(
        "--extra-packages",
        type=str,
        help="Comma-separated list of extra packages to install",
    )
    parser.add_argument(
        "--build-dir", type=str, help="Override the default build directory"
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Enable interactive mode to choose a board",
    )
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
        sys.exit(1)

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
    out: list[str] = []
    while True:
        try:
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


def compile_with_pio_ci(
    board: Board,
    example_paths: list[Path],
    build_dir: str | None,
    defines: list[str],
    verbose: bool,
) -> tuple[bool, str]:
    """Compile examples for a board using pio ci command."""

    # Skip web boards
    if board.board_name == "web":
        locked_print(f"Skipping web target for board {board.board_name}")
        return True, ""

    board_name = board.board_name
    real_board_name = board.get_real_board_name()

    # Set up build directory
    if build_dir:
        board_build_dir = Path(build_dir) / board_name
    else:
        board_build_dir = Path(".build") / board_name

    board_build_dir.mkdir(parents=True, exist_ok=True)

    locked_print(f"*** Compiling examples for board {board_name} using pio ci ***")

    errors: list[str] = []

    for example_path in example_paths:
        locked_print(
            f"*** Building example {example_path.name} for board {board_name} ***"
        )

        # Find the .ino file in the example directory
        ino_files = list(example_path.glob("*.ino"))
        if not ino_files:
            error_msg = f"No .ino file found in {example_path}"
            locked_print(f"ERROR: {error_msg}")
            errors.append(error_msg)
            continue

        # Use the first .ino file found
        ino_file = ino_files[0]

        # Build pio ci command
        cmd_list = [
            "pio",
            "ci",
            str(ino_file),
            "--board",
            real_board_name,
            "--lib",
            "src",  # FastLED source directory
            "--keep-build-dir",
            "--build-dir",
            str(board_build_dir / example_path.name),
        ]

        # Add platform-specific options
        if board.platform:
            cmd_list.extend(["--project-option", f"platform={board.platform}"])

        if board.platform_packages:
            cmd_list.extend(
                ["--project-option", f"platform_packages={board.platform_packages}"]
            )

        if board.framework:
            cmd_list.extend(["--project-option", f"framework={board.framework}"])

        if board.board_build_core:
            cmd_list.extend(
                ["--project-option", f"board_build.core={board.board_build_core}"]
            )

        if board.board_build_filesystem_size:
            cmd_list.extend(
                [
                    "--project-option",
                    f"board_build.filesystem_size={board.board_build_filesystem_size}",
                ]
            )

        # Add defines
        all_defines = defines.copy()
        if board.defines:
            all_defines.extend(board.defines)

        if all_defines:
            build_flags = " ".join(f"-D{define}" for define in all_defines)
            cmd_list.extend(["--project-option", f"build_flags={build_flags}"])

        # Add custom SDK config if specified
        if board.customsdk:
            cmd_list.extend(["--project-option", f"custom_sdkconfig={board.customsdk}"])

        # Add verbose flag if requested
        if verbose:
            cmd_list.append("--verbose")

        # Execute the command
        cmd_str = subprocess.list2cmdline(cmd_list)
        locked_print(f"Running command: {cmd_str}")

        start_time = time.time()

        try:
            result = subprocess.run(
                cmd_list,
                capture_output=True,
                text=True,
                cwd=str(HERE.parent),
                timeout=300,  # 5 minute timeout per example
            )

            elapsed_time = time.time() - start_time

            if result.returncode == 0:
                locked_print(
                    f"*** Successfully built {example_path.name} for {board_name} in {elapsed_time:.2f}s ***"
                )
                if verbose and result.stdout:
                    locked_print(f"Build output:\n{result.stdout}")
            else:
                error_msg = f"Failed to build {example_path.name} for {board_name}"
                locked_print(f"ERROR: {error_msg}")
                locked_print(f"Command: {cmd_str}")
                if result.stdout:
                    locked_print(f"STDOUT:\n{result.stdout}")
                if result.stderr:
                    locked_print(f"STDERR:\n{result.stderr}")
                errors.append(f"{error_msg}: {result.stderr}")

        except subprocess.TimeoutExpired:
            error_msg = f"Timeout building {example_path.name} for {board_name}"
            locked_print(f"ERROR: {error_msg}")
            errors.append(error_msg)
        except Exception as e:
            error_msg = f"Exception building {example_path.name} for {board_name}: {e}"
            locked_print(f"ERROR: {error_msg}")
            errors.append(error_msg)

    if errors:
        return False, "\n".join(errors)

    return True, f"Successfully compiled all examples for {board_name}"


def run_symbol_analysis(boards: list[Board]) -> None:
    """Run symbol analysis on compiled outputs if requested."""
    locked_print("\nRunning symbol analysis on compiled outputs...")

    for board in boards:
        if board.board_name == "web":
            continue

        try:
            locked_print(f"Running symbol analysis for board: {board.board_name}")

            cmd = [
                "uv",
                "run",
                "ci/util/symbol_analysis.py",
                "--board",
                board.board_name,
            ]

            result = subprocess.run(
                cmd, capture_output=True, text=True, cwd=str(HERE.parent)
            )

            if result.returncode != 0:
                locked_print(
                    f"ERROR: Symbol analysis failed for board {board.board_name}: {result.stderr}"
                )
            else:
                locked_print(f"Symbol analysis completed for board: {board.board_name}")
                if result.stdout:
                    print(result.stdout)

        except Exception as e:
            locked_print(
                f"ERROR: Exception during symbol analysis for board {board.board_name}: {e}"
            )


def main() -> int:
    """Main function."""
    args = parse_args()

    if args.supported_boards:
        print(",".join(DEFAULT_BOARDS_NAMES))
        return 0

    # Determine which boards to compile for
    if args.interactive:
        boards_names = choose_board_interactively(
            DEFAULT_BOARDS_NAMES + OTHER_BOARDS_NAMES
        )
    else:
        boards_names = args.boards.split(",") if args.boards else DEFAULT_BOARDS_NAMES

    # Get board objects
    boards: list[Board] = []
    for board_name in boards_names:
        try:
            board = create_board(board_name, no_project_options=False)
            boards.append(board)
        except Exception as e:
            locked_print(f"ERROR: Failed to get board '{board_name}': {e}")
            return 1

    # Determine which examples to compile
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

    # Process example exclusions
    if args.exclude_examples:
        exclude_examples = args.exclude_examples.split(",")
        examples = [ex for ex in examples if ex not in exclude_examples]

    # Resolve example paths
    example_paths: list[Path] = []
    for example in examples:
        try:
            example_path = resolve_example_path(example)
            example_paths.append(example_path)
        except FileNotFoundError as e:
            locked_print(f"ERROR: {e}")
            return 1

    # Add extra examples for specific boards
    extra_examples: dict[Board, list[Path]] = {}
    for board in boards:
        if board in EXTRA_EXAMPLES:
            board_examples = []
            for example in EXTRA_EXAMPLES[board]:
                try:
                    board_examples.append(resolve_example_path(example))
                except FileNotFoundError as e:
                    locked_print(f"WARNING: {e}")
            if board_examples:
                extra_examples[board] = board_examples

    # Set up defines
    defines: list[str] = []
    if args.defines:
        defines.extend(args.defines.split(","))

    # Add FASTLED_ALL_SRC define when --allsrc or --no-allsrc flag is specified
    if args.allsrc:
        defines.append("FASTLED_ALL_SRC=1")
    elif args.no_allsrc:
        defines.append("FASTLED_ALL_SRC=0")

    # Start compilation
    start_time = time.time()
    locked_print(
        f"Starting compilation for {len(boards)} boards with {len(example_paths)} examples"
    )

    compilation_errors: List[str] = []

    # Compile for each board
    for board in boards:
        board_examples = example_paths.copy()
        if board in extra_examples:
            board_examples.extend(extra_examples[board])

        success, message = compile_with_pio_ci(
            board=board,
            example_paths=board_examples,
            build_dir=args.build_dir,
            defines=defines,
            verbose=args.verbose,
        )

        if not success:
            compilation_errors.append(f"Board {board.board_name}: {message}")
            locked_print(f"ERROR: Compilation failed for board {board.board_name}")
            # Continue with other boards instead of stopping

    # Run symbol analysis if requested
    if args.symbols:
        run_symbol_analysis(boards)

    # Report results
    elapsed_time = time.time() - start_time
    time_str = time.strftime("%Mm:%Ss", time.gmtime(elapsed_time))

    if compilation_errors:
        locked_print(
            f"\nCompilation finished in {time_str} with {len(compilation_errors)} error(s):"
        )
        for error in compilation_errors:
            locked_print(f"  - {error}")
        return 1
    else:
        locked_print(f"\nAll compilations completed successfully in {time_str}")
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        locked_print("\nInterrupted by user")
        sys.exit(1)
