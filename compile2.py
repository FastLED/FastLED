#!/usr/bin/env python3
"""
FastLED Target Build Script

Convenience script to run a full build test on any target platform using
the new PlatformIO testing system.
"""

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from ci.compiler.pio import PioCompiler, SketchResult, run_pio_build
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


def create_banner(text: str, color_func: Callable[[str], str] | None = None) -> str:
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
    banner = f"{top_border}\n{middle_line}\n{bottom_border}\n"
    return color_func(banner) if color_func else banner


def _discover_all_examples() -> list[str]:
    """Discover all available examples by scanning the examples directory.
    
    Returns:
        List of all available example names
    """
    examples: list[str] = []
    examples_dir = Path("examples")
    if examples_dir.exists():
        for example_path in examples_dir.iterdir():
            if example_path.is_dir() and not example_path.name.startswith('.'):
                # Check if directory contains a .ino file
                ino_files = list(example_path.glob("*.ino"))
                if ino_files:
                    examples.append(example_path.name)
    return sorted(examples)


def _resolve_example_paths(examples: list[str]) -> list[str]:
    """Resolve example names or paths to appropriate format.
    
    Args:
        examples: List of example names or paths
        
    Returns:
        List of resolved paths or names for the build system
    """
    resolved_examples: list[str] = []
    for example in examples:
        if example.lower() == "all":
            # Replace "All" with all available examples
            all_examples = _discover_all_examples()
            resolved_examples.extend(all_examples)
        else:
            resolved_examples.append(example)
    return resolved_examples


@dataclass
class Args:
    platform: str
    examples: list[str]
    verbose: bool
    clean: bool
    clean_all: bool
    additional_include_dirs: list[str] | None

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
            "--examples", help="Comma-separated list of examples to build (e.g. 'Blink,FestivalStick') or 'All' to build all available examples"
        )
        parser.add_argument(
            "--all", action="store_true", help="Build all available examples (shorthand for --examples All)"
        )
        parser.add_argument(
            "--verbose", action="store_true", help="Enable verbose output"
        )
        parser.add_argument(
            "--list", action="store_true", help="List all available boards and examples in two-column format and exit"
        )
        parser.add_argument(
            "--list-examples", action="store_true", help="List all available examples as JSON dictionary with relative paths and exit"
        )
        parser.add_argument(
            "--clean", action="store_true", help="Clean build artifacts for the specified platform and exit"
        )
        parser.add_argument(
            "--clean-all", action="store_true", help="Clean all build artifacts (local and global packages) for the specified platform and exit"
        )
        parser.add_argument(
            "-I", "--include", action="append", dest="include_dirs", 
            help="Additional include directories to add to build flags (can be used multiple times, e.g. -I src/platforms/sub)"
        )
        
        parsed_args = parser.parse_args()
        
        # Handle --list option first (exit early)
        if parsed_args.list:
            # Get all available boards
            boards = sorted([board.board_name for board in ALL])
            
            # Get all available examples
            examples = _discover_all_examples()
            
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
        
        # Handle --list-examples option (exit early)
        if parsed_args.list_examples:
            examples_dict = {}
            examples_dir = Path("examples")
            
            if examples_dir.exists():
                for example_path in examples_dir.iterdir():
                    if example_path.is_dir() and not example_path.name.startswith('.'):
                        # Check if directory contains a .ino file
                        ino_files = list(example_path.glob("*.ino"))
                        if ino_files:
                            # Use relative path from project root (handle both absolute and relative paths)
                            try:
                                if example_path.is_absolute():
                                    relative_path = str(example_path.relative_to(Path.cwd()))
                                else:
                                    relative_path = str(example_path)
                            except ValueError:
                                # Fallback to just the example path
                                relative_path = str(example_path)
                            examples_dict[example_path.name] = relative_path
            
            # Output as JSON
            print(json.dumps(examples_dict, indent=2, sort_keys=True))
            sys.exit(0)
        
        # Use defaults when not provided
        platform = parsed_args.platform
        if not platform:
            platform = "uno"
            print(yellow_text("WARNING: Board wasn't specified, assuming 'uno'"))
        
        # Handle examples logic
        examples: list[str] = []
        if parsed_args.examples and parsed_args.all:
            raise ValueError("Cannot specify both --examples and --all flags")
        if parsed_args.examples:
            # Split comma-separated examples from --examples flag
            examples = [ex.strip() for ex in parsed_args.examples.split(",") if ex.strip()]
        if parsed_args.all:
            # --all flag is shorthand for --examples All
            examples = ["All"]
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
            verbose=parsed_args.verbose,
            clean=parsed_args.clean,
            clean_all=parsed_args.clean_all,
            additional_include_dirs=parsed_args.include_dirs
        )


def main() -> int:
    """Main entry point."""

    args = Args.parse_args()
    pio = PioCompiler(args.platform, args.verbose, additional_include_dirs=args.additional_include_dirs)

    # Handle clean operations (exit early)
    if args.clean_all:
        print(green_text(f"🧹 Starting clean-all operation for platform {args.platform}"))
        pio.clean_all()
        print(green_text(f"✅ Clean-all completed for platform {args.platform}"))
        return 0
    elif args.clean:
        print(green_text(f"🧹 Starting clean operation for platform {args.platform}"))
        pio.clean()
        print(green_text(f"✅ Clean completed for platform {args.platform}"))
        return 0

    # run_pio_build now accepts both Board objects and strings
    # The string will be automatically resolved to a Board object via create_board()
    futures = pio.build(args.examples)

    def cancel_futures() -> None:
        pio.cancel_all()

    try:
        first = True
        failed_examples: list[str] = []
        successful_examples: list[str] = []
        
        for future in futures:
            timeout = 60*60 if first else 60  # first build can be very long.
            first = False
            result: SketchResult = future.result(timeout=timeout)
            if not result.success:
                error_banner = create_banner(f"BUILD FAILED: {result.example}", red_text)
                print(f"\n{error_banner}")
                failed_examples.append(result.example)
            else:
                success_banner = create_banner(f"BUILD SUCCESS: {result.example}", green_text)
                print(f"\n{success_banner}")
                successful_examples.append(result.example)
    except KeyboardInterrupt:
        cancel_futures()
        print(f"\n{red_text('Build cancelled by user')}")
        return 1
    except Exception as e:
        error_banner = create_banner(f"BUILD ERROR: {e}", red_text)
        print(f"\n{error_banner}")
        cancel_futures()
        return 1

    # Print build summary
    total_examples = len(args.examples)
    successful_count = len(successful_examples)
    failed_count = len(failed_examples)
    
    if failed_examples:
        # Print failure summary
        failure_summary = create_banner(f"BUILD SUMMARY: {failed_count} FAILED, {successful_count} SUCCESS", red_text)
        print(f"\n{failure_summary}")
        print(f"\n{red_text('Failed examples:')}")
        for example in failed_examples:
            print(f"  {red_text('FAILED')}: {example}")
        
        if successful_examples:
            print(f"\n{green_text('Successful examples:')}")
            for example in successful_examples:
                print(f"  {green_text('SUCCESS')}: {example}")
        
        return 1  # Exit with failure code if any builds failed
    else:
        # All builds succeeded
        if total_examples > 1:
            final_banner = create_banner(f"ALL BUILDS SUCCESS: {total_examples} examples", green_text)
            print(f"\n{final_banner}")
        
        return 0


if __name__ == "__main__":
    sys.exit(main())
