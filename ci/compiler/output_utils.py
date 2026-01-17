from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""Output management utilities for FastLED compilation.

This module handles validation and copying of build artifacts to output locations.
"""

from pathlib import Path

from ci.boards import Board
from ci.compiler.board_example_utils import get_board_artifact_extension
from ci.compiler.esp32_artifacts import ESP32ArtifactManager


def validate_output_path(
    output_path: str, sketch_name: str, board: Board
) -> tuple[bool, str, str]:
    """Validate output path and return (is_valid, resolved_path, error_message).

    Args:
        output_path: The user-specified output path
        sketch_name: Name of the sketch being built
        board: Board configuration

    Returns:
        Tuple of (is_valid, resolved_output_path, error_message)
    """
    import os

    expected_ext = get_board_artifact_extension(board)

    # Handle special case: -o .
    if output_path == ".":
        resolved_path = f"{sketch_name}{expected_ext}"
        return True, resolved_path, ""

    # If path ends with /, it's a directory
    if output_path.endswith("/") or output_path.endswith("\\"):
        resolved_path = os.path.join(output_path, f"{sketch_name}{expected_ext}")
        return True, resolved_path, ""

    # If path has an extension, it's a file - validate the extension
    if "." in os.path.basename(output_path):
        _, ext = os.path.splitext(output_path)
        if ext != expected_ext:
            return (
                False,
                "",
                f"Output file extension '{ext}' doesn't match expected '{expected_ext}' for board '{board.board_name}'",
            )
        return True, output_path, ""

    # Path doesn't end with / and has no extension - treat as directory
    resolved_path = os.path.join(output_path, f"{sketch_name}{expected_ext}")
    return True, resolved_path, ""


def validate_esp32_flash_mode_for_qemu(build_dir: Path, board: Board) -> bool:
    """Validate ESP32 flash mode for QEMU compatibility.

    QEMU requires DIO/80MHz flash mode, not QIO mode.

    Args:
        build_dir: Build directory path
        board: Board configuration

    Returns:
        True if flash mode is compatible or successfully validated
    """
    if not board.board_name.startswith("esp32"):
        return True  # Not an ESP32 board, no validation needed

    try:
        # Check platformio.ini for flash mode settings
        platformio_ini = build_dir / "platformio.ini"
        if platformio_ini.exists():
            content = platformio_ini.read_text()

            # Look for flash mode settings in build_flags
            if "FLASH_MODE=qio" in content.upper():
                print("⚠️  WARNING: QIO flash mode detected in platformio.ini")
                print("   QEMU requires DIO flash mode for compatibility")
                print("   Consider using DIO mode for QEMU builds")
                return True  # Warning but not blocking

            if "FLASH_MODE=dio" in content.upper():
                print("✅ DIO flash mode detected - compatible with QEMU")
                return True

        # Check if build artifacts exist to examine flash settings
        artifact_dir = build_dir / ".pio" / "build" / board.board_name
        if artifact_dir.exists():
            # Look for flash_args file which contains esptool flash arguments
            flash_args_file = artifact_dir / "flash_args"
            if flash_args_file.exists():
                flash_args = flash_args_file.read_text()
                if "--flash_mode qio" in flash_args:
                    print("⚠️  WARNING: QIO flash mode detected in build artifacts")
                    print("   QEMU may not boot properly with QIO mode")
                elif "--flash_mode dio" in flash_args:
                    print(
                        "✅ DIO flash mode detected in build artifacts - QEMU compatible"
                    )

        print(f"ℹ️  Flash mode validation complete for {board.board_name}")
        return True

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"WARNING: Could not validate ESP32 flash mode: {e}")
        return True  # Don't fail build for validation issues


def copy_esp32_qemu_artifacts(
    build_dir: Path, board: Board, sketch_name: str, output_path: str
) -> bool:
    """Copy all ESP32 QEMU artifacts to the specified output directory.

    Args:
        build_dir: Build directory path
        board: Board configuration
        sketch_name: Name of the sketch
        output_path: Target output path (directory or firmware.bin file)

    Returns:
        True if successful, False otherwise
    """
    print("🔧 ESP32 QEMU mode detected - collecting all required artifacts")

    # Validate flash mode for QEMU compatibility
    validate_esp32_flash_mode_for_qemu(build_dir, board)

    # Use the new ESP32ArtifactManager for clean artifact handling
    manager = ESP32ArtifactManager(build_dir, board.board_name)
    return manager.copy_qemu_artifacts(output_path, sketch_name)


def copy_build_artifact(
    build_dir: Path, board: Board, sketch_name: str, output_path: str
) -> bool:
    """Copy the build artifact to the specified output path.

    For ESP32 boards with QEMU-style output paths, this will copy all required
    QEMU artifacts. For other cases, copies only the main firmware file.

    Args:
        build_dir: Build directory path
        board: Board configuration
        sketch_name: Name of the sketch
        output_path: Target output path

    Returns:
        True if successful, False otherwise
    """
    import shutil

    # Detect ESP32 QEMU mode
    is_esp32 = board.board_name.startswith("esp32")
    is_qemu_mode = (
        "qemu" in output_path.lower()
        or output_path.endswith("/")
        or output_path.endswith("\\")
    )

    if is_esp32 and is_qemu_mode:
        return copy_esp32_qemu_artifacts(build_dir, board, sketch_name, output_path)

    # Original single-file copy logic for non-QEMU cases
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
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"ERROR: Failed to copy build artifact: {e}")
        return False
