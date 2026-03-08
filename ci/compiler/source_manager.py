"""Source and example management for FastLED PlatformIO builds."""

import filecmp
import os
import shutil
import sys
import time
import warnings
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


def _scandir_sync(
    source: str, dest: str, purge: bool = True, content: bool = False
) -> None:
    """Fast directory sync using os.scandir instead of dirsync/os.walk.

    On Windows, DirEntry.stat() reuses FindFirstFile data (no extra syscall
    per file), making this significantly faster than dirsync which uses
    os.walk + os.stat per entry.

    Args:
        source: Source directory path
        dest: Destination directory path
        purge: If True, delete files in dest that don't exist in source
        content: If True, compare file contents (for Docker); otherwise use size+mtime
    """
    # Collect source entries: {relative_path: absolute_path}
    src_files: dict[str, str] = {}
    src_dirs: set[str] = set()
    src_len = len(source)
    if not source.endswith(os.sep):
        src_len += 1

    stack = [source]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as it:
                for entry in it:
                    try:
                        rel = entry.path[src_len:]
                        if entry.is_dir(follow_symlinks=False):
                            src_dirs.add(rel)
                            stack.append(entry.path)
                        elif entry.is_file(follow_symlinks=False):
                            src_files[rel] = entry.path
                    except OSError:
                        pass
        except OSError:
            pass

    # Ensure dest directory exists
    os.makedirs(dest, exist_ok=True)

    # Ensure subdirectories exist in dest
    for rel_dir in sorted(src_dirs):
        dest_dir = os.path.join(dest, rel_dir)
        os.makedirs(dest_dir, exist_ok=True)

    # Copy new/changed files
    for rel_path, src_abs in src_files.items():
        dest_abs = os.path.join(dest, rel_path)
        os.makedirs(os.path.dirname(dest_abs), exist_ok=True)

        if os.path.isfile(dest_abs):
            if content:
                # Content comparison (Docker mode) — use filecmp for speed
                if filecmp.cmp(src_abs, dest_abs, shallow=False):
                    continue
            else:
                # Size + mtime comparison
                try:
                    src_st = os.stat(src_abs)
                    dst_st = os.stat(dest_abs)
                    if (
                        src_st.st_size == dst_st.st_size
                        and abs(src_st.st_mtime - dst_st.st_mtime) < 0.001
                    ):
                        continue
                except OSError:
                    pass

        shutil.copy2(src_abs, dest_abs)

    # Purge files/dirs in dest that don't exist in source
    if purge:
        dest_len = len(dest)
        if not dest.endswith(os.sep):
            dest_len += 1
        # Collect dest entries
        dest_files_to_remove: list[str] = []
        dest_dirs_to_remove: list[str] = []
        d_stack = [dest]
        while d_stack:
            current = d_stack.pop()
            try:
                with os.scandir(current) as it:
                    for entry in it:
                        try:
                            rel = entry.path[dest_len:]
                            if entry.is_dir(follow_symlinks=False):
                                if rel not in src_dirs:
                                    dest_dirs_to_remove.append(entry.path)
                                else:
                                    d_stack.append(entry.path)
                            elif entry.is_file(follow_symlinks=False):
                                if rel not in src_files:
                                    dest_files_to_remove.append(entry.path)
                        except OSError:
                            pass
            except OSError:
                pass

        for f in dest_files_to_remove:
            try:
                os.remove(f)
            except OSError:
                pass
        # Remove dirs deepest-first
        for d in sorted(dest_dirs_to_remove, reverse=True):
            try:
                shutil.rmtree(d)
            except OSError:
                pass


def generate_main_cpp(ino_files: list[str]) -> str:
    """Generate stub main.cpp content that includes .ino files from sketch directory.

    Args:
        ino_files: List of .ino filenames to include

    Returns:
        Content for main.cpp file

    Note:
        We do NOT include FastLED.h here because some sketches define macros
        (like USE_OCTOWS2811) before including FastLED.h. The sketch must include
        FastLED.h itself at the appropriate point.
    """
    includes: list[str] = []
    for ino_file in sorted(ino_files):
        includes.append("#include <Arduino.h>")
        # Don't include FastLED.h here - let the sketch include it after any
        # required #defines (like USE_OCTOWS2811, USE_WS2812SERIAL, etc.)
        includes.append(f'#include "sketch/{ino_file}"')

    include_lines = "\n".join(includes)

    int_main = """
void init(void) __attribute__((weak));

__attribute__((weak)) int main() {{
    init();  // Initialize Arduino timers and enable interrupts (calls sei())
    setup();
    while (true) {{
        loop();
        fl::delay(0);  // Yield to scheduler/watchdog (critical for ESP32 QEMU)
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
            # Skip .ino.cpp artifacts (preprocessed .ino files that would cause
            # duplicate symbols when PlatformIO also compiles the .ino via main.cpp)
            if file_path.name.endswith(".ino.cpp"):
                continue
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
                    _is_docker = os.environ.get("DOCKER_CONTAINER", "") != ""
                    _scandir_sync(
                        str(file_path),
                        str(dest_subdir),
                        purge=True,
                        content=_is_docker,
                    )
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
                _is_docker = os.environ.get("DOCKER_CONTAINER", "") != ""
                _scandir_sync(
                    str(boards_src),
                    str(boards_dst),
                    purge=True,
                    content=_is_docker,
                )
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
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
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
                _is_docker = os.environ.get("DOCKER_CONTAINER", "") != ""
                _scandir_sync(
                    str(fastled_src_path),
                    str(lib_dir),
                    purge=True,
                    content=_is_docker,
                )
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
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as sync_error:
        warnings.warn(f"Failed to sync FastLED library: {sync_error}")
        return False

    return True
