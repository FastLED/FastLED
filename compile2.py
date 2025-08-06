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
from typing import Callable

from ci.compiler.pio import PlatformIoBuilder, BuildResult, run_pio_build
from ci.util.boards import ALL


def yellow_text(text: str) -> str:
    """Return text in yellow color using ANSI escape codes."""
    return f"\033[33m{text}\033[0m"


def green_text(text: str) -> str:
    """Return text in green color using ANSI escape codes."""
    return f"\033[32m{text}\033[0m"


def red_text(text: str) -> str:
    """Return text in red color using ANSI escape codes."""
    return f"\033[31m{text}\033[0m"


def create_banner(text: str, color_func: Callable[[str], str]) -> str:
    """Create a banner with the given text and color function."""
    # Create a banner box around the text
    border_char = "="
    padding = 2
    text_width = len(text)
    total_width = text_width + (padding * 2)
    
    # Create the banner lines
    top_border = border_char * (total_width + 4)
    middle_line = f"{border_char} {' ' * padding}{text}{' ' * padding} {border_char}"
    bottom_border = border_char * (total_width + 4)
    
    # Apply color to the entire banner
    banner = f"{top_border}\n{middle_line}\n{bottom_border}"
    return color_func(banner)


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
            "platform", nargs="?", help="Platform to build for (default: 'uno', e.g. 'esp32dev', 'teensy31')"
        )
        parser.add_argument(
            "example", nargs="?", help="Optional example name to build (default: 'Blink')"
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
        
        # Use defaults when not provided
        platform = parsed_args.platform
        if not platform:
            platform = "uno"
            print(yellow_text("WARNING: Board wasn't specified, assuming 'uno'"))
        
        # Handle examples logic
        examples: list[str] = []
        if parsed_args.examples:
            # Split comma-separated examples from --examples flag
            examples = [ex.strip() for ex in parsed_args.examples.split(",") if ex.strip()]
        if parsed_args.example:
            # Add positional example if provided
            examples.append(parsed_args.example)
        
        # Use default example if none provided
        if not examples:
            examples = ["Blink"]
            print(yellow_text("WARNING: Example wasn't specified, assuming 'Blink'"))
        
        # Resolve example names/paths
        resolved_examples = _resolve_example_paths(examples)
        
        return Args(
            platform=platform,
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
                error_banner = create_banner(f"BUILD FAILED: {result.example}", red_text)
                print(f"\n{error_banner}")
                print(f"Error details: {result.output}")
                cancel_futures()
                return 1
            else:
                success_banner = create_banner(f"BUILD SUCCESS: {result.example}", green_text)
                print(f"\n{success_banner}")
    except Exception as e:
        error_banner = create_banner(f"BUILD ERROR: {e}", red_text)
        print(f"\n{error_banner}")
        cancel_futures()
        return 1

    # Final success message for multiple examples
    if len(args.examples) > 1:
        final_banner = create_banner(f"ALL BUILDS COMPLETE: {len(args.examples)} examples", green_text)
        print(f"\n{final_banner}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
