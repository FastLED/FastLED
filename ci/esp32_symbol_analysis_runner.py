#!/usr/bin/env python3
"""
ESP32 Symbol Analysis Runner for GitHub Actions
Wrapper script to run ESP32 symbol analysis with proper error handling
"""

import argparse
import os
import sys
from pathlib import Path

from ci.util.esp32_symbol_analysis import main as esp32_symbol_analysis


def is_esp32_board(board_name: str) -> bool:
    """Check if the board is ESP32-based"""
    esp32_boards = [
        "esp32dev",
        "esp32",
        "esp32s2",
        "esp32s3",
        "esp32c3",
        "esp32c6",
        "esp32h2",
        "esp32p4",
        "esp32c2",
    ]
    return any(esp32_board in board_name.lower() for esp32_board in esp32_boards)


def main():
    parser = argparse.ArgumentParser(
        description="Run ESP32 symbol analysis for GitHub Actions"
    )
    parser.add_argument("--board", required=True, help="Board name being analyzed")
    parser.add_argument("--example", default="Blink", help="Example that was compiled")
    parser.add_argument(
        "--skip-on-failure",
        action="store_true",
        help="Don't fail the build if symbol analysis fails",
    )
    args = parser.parse_args()

    print(
        f"ESP32 Symbol Analysis Runner - Board: {args.board}, Example: {args.example}"
    )

    # Only run for ESP32 boards
    if not is_esp32_board(args.board):
        print(f"Skipping symbol analysis - {args.board} is not an ESP32-based board")
        return 0

    # Check if build_info.json exists
    build_info_path = Path(".build") / args.board / "build_info.json"
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
        # TODO - Remove this.
        # Change to the ci/util directory since the script expects to be run from there
        original_cwd = os.getcwd()
        ci_ci_dir = Path(__file__).parent / "ci"
        os.chdir(ci_ci_dir)

        # Import and run the ESP32 symbol analysis
        try:
            print("Running ESP32 symbol analysis...")
            esp32_symbol_analysis()
            print("ESP32 symbol analysis completed successfully!")
            return 0
        except ImportError as e:
            print(f"Failed to import esp32_symbol_analysis module: {e}")
            if args.skip_on_failure:
                return 0
            return 1
        except Exception as e:
            print(f"ESP32 symbol analysis failed: {e}")
            if args.skip_on_failure:
                return 0
            return 1
        finally:
            os.chdir(original_cwd)

    except Exception as e:
        print(f"Unexpected error in ESP32 symbol analysis runner: {e}")
        if args.skip_on_failure:
            return 0
        return 1


if __name__ == "__main__":
    sys.exit(main())
