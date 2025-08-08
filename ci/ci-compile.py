#!/usr/bin/env python3
"""
FastLED Example Compiler

Streamlined compiler that uses the PioCompiler to build FastLED examples for various boards.
This replaces the previous complex compilation system with a simpler approach using the Pio compiler.
"""

import argparse
import os
import sys
import threading
import time
from concurrent.futures import Future, as_completed
from pathlib import Path
from typing import List, Optional, cast

from ci.compiler.compiler import CacheType, SketchResult
from ci.compiler.pio import PioCompiler
from ci.util.boards import ALL, Board, create_board


def green_text(text: str) -> str:
    """Return text in green color."""
    return f"\033[32m{text}\033[0m"


def red_text(text: str) -> str:
    """Return text in red color."""
    return f"\033[31m{text}\033[0m"


def get_default_boards() -> List[str]:
    """Get all board names from the ALL boards list, with preferred boards first."""
    # These are the boards we want to compile first (preferred order)
    # Order matters: UNO first because it's used for global init and builds faster
    preferred_board_names = [
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

    # Get all available board names from the ALL list
    available_board_names = {board.board_name for board in ALL}

    # Start with preferred boards that exist, warn about missing ones
    default_boards: List[str] = []
    for board_name in preferred_board_names:
        if board_name in available_board_names:
            default_boards.append(board_name)
        else:
            print(
                f"WARNING: Preferred board '{board_name}' not found in available boards"
            )

    # Add all remaining boards (sorted for consistency)
    remaining_boards = sorted(available_board_names - set(default_boards))
    default_boards.extend(remaining_boards)

    return default_boards


def get_all_examples() -> List[str]:
    """Get all available example names from the examples directory."""
    project_root = Path(__file__).parent.parent.resolve()
    examples_dir = project_root / "examples"

    if not examples_dir.exists():
        return []

    examples: List[str] = []
    for item in examples_dir.iterdir():
        if item.is_dir():
            # Check if it contains a .ino file with the same name
            ino_file = item / f"{item.name}.ino"
            if ino_file.exists():
                examples.append(item.name)

    # Sort for consistent ordering
    examples.sort()
    return examples


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
        "--no-cache", action="store_true", help="Disable sccache for faster compilation"
    )
    parser.add_argument(
        "--cache",
        action="store_true",
        help="(Deprecated) Enable sccache for faster compilation",
    )
    parser.add_argument(
        "--supported-boards",
        action="store_true",
        help="Print the list of supported boards and exit",
    )
    parser.add_argument(
        "--no-interactive",
        action="store_true",
        help="Disables interactive mode (deprecated)",
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
    board: Board,
    examples: List[str],
    defines: List[str],
    verbose: bool,
    enable_cache: bool,
) -> tuple[bool, str]:
    """Compile examples for a single board using PioCompiler."""
    print(f"\n{'=' * 60}")
    print(f"COMPILING BOARD: {board.board_name}")
    print(f"EXAMPLES: {', '.join(examples)}")
    print(f"{'=' * 60}")

    try:
        # Determine cache type based on flag
        cache_type = CacheType.SCCACHE if enable_cache else CacheType.NO_CACHE

        # Create PioCompiler instance
        compiler = PioCompiler(
            board=board,
            verbose=verbose,
            additional_defines=defines,
            cache_type=cache_type,
        )

        # Build all examples
        futures: List[Future[SketchResult]] = compiler.build(examples)

        # Wait for completion and collect results
        compilation_errors: List[str] = []
        successful_builds = 0

        for future in futures:
            try:
                result = future.result()
                if result.success:
                    successful_builds += 1
                    # SUCCESS message is now printed by the worker thread
                else:
                    compilation_errors.append(f"{result.example}: {result.output}")
                    # FAILED message is now printed by the worker thread
            except KeyboardInterrupt:
                print("Keyboard interrupt detected, cancelling builds")
                compiler.cancel_all()
                import _thread

                _thread.interrupt_main()
                raise
            except Exception as e:
                compilation_errors.append(f"Build exception: {str(e)}")
                print(f"EXCEPTION during build: {e}")
                # Cleanup
                compiler.cancel_all()

        # Show compiler statistics after all builds complete
        try:
            stats = compiler.get_cache_stats()
            if stats:
                print("\n" + "=" * 60)
                print("COMPILER STATISTICS")
                print("=" * 60)
                print(stats)
                print("=" * 60)
        except Exception as e:
            print(f"Warning: Could not retrieve compiler statistics: {e}")

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
    if args.verbose:
        os.environ["VERBOSE"] = "1"

    if args.supported_boards:
        print(",".join(get_default_boards()))
        return 0

    # Determine which boards to compile for
    if args.boards:
        boards_names = args.boards.split(",")
    else:
        boards_names = get_default_boards()

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
        # Compile all available examples since builds are fast now!
        examples = get_all_examples()

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
            enable_cache=not args.no_cache,
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
