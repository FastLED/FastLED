#!/usr/bin/env python3
"""Filter examples for stub platform using @filter directives.

This script checks each .ino file for @filter blocks and determines if it
should be compiled for the stub platform (host-based compilation).

Output format: SKIP:<example_name>:<reason>
Only outputs lines for examples that should be SKIPPED.

Usage:
    python ci/meson/filter_examples_for_stub.py <examples_dir>
"""

import sys
from pathlib import Path


# Add project root to sys.path for imports
project_root = Path(__file__).resolve().parent.parent.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

from ci.boards import Board
from ci.compiler.sketch_filter import parse_filter_from_sketch, should_skip_sketch


def create_stub_board() -> Board:
    """Create a Board object representing the stub platform.

    Returns:
        Board object with stub platform properties
    """
    # Create a minimal Board-like object for stub platform
    # The stub platform has these characteristics:
    # - platform_family: "stub" or "native"
    # - memory_class: "high" (host has plenty of memory)
    # - board_name: "stub"
    # - target: none or "native"

    class StubBoard:
        """Minimal Board representation for stub platform."""

        platform_family = "native"
        memory_class = "high"
        board_name = "stub"

        def get_mcu_target(self) -> str:
            return "native"

    return StubBoard()  # type: ignore


def main() -> int:
    """Main entry point."""
    if len(sys.argv) != 2:
        print(
            "Usage: python ci/meson/filter_examples_for_stub.py <examples_dir>",
            file=sys.stderr,
        )
        return 1

    examples_dir = Path(sys.argv[1]).resolve()
    if not examples_dir.exists():
        print(f"Error: Examples directory not found: {examples_dir}", file=sys.stderr)
        return 1

    # Create stub board representation
    stub_board = create_stub_board()

    # Find all .ino files
    ino_files = sorted(examples_dir.rglob("*.ino"))

    # Check each example against @filter directives
    for ino_file in ino_files:
        example_name = ino_file.stem

        try:
            # Parse @filter block from .ino file
            sketch_filter = parse_filter_from_sketch(ino_file)

            # Check if this example should be skipped for stub platform
            should_skip, reason = should_skip_sketch(stub_board, sketch_filter)

            if should_skip:
                # Output skip directive for Meson
                print(f"SKIP:{example_name}:{reason}")
        except KeyboardInterrupt:
            import _thread

            # Raise KeyboardInterrupt to stop the script
            _thread.interrupt_main()
            return 1

        except Exception as e:
            # If filter parsing fails, warn but don't skip
            # (Better to attempt compilation than to silently exclude)
            print(
                f"Warning: Error parsing filter for {example_name}: {e}",
                file=sys.stderr,
            )
            continue

    return 0


if __name__ == "__main__":
    sys.exit(main())
