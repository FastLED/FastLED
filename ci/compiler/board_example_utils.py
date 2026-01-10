"""Board and Example Management Utilities.

This module provides utility functions for managing boards and examples
in the FastLED compilation system.
"""

import os
from pathlib import Path

from ci.boards import ALL, Board
from ci.compiler.sketch_filter import parse_filter_from_sketch, should_skip_sketch


def _get_project_root() -> Path:
    """Get the FastLED project root, handling both native and Docker environments.

    Returns:
        Path to the FastLED project root directory
    """
    # Detect if running inside Docker container
    if os.environ.get("FASTLED_DOCKER") == "1":
        return Path("/fastled")
    else:
        # Assume we're in ci/compiler/, go up two levels to project root
        return Path(__file__).parent.parent.parent.resolve()


def get_default_boards() -> list[str]:
    """Get all board names from the ALL boards list, with preferred boards first.

    Returns:
        List of board names in preferred order
    """
    # These are the boards we want to compile first (preferred order)
    # Order matters: UNO first because it's used for global init and builds faster
    preferred_board_names = [
        "uno",  # Build is faster if this is first, because it's used for global init.
        "esp32dev",
        "esp8266",  # ESP8266
        "esp32c3",
        "esp32c6",
        "esp32s3",
        "teensylc",
        "teensy31",
        "teensy41",
        "sam3x8e_due",
        "rp2040",
        "rp2350",
    ]

    # Get all available board names from the ALL list
    available_board_names = {board.board_name for board in ALL}

    # Start with preferred boards that exist, warn about missing ones
    default_boards: list[str] = []
    for board_name in preferred_board_names:
        if board_name in available_board_names:
            default_boards.append(board_name)
        else:
            print(
                f"WARNING: Preferred board '{board_name}' not found in available boards"
            )

    # Add all remaining boards (sorted for consistency)
    remaining_boards = sorted(available_board_names - set(default_boards))
    default_boards.extend(remaining_boards)

    return default_boards


def get_all_examples() -> list[str]:
    """Get all available example names from the examples directory.

    Returns:
        Sorted list of example names relative to examples directory
    """
    project_root = _get_project_root()
    examples_dir = project_root / "examples"

    if not examples_dir.exists():
        return []

    # Find all .ino files recursively
    ino_files = list(examples_dir.rglob("*.ino"))

    examples: list[str] = []
    for ino_file in ino_files:
        # Get the parent directory relative to examples/
        # For examples/Blink/Blink.ino -> "Blink"
        # For examples/Fx/FxWave2d/FxWave2d.ino -> "Fx/FxWave2d"
        example_dir = ino_file.parent.relative_to(examples_dir)
        example_name = str(example_dir).replace("\\", "/")  # Normalize path separators
        examples.append(example_name)

    # Sort for consistent ordering
    examples.sort()
    return examples


def resolve_example_path(example: str) -> str:
    """Resolve example name to path, ensuring it exists.

    Args:
        example: Example name (e.g., "Blink" or "examples/Blink")

    Returns:
        Normalized example name without "examples/" prefix

    Raises:
        FileNotFoundError: If example directory does not exist
    """
    project_root = _get_project_root()
    examples_dir = project_root / "examples"

    # Handle both "Blink" and "examples/Blink" formats
    if example.startswith("examples/"):
        example = example[len("examples/") :]

    example_path = examples_dir / example
    if not example_path.exists():
        raise FileNotFoundError(f"Example not found: {example_path}")

    return example


def get_board_artifact_extension(board: Board) -> str:
    """Get the primary artifact extension for a board.

    Args:
        board: Board configuration

    Returns:
        File extension including the dot (e.g., ".bin", ".hex")
    """
    # ESP32/ESP8266 boards always produce .bin files
    if board.board_name.startswith("esp"):
        return ".bin"

    # Most Arduino-based boards produce .hex files
    if board.framework and "arduino" in board.framework.lower():
        return ".hex"

    # Default to .hex for most microcontroller boards
    return ".hex"


def get_example_ino_path(example: str) -> Path:
    """Get the path to the .ino file for an example.

    Args:
        example: Example name (e.g., "Blink" or "Fx/FxWave2d")

    Returns:
        Path to the .ino file

    Raises:
        FileNotFoundError: If .ino file not found or multiple .ino files found
    """
    project_root = _get_project_root()
    examples_dir = project_root / "examples"

    # Handle both "Blink" and "examples/Blink" formats
    if example.startswith("examples/"):
        example = example[len("examples/") :]

    # Find the .ino file in the example directory
    example_dir = examples_dir / example
    example_name = Path(
        example
    ).name  # Get just the last part (e.g., "Blink" or "FxWave2d")
    ino_file = example_dir / f"{example_name}.ino"

    if ino_file.exists():
        return ino_file

    # If the expected filename doesn't exist, search for any .ino file in the directory
    if example_dir.exists() and example_dir.is_dir():
        ino_files = list(example_dir.glob("*.ino"))
        if len(ino_files) == 1:
            # Exactly one .ino file found, use it
            return ino_files[0]
        elif len(ino_files) > 1:
            # Multiple .ino files - ambiguous, raise error
            raise FileNotFoundError(
                f"Example directory '{example}' contains multiple .ino files: "
                f"{[f.name for f in ino_files]}. Cannot determine which to use."
            )
        # else: no .ino files found, will raise below

    raise FileNotFoundError(f"Example .ino file not found: {ino_file}")


def should_skip_example_for_board(board: Board, example: str) -> tuple[bool, str]:
    """Check if an example should be skipped for a specific board.

    Parses the @filter/@end-filter block from the .ino file and determines
    if compilation should be skipped based on board properties.

    Args:
        board: Board configuration
        example: Example name

    Returns:
        Tuple of (skip: bool, reason: str)
        - skip=True means don't compile for this board
        - reason explains why (or empty string if not skipped)
    """
    try:
        ino_path = get_example_ino_path(example)
    except FileNotFoundError:
        # If we can't find the .ino file, don't skip
        return False, ""

    sketch_filter = parse_filter_from_sketch(ino_path)
    return should_skip_sketch(board, sketch_filter)


def get_filtered_examples(
    board: Board, examples: list[str]
) -> tuple[list[str], list[tuple[str, str]]]:
    """Filter examples based on board capabilities.

    Args:
        board: Board configuration
        examples: List of example names to filter

    Returns:
        Tuple of (included_examples, skipped_with_reasons)
        - included_examples: List of examples that should be compiled
        - skipped_with_reasons: List of (example_name, skip_reason) tuples
    """
    included: list[str] = []
    skipped: list[tuple[str, str]] = []

    for example in examples:
        should_skip, reason = should_skip_example_for_board(board, example)
        if should_skip:
            skipped.append((example, reason))
        else:
            included.append(example)

    return included, skipped
