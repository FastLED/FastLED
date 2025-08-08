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

    # Check if build_info.json exists
    build_info_path = Path(".build") / args.board / "build_info.json"
    if not build_info_path.exists():
        # Fall back to nested pio structure
        build_info_path = Path(".build") / "pio" / args.board / "build_info.json"

    if not build_info_path.exists():
        message = (
            f"Build info not found at {build_info_path}. Skipping symbol analysis."
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

        # Override sys.argv to pass the board argument to the symbol analysis script
        original_argv = sys.argv
        sys.argv = ["symbol_analysis.py", "--board", args.board]

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
    except Exception as e:
        print(f"Symbol analysis failed: {e}")
        if args.skip_on_failure:
            return 0
        return 1


if __name__ == "__main__":
    sys.exit(main())
