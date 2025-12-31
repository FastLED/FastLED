#!/usr/bin/env python3
"""Arduino sketch path resolution utilities.

This module provides utilities for resolving Arduino sketch names and paths
into canonical project-relative paths. It supports various input formats:

Input formats:
    - Sketch name: 'RX' → 'examples/RX'
    - Relative path: 'examples/RX' → 'examples/RX'
    - Full path with .ino: 'examples/RX/RX.ino' → 'examples/RX'
    - Absolute path: '/full/path/to/examples/RX' → 'examples/RX'
    - Deep nested: 'examples/deep/nested/Sketch' → 'examples/deep/nested/Sketch'

Disambiguation:
    - If multiple sketches have the same name, reports all matches and exits
    - User must provide full path to resolve ambiguity
"""

import sys
from pathlib import Path


def resolve_sketch_path(sketch_arg: str, project_dir: Path) -> str:
    """Resolve sketch argument to examples directory path.

    Handles various input formats and performs disambiguation when needed.

    Args:
        sketch_arg: Sketch name, relative path, or full path
        project_dir: Project root directory

    Returns:
        Resolved path relative to project root (e.g., 'examples/RX')

    Raises:
        SystemExit: If sketch cannot be found or is ambiguous
    """
    # Handle full paths
    sketch_path = Path(sketch_arg)
    if sketch_path.is_absolute():
        # Convert absolute path to relative from project root
        try:
            relative_path = sketch_path.relative_to(project_dir)
            # If it's a .ino file, get the parent directory
            if relative_path.suffix == ".ino":
                relative_path = relative_path.parent
            return str(relative_path).replace("\\", "/")
        except ValueError:
            print(f"❌ Error: Sketch path is outside project directory: {sketch_arg}")
            print(f"   Project directory: {project_dir}")
            sys.exit(1)

    # Handle relative paths or sketch names
    sketch_str = str(sketch_path).replace("\\", "/")

    # Strip .ino extension if present
    if sketch_str.endswith(".ino"):
        sketch_str = str(Path(sketch_str).parent).replace("\\", "/")

    # If already starts with 'examples/', use as-is
    if sketch_str.startswith("examples/"):
        candidate = project_dir / sketch_str
        if candidate.is_dir():
            return sketch_str
        print(f"❌ Error: Sketch directory not found: {sketch_str}")
        print(f"   Expected directory: {candidate}")
        sys.exit(1)

    # Search for sketch in examples directory
    examples_dir = project_dir / "examples"
    if not examples_dir.exists():
        print(f"❌ Error: examples directory not found: {examples_dir}")
        sys.exit(1)

    # Find all matching directories
    sketch_name = sketch_str.split("/")[-1]  # Get the sketch name
    matches = list(examples_dir.rglob(f"*/{sketch_name}")) + list(
        examples_dir.glob(sketch_name)
    )

    # Filter to directories only
    matches = [m for m in matches if m.is_dir()]

    if len(matches) == 0:
        print(f"❌ Error: Sketch not found: {sketch_arg}")
        print(f"   Searched in: {examples_dir}")
        sys.exit(1)
    elif len(matches) > 1:
        print(
            f"❌ Error: Ambiguous sketch name '{sketch_arg}'. Multiple matches found:"
        )
        for match in matches:
            rel_path = match.relative_to(project_dir)
            print(f"   - {rel_path}")
        print("\n   Please specify the full path to resolve ambiguity.")
        sys.exit(1)

    # Single match found
    resolved = matches[0].relative_to(project_dir)
    return str(resolved).replace("\\", "/")


def parse_timeout(timeout_str: str) -> int:
    """Parse timeout string with optional suffix into seconds.

    Supported formats:
        - Plain number: "120" → 120 seconds
        - Milliseconds: "5000ms" → 5 seconds
        - Seconds: "120s" → 120 seconds
        - Minutes: "2m" → 120 seconds

    Args:
        timeout_str: Timeout string (e.g., "120", "2m", "5000ms")

    Returns:
        Timeout in seconds (integer)

    Raises:
        ValueError: If format is invalid or value is not positive
    """
    import re

    timeout_str = timeout_str.strip()

    # Match number with optional suffix
    match = re.match(r"^(\d+(?:\.\d+)?)\s*(ms|s|m)?$", timeout_str, re.IGNORECASE)
    if not match:
        raise ValueError(
            f"Invalid timeout format: '{timeout_str}'. "
            f"Expected formats: '120', '120s', '2m', '5000ms'"
        )

    value_str, suffix = match.groups()
    value = float(value_str)

    if value <= 0:
        raise ValueError(f"Timeout must be positive, got: {value}")

    # Convert to seconds
    if suffix is None or suffix.lower() == "s":
        # Default is seconds
        seconds = value
    elif suffix.lower() == "ms":
        # Milliseconds to seconds
        seconds = value / 1000
    elif suffix.lower() == "m":
        # Minutes to seconds
        seconds = value * 60
    else:
        # Should never reach here due to regex
        raise ValueError(f"Unknown suffix: {suffix}")

    # Return as integer (round up to ensure at least 1 second for small values)
    return max(1, int(seconds))
