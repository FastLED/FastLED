from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Generic Symbol Analysis Runner for GitHub Actions
Wrapper script to run symbol analysis with proper error handling for any platform
"""

import argparse
import sys
from pathlib import Path

from ci.util.symbol_analysis import main as symbol_analysis


def main():
    parser = argparse.ArgumentParser(
        description="Run symbol analysis for any platform with GitHub Actions support"
    )
    parser.add_argument("--board", required=True, help="Board name being analyzed")
    parser.add_argument("--example", default="Blink", help="Example that was compiled")
    parser.add_argument(
        "--skip-on-failure",
        action="store_true",
        help="Don't fail the build if symbol analysis fails",
    )
    args = parser.parse_args()

    print(f"Symbol Analysis Runner - Board: {args.board}, Example: {args.example}")

    # Build list of all paths to check (in priority order)
    build_info_filename = f"build_info_{args.example}.json"
    paths_to_check = [
        Path(".build") / args.board / build_info_filename,
        Path(".build") / "pio" / args.board / build_info_filename,
        Path(".build") / args.board / "build_info.json",
        Path(".build") / "pio" / args.board / "build_info.json",
    ]

    # Find first existing path
    build_info_path = None
    for path in paths_to_check:
        if path.exists():
            build_info_path = path
            break

    if build_info_path is None:
        # Show all paths that were checked
        checked_paths = "\n  - ".join(str(p) for p in paths_to_check)
        message = (
            f"Build info not found. Checked the following paths:\n  - {checked_paths}\n"
            f"This may indicate the compilation did not complete successfully. "
            f"Check the compilation logs for warnings about build_info generation."
        )
        if args.skip_on_failure:
            print(f"Warning: {message}")
            return 0
        else:
            print(f"Error: {message}")
            return 1

    try:
        # Import and run the generic symbol analysis
        print("Running symbol analysis...")

        # Override sys.argv to pass the board and example arguments to the symbol analysis script
        original_argv = sys.argv
        sys.argv = [
            "symbol_analysis.py",
            "--board",
            args.board,
            "--example",
            args.example,
        ]

        try:
            symbol_analysis()
            print("Symbol analysis completed successfully!")
            return 0
        finally:
            sys.argv = original_argv

    except ImportError as e:
        print(f"Failed to import symbol_analysis module: {e}")
        if args.skip_on_failure:
            return 0
        return 1
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Symbol analysis failed: {e}")
        if args.skip_on_failure:
            return 0
        return 1


if __name__ == "__main__":
    sys.exit(main())
