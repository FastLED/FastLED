#!/usr/bin/env python3
"""
Discover all Arduino examples (.ino files) in the examples directory.

This script provides example discovery for test.py integration and validation.
It returns a simple list of example names (stems) that can be used for filtering
and validation of user-provided example names.

Output format: One example name per line (just the stem, not the full path).

Usage:
    python ci/discover_examples.py [examples_dir]

If examples_dir is not provided, defaults to 'examples' relative to project root.
"""

import sys
from pathlib import Path


def discover_examples(examples_dir: Path) -> list[str]:
    """
    Discover all .ino files in the examples directory.

    Args:
        examples_dir: Path to examples directory

    Returns:
        List of example names (stems) sorted alphabetically
    """
    if not examples_dir.exists():
        print(f"Error: Examples directory not found: {examples_dir}", file=sys.stderr)
        return []

    # Find all .ino files recursively
    ino_files = sorted(examples_dir.rglob("*.ino"))

    # Extract stems (example names)
    example_names = [ino_file.stem for ino_file in ino_files]

    return sorted(set(example_names))  # Remove duplicates and sort


def main() -> int:
    """Main entry point."""
    # Determine examples directory
    if len(sys.argv) > 1:
        examples_dir = Path(sys.argv[1]).resolve()
    else:
        # Default to examples/ relative to script location
        script_dir = Path(__file__).parent
        project_root = script_dir.parent
        examples_dir = project_root / "examples"

    # Discover examples
    examples = discover_examples(examples_dir)

    # Output one per line
    for example_name in examples:
        print(example_name)

    return 0


if __name__ == "__main__":
    sys.exit(main())
