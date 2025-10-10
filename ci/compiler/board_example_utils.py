"""Board and Example Management Utilities.

This module provides utility functions for managing boards and examples
in the FastLED compilation system.
"""

from pathlib import Path
from typing import List

from ci.boards import ALL, Board


def get_default_boards() -> List[str]:
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
        "digix",
        "rpipico",
        "rpipico2",
    ]

    # Get all available board names from the ALL list
    available_board_names = {board.board_name for board in ALL}

    # Start with preferred boards that exist, warn about missing ones
    default_boards: List[str] = []
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


def get_all_examples() -> List[str]:
    """Get all available example names from the examples directory.

    Returns:
        Sorted list of example names relative to examples directory
    """
    # Assume we're in ci/compiler/, go up two levels to project root
    project_root = Path(__file__).parent.parent.parent.resolve()
    examples_dir = project_root / "examples"

    if not examples_dir.exists():
        return []

    # Find all .ino files recursively
    ino_files = list(examples_dir.rglob("*.ino"))

    examples: List[str] = []
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
    # Assume we're in ci/compiler/, go up two levels to project root
    project_root = Path(__file__).parent.parent.parent.resolve()
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
