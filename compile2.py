#!/usr/bin/env python3
"""
FastLED Target Build Script

Convenience script to run a full build test on any target platform using
the new PlatformIO testing system.
"""

import argparse
import sys
from dataclasses import dataclass

from ci.compiler.pio import PlatformIoBuilder, BuildResult, run_pio_build


@dataclass
class Args:
    board: str
    examples: list[str]
    verbose: bool

    @staticmethod
    def parse_args() -> "Args":
        parser = argparse.ArgumentParser(description="FastLED Target Build Script")
        parser.add_argument(
            "board", help="Board to build for (required)"
        )
        parser.add_argument(
            "examples", nargs="+", help="One or more examples to build (required)"
        )
        parser.add_argument(
            "--verbose", action="store_true", help="Enable verbose output"
        )
        return Args(**vars(parser.parse_args()))


def main() -> int:
    """Main entry point."""

    args = Args.parse_args()

    # run_pio_build now accepts both Board objects and strings
    # The string will be automatically resolved to a Board object via get_board()
    futures = run_pio_build(args.board, args.examples, args.verbose)


    for future in futures:
        result: BuildResult = future.result()
        if not result.success:
            print(f"Failed to build {result.example}: {result.output}")
            for future in futures:
                future.cancel()
            return 1
        else:
            print(f"Successfully built {result.example}")
    

    print(f"Successfully built all {len(args.examples)} examples")
    return 0


if __name__ == "__main__":
    sys.exit(main())
