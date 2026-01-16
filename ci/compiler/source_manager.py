"""Source and example management for FastLED PlatformIO builds."""

import os
import shutil
import sys
import time
import warnings
from pathlib import Path

from dirsync import sync  # type: ignore

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


def generate_main_cpp(ino_files: list[str]) -> str:
    """Generate stub main.cpp content that includes .ino files from sketch directory.

    Args:
        ino_files: List of .ino filenames to include

    Returns:
        Content for main.cpp file
    """
    includes: list[str] = []
    for ino_file in sorted(ino_files):
        includes.append("#include <Arduino.h>")
        includes.append(f'#include "sketch/{ino_file}"')

    include_lines = "\n".join(includes)

    int_main = """
void init(void) __attribute__((weak));

__attribute__((weak)) int main() {{
    init();  // Initialize Arduino timers and enable interrupts (calls sei())
    setup();
    while (true) {{
        loop();
        delay(0);  // Yield to scheduler/watchdog (critical for ESP32 QEMU)
    }}
}}
"""

    main_cpp_content = f"""// Auto-generated main.cpp stub for PlatformIO
// This file includes all .ino files from the sketch directory

{include_lines}

// main.cpp is required by PlatformIO but Arduino-style sketches
// use setup() and loop() functions which are called automatically
// by the FastLED/Arduino framework
//
//
{int_main}
"""
    return main_cpp_content


def copy_example_source(project_root: Path, build_dir: Path, example: str) -> bool:
    """Copy example source to the build directory with sketch subdirectory structure.

    Args:
        project_root: FastLED project root directory
        build_dir: Build directory for the target
        example: Name of the example to copy, or path to example directory

    Returns:
        True if successful, False if example not found
    """
    # Configure example source - handle both names and paths
    example_path = Path(example)

    if example_path.is_absolute():
        # Absolute path - use as-is
        if not example_path.exists():
            return False
    else:
        # Relative path (including nested paths like Fx/FxWave2d) - resolve to examples directory
        example_path = project_root / "examples" / example
        if not example_path.exists():
            return False

    # Create src and sketch directories (PlatformIO requirement with sketch subdirectory)
    src_dir = build_dir / "src"
    sketch_dir = src_dir / "sketch"

    # Create directories if they don't exist, but don't remove existing src_dir
    src_dir.mkdir(exist_ok=True)

    # Clean and recreate sketch subdirectory for fresh .ino files
    if sketch_dir.exists():
        shutil.rmtree(sketch_dir)
    sketch_dir.mkdir(parents=True, exist_ok=True)

    # Copy all files and subdirectories from example directory to sketch subdirectory
    ino_files: list[str] = []
    for file_path in example_path.iterdir():
        if "fastled_js" in str(file_path):
            # skip fastled_js output folder.
            continue

        if file_path.is_file():
            shutil.copy2(file_path, sketch_dir)
            # Calculate relative paths for cleaner output
            try:
                rel_source = file_path.relative_to(Path.cwd())
                rel_dest = sketch_dir.relative_to(Path.cwd())
                print(f"Copied {rel_source} to {rel_dest}")
            except ValueError:
                # Fallback to absolute paths if relative calculation fails
                print(f"Copied {file_path} to {sketch_dir}")
            if file_path.suffix == ".ino":
                ino_files.append(file_path.name)
        elif file_path.is_dir():
            # Recursively sync subdirectories for better caching
            dest_subdir = sketch_dir / file_path.name
            dest_subdir.mkdir(parents=True, exist_ok=True)
            # Only pass content=True in Docker environments (content-based comparison)
            # On other systems, use default behavior (timestamp/size) by omitting the parameter
            # On Windows, add retry logic for file locking issues
            max_retries = 3 if sys.platform == "win32" else 1
            retry_delay = 0.5  # seconds

            for attempt in range(max_retries):
                try:
                    if os.environ.get("DOCKER_CONTAINER", "") != "":
                        sync(
                            str(file_path),
                            str(dest_subdir),
                            "sync",
                            purge=True,
                            content=True,
                        )
                    else:
                        sync(str(file_path), str(dest_subdir), "sync", purge=True)
                    break  # Success, exit retry loop
                except OSError as e:
                    # Handle Windows file locking errors (WinError 32)
                    if sys.platform == "win32" and attempt < max_retries - 1:
                        print(
                            f"File access error syncing example subdirectory, retrying ({attempt + 1}/{max_retries}): {e}"
                        )
                        time.sleep(retry_delay)
                    else:
                        raise

            try:
                rel_source = file_path.relative_to(Path.cwd())
                rel_dest = dest_subdir.relative_to(Path.cwd())
                print(f"Synced directory {rel_source} to {rel_dest}")
            except ValueError:
                print(f"Synced directory {file_path} to {dest_subdir}")

    # Create or update stub main.cpp that includes the .ino files
    main_cpp_content = generate_main_cpp(ino_files)
    main_cpp_path = src_dir / "main.cpp"

    # Only write main.cpp if content has changed to avoid triggering rebuilds
    should_write = True
    if main_cpp_path.exists():
        try:
            existing_content = main_cpp_path.read_text(encoding="utf-8")
            should_write = existing_content != main_cpp_content
        except (OSError, UnicodeDecodeError):
            # If we can't read the existing file, write new content
            should_write = True

    if should_write:
        main_cpp_path.write_text(main_cpp_content, encoding="utf-8")

    return True


def copy_boards_directory(project_root: Path, build_dir: Path) -> bool:
    """Copy boards directory to the build directory."""
    boards_src = project_root / "ci" / "boards"
    boards_dst = build_dir / "boards"

    if not boards_src.exists():
        warnings.warn(f"Boards directory not found: {boards_src}")
        return False

    try:
        # Ensure target directory exists for dirsync
        boards_dst.mkdir(parents=True, exist_ok=True)

        # Use sync for better caching - purge=True removes extra files
        # Only pass content=True in Docker environments (content-based comparison)
        # On other systems, use default behavior (timestamp/size) by omitting the parameter
        # On Windows, add retry logic for file locking issues
        max_retries = 3 if sys.platform == "win32" else 1
        retry_delay = 0.5  # seconds

        for attempt in range(max_retries):
            try:
                if os.environ.get("DOCKER_CONTAINER", "") != "":
                    sync(
                        str(boards_src),
                        str(boards_dst),
                        "sync",
                        purge=True,
                        content=True,
                    )
                else:
                    sync(str(boards_src), str(boards_dst), "sync", purge=True)
                break  # Success, exit retry loop
            except OSError as e:
                # Handle Windows file locking errors (WinError 32)
                if sys.platform == "win32" and attempt < max_retries - 1:
                    print(
                        f"File access error during boards sync, retrying ({attempt + 1}/{max_retries}): {e}"
                    )
                    time.sleep(retry_delay)
                else:
                    raise
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        warnings.warn(f"Failed to sync boards directory: {e}")
        return False

    return True


def copy_fastled_library(project_root: Path, build_dir: Path) -> bool:
    """Copy FastLED library to build directory with proper library.json structure."""
    lib_dir = build_dir / "lib" / "FastLED"
    lib_parent = build_dir / "lib"

    # Copy src/ directory into lib/FastLED using dirsync for better caching
    fastled_src_path = project_root / "src"
    try:
        # Create lib directory if it doesn't exist
        lib_parent.mkdir(parents=True, exist_ok=True)

        # Try to remove existing FastLED directory if it exists
        # On Windows, files may be locked by antivirus or explorer, so handle gracefully
        if lib_dir.exists():
            if lib_dir.is_symlink():
                lib_dir.unlink()
            else:
                # On Windows, add retry logic for rmtree since files may still be locked
                # This can happen when antivirus software or explorer.exe still has handles open
                max_rmtree_retries = 5 if sys.platform == "win32" else 1
                rmtree_delay_ms = 100 if sys.platform == "win32" else 0

                for rmtree_attempt in range(max_rmtree_retries):
                    try:
                        shutil.rmtree(lib_dir)
                        break  # Success
                    except OSError as e:
                        if (
                            sys.platform == "win32"
                            and rmtree_attempt < max_rmtree_retries - 1
                        ):
                            delay_seconds = rmtree_delay_ms / 1000.0
                            time.sleep(delay_seconds)
                        elif rmtree_attempt == max_rmtree_retries - 1:
                            # On last attempt failure, just warn and continue with update instead
                            print(
                                f"Warning: Could not remove old FastLED library, will update existing: {e}"
                            )
                            break

        # Ensure target directory exists for dirsync
        lib_dir.mkdir(parents=True, exist_ok=True)

        # Use dirsync.sync for efficient incremental synchronization
        # Only pass content=True in Docker environments (content-based comparison)
        # On other systems, use default behavior (timestamp/size) by omitting the parameter
        # On Windows, add retry logic for file locking issues
        max_retries = 3 if sys.platform == "win32" else 1
        retry_delay = 0.5  # seconds

        for attempt in range(max_retries):
            try:
                if os.environ.get("DOCKER_CONTAINER", "") != "":
                    sync(
                        str(fastled_src_path),
                        str(lib_dir),
                        "sync",
                        purge=True,
                        content=True,
                    )
                else:
                    sync(str(fastled_src_path), str(lib_dir), "sync", purge=True)
                break  # Success, exit retry loop
            except OSError as e:
                # Handle Windows file locking errors (WinError 32)
                if sys.platform == "win32" and attempt < max_retries - 1:
                    print(
                        f"File access error during sync, retrying ({attempt + 1}/{max_retries}): {e}"
                    )
                    time.sleep(retry_delay)
                else:
                    raise

        # Copy library.json to the root of lib/FastLED
        library_json_src = project_root / "library.json"
        library_json_dst = lib_dir / "library.json"
        if library_json_src.exists():
            shutil.copy2(library_json_src, library_json_dst)

        # Calculate relative paths for cleaner output
        try:
            rel_lib_dir = lib_dir.relative_to(Path.cwd())
            rel_src_path = fastled_src_path.relative_to(Path.cwd())
            print(f"Synced FastLED library: {rel_src_path} -> {rel_lib_dir}")
            if library_json_src.exists():
                print(f"Copied library.json to {rel_lib_dir}")
        except ValueError:
            # Fallback to absolute paths if relative calculation fails
            print(f"Synced FastLED library to {lib_dir}")
            if library_json_src.exists():
                print(f"Copied library.json to {lib_dir}")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as sync_error:
        warnings.warn(f"Failed to sync FastLED library: {sync_error}")
        return False

    return True
