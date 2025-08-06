#!/usr/bin/env python3
"""
FastLED Target Build Script

Convenience script to run a full build test on any target platform using
the new PlatformIO testing system.
"""

import argparse
import sys
from dataclasses import dataclass

from ci.compiler.pio import PlatformIoBuilder, BuildResult


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

    # Create PlatformIO builder with only the parameters it needs
    builder = PlatformIoBuilder(board=args.board, verbose=args.verbose)
    
    # Initialize build environment
    init_result = builder.init_build()
    if not init_result.success:
        print(f"Failed to initialize build: {init_result.output}")
        return 1

    results: list[BuildResult] = []
    for example in args.examples:
        print(f"Building example: {example}")
        
        # Build the example
        result = builder.build(example)
        results.append(result)
        if not result.success:
            print(f"Failed to build {example}")
        else:
            print(f"Successfully built {example}")

    failed_builds = [(args.examples[i], result) for i, result in enumerate(results) if not result.success]
    if failed_builds:
        print(f"Failed to build {len(failed_builds)} out of {len(results)} examples")
        for example_name, result in failed_builds:
            print(f"Error building {example_name}: {result.output}")
        return 1

    print(f"Successfully built all {len(results)} examples")
    return 0


if __name__ == "__main__":
    sys.exit(main())
