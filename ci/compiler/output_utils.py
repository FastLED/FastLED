"""Output management utilities for FastLED compilation.

This module handles validation and copying of build artifacts to output locations.
"""

from dataclasses import dataclass
from pathlib import Path

from ci.boards import Board
from ci.compiler.board_example_utils import get_board_artifact_extension
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


@dataclass(slots=True)
class ValidateOutputPathResult:
    """Result of validate_output_path."""

    is_valid: bool
    resolved_path: str
    error_message: str


def validate_output_path(
    output_path: str, sketch_name: str, board: Board
) -> ValidateOutputPathResult:
    """Validate output path and return a result with is_valid, resolved_path, error_message.

    Args:
        output_path: The user-specified output path
        sketch_name: Name of the sketch being built
        board: Board configuration

    Returns:
        ValidateOutputPathResult with is_valid, resolved_path, and error_message fields
    """
    import os

    expected_ext = get_board_artifact_extension(board)

    # Handle special case: -o .
    if output_path == ".":
        resolved_path = f"{sketch_name}{expected_ext}"
        return ValidateOutputPathResult(True, resolved_path, "")

    # If path ends with /, it's a directory
    if output_path.endswith("/") or output_path.endswith("\\"):
        resolved_path = os.path.join(output_path, f"{sketch_name}{expected_ext}")
        return ValidateOutputPathResult(True, resolved_path, "")

    # If path has an extension, it's a file - validate the extension
    if "." in os.path.basename(output_path):
        _, ext = os.path.splitext(output_path)
        if ext != expected_ext:
            return ValidateOutputPathResult(
                False,
                "",
                f"Output file extension '{ext}' doesn't match expected '{expected_ext}' for board '{board.board_name}'",
            )
        return ValidateOutputPathResult(True, output_path, "")

    # Path doesn't end with / and has no extension - treat as directory
    resolved_path = os.path.join(output_path, f"{sketch_name}{expected_ext}")
    return ValidateOutputPathResult(True, resolved_path, "")


def copy_build_artifact(
    build_dir: Path, board: Board, sketch_name: str, output_path: str
) -> bool:
    """Copy the build artifact to the specified output path.

    Args:
        build_dir: Build directory path
        board: Board configuration
        sketch_name: Name of the sketch
        output_path: Target output path

    Returns:
        True if successful, False otherwise
    """
    import shutil

    expected_ext = get_board_artifact_extension(board)

    # Find the source artifact
    # PlatformIO builds are in .build/pio/{board}/.pio/build/{board}/firmware.{ext}
    artifact_dir = build_dir / ".pio" / "build" / board.board_name
    source_artifact = artifact_dir / f"firmware{expected_ext}"

    if not source_artifact.exists():
        print(f"ERROR: Build artifact not found: {source_artifact}")
        return False

    # Ensure output directory exists
    output_path_obj = Path(output_path)
    output_path_obj.parent.mkdir(parents=True, exist_ok=True)

    try:
        print(f"Copying {source_artifact} to {output_path}")
        shutil.copy2(source_artifact, output_path)
        print(f"✅ Build artifact saved to: {output_path}")
        return True
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"ERROR: Failed to copy build artifact: {e}")
        return False
