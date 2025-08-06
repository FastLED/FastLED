#!/usr/bin/env python3
"""
FastLED Example Compiler

Streamlined compiler that uses the PioCompiler to build FastLED examples for various boards.
This replaces the previous complex compilation system with a simpler approach using the Pio compiler.
"""

import argparse
import sys
import time
from concurrent.futures import Future, as_completed
from pathlib import Path
from typing import List, Optional, cast

from ci.compiler.compiler import SketchResult
from ci.compiler.pio import PioCompiler
from ci.util.boards import Board, create_board


# Default boards to compile for
DEFAULT_BOARDS_NAMES = [
    "uno",  # Build is faster if this is first, because it's used for global init.
    "esp32dev",
    "esp01",  # ESP8266
    "esp32c3",
    "esp32c6",
    "esp32s3",
    "teensylc",
    "teensy31",
    "teensy41",
    "digix",
    "rpipico",
    "rpipico2",
]

# Default examples to compile
DEFAULT_EXAMPLES = [
    "Blink",
    "DemoReel100",
    "Fire2012",
    "ColorPalette",
    "Noise",
    "Pride2015",
    "Pacifica",
]


def parse_args(args: Optional[list[str]] = None) -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Compile FastLED examples for various boards using PioCompiler"
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
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose output"
    )
    parser.add_argument(
        "--supported-boards",
        action="store_true",
        help="Print the list of supported boards and exit",
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
        parsed_args = parser.parse_intermixed_args(args)
        unknown = []
    except SystemExit:
        # If parse_intermixed_args fails, fall back to parse_known_args
        parsed_args, unknown = parser.parse_known_args(args)

    # Handle unknown arguments intelligently - treat non-flag arguments as examples
    unknown_examples: List[str] = []
    for arg in unknown:
        if not arg.startswith("-"):
            unknown_examples.append(arg)

    # Add unknown examples to positional_examples
    if unknown_examples:
        if (
            not hasattr(parsed_args, "positional_examples")
            or parsed_args.positional_examples is None
        ):
            parsed_args.positional_examples = []
        # Type assertion to help the type checker - argparse returns Any but we know it's List[str]
        positional_examples: List[str] = cast(
            List[str], getattr(parsed_args, "positional_examples", [])
        )
        positional_examples.extend(unknown_examples)
        parsed_args.positional_examples = positional_examples

    return parsed_args


def resolve_example_path(example: str) -> str:
    """Resolve example name to path, ensuring it exists."""
    project_root = Path(__file__).parent.parent.resolve()
    examples_dir = project_root / "examples"

    # Handle both "Blink" and "examples/Blink" formats
    if example.startswith("examples/"):
        example = example[len("examples/") :]

    example_path = examples_dir / example
    if not example_path.exists():
        raise FileNotFoundError(f"Example not found: {example_path}")

    return example


def compile_board_examples(
    board: Board, examples: List[str], defines: List[str], verbose: bool
) -> tuple[bool, str]:
    """Compile examples for a single board using PioCompiler."""
    print(f"\n{'=' * 60}")
    print(f"COMPILING BOARD: {board.board_name}")
    print(f"EXAMPLES: {', '.join(examples)}")
    print(f"{'=' * 60}")

    try:
        # Create PioCompiler instance
        compiler = PioCompiler(
            board=board,
            verbose=verbose,
            additional_defines=defines,
        )

        # Build all examples
        futures: List[Future[SketchResult]] = compiler.build(examples)

        # Wait for completion and collect results
        compilation_errors: List[str] = []
        successful_builds = 0

        for future in as_completed(futures):
            try:
                result = future.result()
                if result.success:
                    successful_builds += 1
                    print(f"SUCCESS: {result.example}")
                else:
                    compilation_errors.append(f"{result.example}: {result.output}")
                    print(f"FAILED: {result.example}")
            except Exception as e:
                compilation_errors.append(f"Build exception: {str(e)}")
                print(f"EXCEPTION during build: {e}")

        # Cleanup
        compiler.cancel_all()

        if compilation_errors:
            error_msg = f"Failed to compile {len(compilation_errors)} examples:\n"
            error_msg += "\n".join(f"  - {error}" for error in compilation_errors)
            return False, error_msg
        else:
            return True, f"Successfully compiled {successful_builds} examples"

    except Exception as e:
        return False, f"Compiler setup failed: {str(e)}"


def main() -> int:
    """Main function."""
    args = parse_args()

    if args.supported_boards:
        print(",".join(DEFAULT_BOARDS_NAMES))
        return 0

    # Determine which boards to compile for
    if args.boards:
        boards_names = args.boards.split(",")
    else:
        boards_names = DEFAULT_BOARDS_NAMES

    # Get board objects
    boards: List[Board] = []
    for board_name in boards_names:
        try:
            board = create_board(board_name, no_project_options=False)
            boards.append(board)
        except Exception as e:
            print(f"ERROR: Failed to get board '{board_name}': {e}")
            return 1

    # Determine which examples to compile
    if args.positional_examples:
        # Convert positional examples, handling both "examples/Blink" and "Blink" formats
        examples: List[str] = []
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

    # Validate examples exist
    for example in examples:
        try:
            resolve_example_path(example)
        except FileNotFoundError as e:
            print(f"ERROR: {e}")
            return 1

    # Set up defines
    defines: List[str] = []
    if args.defines:
        defines.extend(args.defines.split(","))

    # Add FASTLED_ALL_SRC define when --allsrc or --no-allsrc flag is specified
    if args.allsrc:
        defines.append("FASTLED_ALL_SRC=1")
    elif args.no_allsrc:
        defines.append("FASTLED_ALL_SRC=0")

    # Start compilation
    start_time = time.time()
    print(
        f"Starting compilation for {len(boards)} boards with {len(examples)} examples"
    )

    compilation_errors: List[str] = []

    # Compile for each board
    for board in boards:
        success, message = compile_board_examples(
            board=board,
            examples=examples,
            defines=defines,
            verbose=args.verbose,
        )

        if not success:
            compilation_errors.append(f"Board {board.board_name}: {message}")
            print(f"ERROR: Compilation failed for board {board.board_name}")
            # Continue with other boards instead of stopping

    # Report results
    elapsed_time = time.time() - start_time
    time_str = time.strftime("%Mm:%Ss", time.gmtime(elapsed_time))

    if compilation_errors:
        print(
            f"\nCompilation finished in {time_str} with {len(compilation_errors)} error(s):"
        )
        for error in compilation_errors:
            print(f"  - {error}")
        return 1
    else:
        print(f"\nAll compilations completed successfully in {time_str}")
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(1)
