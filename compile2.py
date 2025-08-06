#!/usr/bin/env python3
"""
FastLED Target Build Script

Convenience script to run a full build test on any target platform using
the new PlatformIO testing system.
"""

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

from ci.compiler.pio import PlatformIoBuilder, BuildResult, run_pio_build
from ci.util.boards import ALL


def _resolve_example_paths(examples: list[str]) -> list[str]:
    """Resolve example names or paths to appropriate format.
    
    Args:
        examples: List of example names or paths
        
    Returns:
        List of resolved paths or names for the build system
    """
    # Just pass through - the PlatformIO module now handles both names and paths
    return examples


@dataclass
class Args:
    platform: str
    examples: list[str]
    verbose: bool

    @staticmethod
    def parse_args() -> "Args":
        parser = argparse.ArgumentParser(description="FastLED Target Build Script")
        parser.add_argument(
            "platform", nargs="?", help="Platform to build for (e.g. 'uno', 'esp32dev')"
        )
        parser.add_argument(
            "example", nargs="?", help="Optional example name to build"
        )
        parser.add_argument(
            "--examples", help="Comma-separated list of examples to build (e.g. 'Blink,FestivalStick')"
        )
        parser.add_argument(
            "--verbose", action="store_true", help="Enable verbose output"
        )
        parser.add_argument(
            "--list", action="store_true", help="List all available boards and examples in two-column format and exit"
        )
        
        parsed_args = parser.parse_args()
        
        # Handle --list option first (exit early)
        if parsed_args.list:
            # Get all available boards
            boards = sorted([board.board_name for board in ALL])
            
            # Get all available examples
            examples_dir = Path("examples")
            examples = []
            if examples_dir.exists():
                for example_path in examples_dir.iterdir():
                    if example_path.is_dir() and not example_path.name.startswith('.'):
                        # Check if directory contains a .ino file
                        ino_files = list(example_path.glob("*.ino"))
                        if ino_files:
                            examples.append(example_path.name)
            examples.sort()
            
            # Calculate column widths
            header1 = "Available Boards"
            header2 = "Examples"
            padding = 2  # Space padding for readability
            
            # Find max width for each column (including headers)
            max_board_width = max(len(header1), max(len(board) for board in boards) if boards else 0)
            max_example_width = max(len(header2), max(len(example) for example in examples) if examples else 0)
            
            # Add padding
            board_col_width = max_board_width + padding
            example_col_width = max_example_width + padding
            
            # Print two-column format with calculated widths
            print(f"{header1:<{board_col_width}}| {header2}")
            print("-" * board_col_width + "+" + "-" * (example_col_width + 1))
            
            max_rows = max(len(boards), len(examples))
            for i in range(max_rows):
                board = boards[i] if i < len(boards) else ""
                example = examples[i] if i < len(examples) else ""
                print(f"{board:<{board_col_width}}| {example}")
            
            sys.exit(0)
        
        # Validate required arguments when not using --list
        if not parsed_args.platform:
            parser.error("Platform argument is required")
        
        # Handle examples logic
        examples: list[str] = []
        if parsed_args.examples:
            # Split comma-separated examples from --examples flag
            examples = [ex.strip() for ex in parsed_args.examples.split(",") if ex.strip()]
        if parsed_args.example:
            # Add positional example if provided
            examples.append(parsed_args.example)
        
        if not examples:
            parser.error("Must specify at least one example either as positional argument or via --examples")
        
        # Resolve example names/paths
        resolved_examples = _resolve_example_paths(examples)
        
        return Args(
            platform=parsed_args.platform,
            examples=resolved_examples,
            verbose=parsed_args.verbose
        )


def main() -> int:
    """Main entry point."""

    args = Args.parse_args()

    # run_pio_build now accepts both Board objects and strings
    # The string will be automatically resolved to a Board object via get_board()
    futures = run_pio_build(args.platform, args.examples, args.verbose)

    def cancel_futures() -> None:
        for future in futures:
            future.cancel()

    try:
        for future in futures:
            result: BuildResult = future.result(timeout=120)
            if not result.success:
                print(f"Failed to build {result.example}: {result.output}")
                cancel_futures()
                return 1
            else:
                print(f"Successfully built {result.example}")
    except Exception as e:
        print(f"Error: {e}")
        cancel_futures()
        return 1

    print(f"Successfully built all {len(args.examples)} examples")
    return 0


if __name__ == "__main__":
    sys.exit(main())
