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
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, cast

from typeguard import typechecked

from ci.boards import ALL, Board, create_board
from ci.compiler.compiler import CacheType, SketchResult
from ci.compiler.pio import PioCompiler


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
        "--no-cache",
        action="store_true",
        help="Disable sccache for faster compilation (default is already disabled)",
    )
    parser.add_argument(
        "--enable-cache",
        action="store_true",
        help="Enable sccache for faster compilation",
    )
    parser.add_argument(
        "--cache",
        action="store_true",
        help="(Deprecated) Enable sccache for faster compilation - use --enable-cache instead",
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
    parser.add_argument(
        "--log-failures",
        type=str,
        help="Directory to write per-example failure logs (created on first failure)",
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


@typechecked
@dataclass
class BoardCompilationResult:
    """Aggregated result for compiling a set of examples on a single board."""

    ok: bool
    sketch_results: List[SketchResult]


def compile_board_examples(
    board: Board,
    examples: List[str],
    defines: List[str],
    verbose: bool,
    enable_cache: bool,
) -> BoardCompilationResult:
    """Compile examples for a single board using PioCompiler."""
    print(f"\n{'=' * 60}")
    print(f"COMPILING BOARD: {board.board_name}")
    print(f"EXAMPLES: {', '.join(examples)}")
    print(f"{'=' * 60}")

    try:
        # Determine cache type based on flag and board frameworks
        frameworks = [f.strip().lower() for f in (board.framework or "").split(",")]
        mixed_frameworks = "arduino" in frameworks and "espidf" in frameworks
        cache_type = (
            CacheType.SCCACHE
            if enable_cache and not mixed_frameworks
            else CacheType.NO_CACHE
        )

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
        results: List[SketchResult] = []

        for future in futures:
            try:
                result = future.result()
                results.append(result)
                # SUCCESS/FAILED messages are printed by worker threads
            except KeyboardInterrupt:
                print("Keyboard interrupt detected, cancelling builds")
                compiler.cancel_all()
                import _thread

                _thread.interrupt_main()
                raise
            except Exception as e:
                # Represent unexpected exception as a failed SketchResult for consistency
                from pathlib import Path as _Path

                results.append(
                    SketchResult(
                        success=False,
                        output=f"Build exception: {str(e)}",
                        build_dir=_Path("."),
                        example="<exception>",
                    )
                )
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

        any_failures = False
        for r in results:
            if not r.success:
                any_failures = True
                break
        return BoardCompilationResult(ok=not any_failures, sketch_results=results)
    except KeyboardInterrupt:
        print("Keyboard interrupt detected, cancelling builds")
        import _thread

        _thread.interrupt_main()
        raise
    except Exception as e:
        # Compiler could not be set up; return a single failed result to carry message
        from pathlib import Path as _Path

        return BoardCompilationResult(
            ok=False,
            sketch_results=[
                SketchResult(
                    success=False,
                    output=f"Compiler setup failed: {str(e)}",
                    build_dir=_Path("."),
                    example="<setup>",
                )
            ],
        )


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
    failed_example_names: List[str] = []
    failure_logs_dir: Optional[Path] = (
        Path(args.log_failures) if getattr(args, "log_failures", None) else None
    )

    # Compile for each board
    for board in boards:
        result = compile_board_examples(
            board=board,
            examples=examples,
            defines=defines,
            verbose=args.verbose,
            enable_cache=(args.enable_cache or args.cache) and not args.no_cache,
        )

        if not result.ok:
            # Record board-level error
            compilation_errors.append(f"Board {board.board_name} failed")
            print(red_text(f"ERROR: Compilation failed for board {board.board_name}"))
            # Print each failing sketch's stdout and collect names for summary
            for sketch in result.sketch_results:
                if not sketch.success:
                    if sketch.example and sketch.example not in failed_example_names:
                        failed_example_names.append(sketch.example)
                    # Write per-example failure logs when requested
                    if failure_logs_dir is not None:
                        try:
                            failure_logs_dir.mkdir(parents=True, exist_ok=True)
                            safe_name = f"{sketch.example}.log"
                            log_path = failure_logs_dir / safe_name
                            with log_path.open(
                                "a", encoding="utf-8", errors="ignore"
                            ) as f:
                                f.write(sketch.output)
                                f.write("\n")
                        except KeyboardInterrupt:
                            print("Keyboard interrupt detected, cancelling builds")
                            import _thread

                            _thread.interrupt_main()
                            raise
                        except Exception as e:
                            print(
                                f"Warning: Could not write failure log for {sketch.example}: {e}"
                            )
                    print(f"\n{'-' * 60}")
                    print(f"Sketch: {sketch.example}")
                    print(f"{'-' * 60}")
                    # Print the collected output for this sketch
                    print(sketch.output)
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
        if failed_example_names:
            print("")
            print(red_text(f"ERROR! There were {len(failed_example_names)} failures:"))
            # Sort for stable output
            for name in sorted(failed_example_names):
                # print(f"  {name}")
                # same but in red
                print(red_text(f"  {name}"))
            print("")
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
