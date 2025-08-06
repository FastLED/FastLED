#!/usr/bin/env python3
"""
PlatformIO Builder for FastLED

Provides a clean interface for building FastLED projects with PlatformIO.
"""

import json
import os
import platform
import re
import shutil
import subprocess
import threading
import time
import urllib.request
import warnings
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Dict

from dirsync import sync  # type: ignore
from filelock import FileLock, Timeout  # type: ignore

from ci.compiler.compiler import Compiler, InitResult, SketchResult
from ci.util.boards import ALL, Board, create_board
from ci.util.output_formatter import create_sketch_path_formatter
from ci.util.create_build_dir import insert_tool_aliases
from ci.util.running_process import EndOfStream, RunningProcess


_HERE = Path(__file__).parent.resolve()
_PROJECT_ROOT = _HERE.parent.parent.resolve()


assert (_PROJECT_ROOT / "library.json").exists(), (
    f"Library JSON not found at {_PROJECT_ROOT / 'library.json'}"
)


class CountingFileLock:
    """A FileLock with atomic reference counting and explicit acquire(N)/decrement() methods."""

    def __init__(self, platform_name: str, build_dir: Path) -> None:
        self.platform_name = platform_name
        self.build_dir = build_dir

        # Use centralized path management
        self.paths = FastLEDPaths(platform_name)
        self.lock_file = self.paths.platform_lock_file
        self._file_lock = FileLock(self.lock_file)  # type: ignore
        self._count = 0
        self._count_lock = threading.Lock()
        self._is_acquired = False

    def acquire(self, count: int) -> None:
        """Acquire the lock for N work units."""
        if count <= 0:
            return

        with self._count_lock:
            was_zero = self._count == 0
            self._count += count

        if was_zero:
            # Only acquire the file lock when going from 0 to N
            self._acquire_with_warning()

    def decrement(self) -> None:
        """Decrement the count by 1. Releases lock when count reaches 0."""
        with self._count_lock:
            if self._count > 0:
                self._count -= 1
                should_release = self._count == 0
            else:
                should_release = False

        if should_release:
            # Only release the file lock when count reaches 0
            try:
                self._file_lock.release()
                self._is_acquired = False
                print(f"Released lock for platform {self.platform_name}")
            except Exception as e:
                warnings.warn(f"Failed to release lock for {self.platform_name}: {e}")

    def _acquire_with_warning(self) -> None:
        """Acquire the file lock using busy loop to allow keyboard interrupts."""
        if self._is_acquired:
            return  # Already acquired

        start_time = time.time()
        warning_shown = False

        while True:
            # Try to acquire with very short timeout (non-blocking)
            try:
                success = self._file_lock.acquire(timeout=0.1)
                if success:
                    self._is_acquired = True
                    print(f"Acquired lock for platform {self.platform_name}")
                    return
            except KeyboardInterrupt as ke:
                warnings.warn(
                    f"Keyboard interrupt while acquiring lock for platform {self.platform_name}"
                )
                import _thread

                _thread.interrupt_main()
                raise ke
            except Timeout:
                # Handle timeout exceptions as failed acquisition (continue loop)
                pass  # Continue the loop to check elapsed time and try again
            except Exception as e:
                # If acquire fails for other reasons, re-raise
                raise

            # Check if we should show warning (after 1 second)
            elapsed = time.time() - start_time
            if not warning_shown and elapsed >= 1.0:
                yellow = "\033[33m"
                reset = "\033[0m"
                folder_path = self.build_dir.parent.absolute()
                print(
                    f"{yellow}Platform {self.platform_name} is waiting to acquire {folder_path}{reset}"
                )
                warning_shown = True

            # Check for timeout (after 5 seconds)
            if elapsed >= 5.0:
                raise TimeoutError(
                    f"Failed to acquire lock for platform {self.platform_name} within 5 seconds. "
                    f"Lock file: {self.lock_file}. "
                    f"This may indicate another process is holding the lock or a deadlock occurred."
                )

            # Small sleep to prevent excessive CPU usage while allowing interrupts
            time.sleep(0.1)


def _ensure_platform_installed(board: Board) -> bool:
    """Ensure the required platform is installed for the board."""
    if not board.platform_needs_install:
        return True

    # Platform installation is handled by existing platform management code
    # This is a placeholder for future platform installation logic
    print(f"Platform installation needed for {board.board_name}: {board.platform}")
    return True


def _generate_build_info_json_from_existing_build(
    build_dir: Path, board: Board
) -> bool:
    """Generate build_info.json from an existing PlatformIO build.

    Args:
        build_dir: Build directory containing the PlatformIO project
        board: Board configuration

    Returns:
        True if build_info.json was successfully generated
    """
    try:
        # Use existing project to get metadata (no temporary project needed)
        metadata_cmd = ["pio", "project", "metadata", "--json-output"]
        metadata_result = subprocess.run(
            metadata_cmd,
            capture_output=True,
            text=True,
            cwd=build_dir,
            timeout=60,
        )

        if metadata_result.returncode != 0:
            print(
                f"Warning: Failed to get metadata for build_info.json: {metadata_result.stderr}"
            )
            return False

        # Parse and save the metadata
        try:
            data = json.loads(metadata_result.stdout)

            # Add tool aliases for symbol analysis and debugging
            insert_tool_aliases(data)

            # Save to build_info.json
            build_info_path = build_dir / "build_info.json"
            with open(build_info_path, "w") as f:
                json.dump(data, f, indent=4, sort_keys=True)

            print(f"✅ Generated build_info.json at {build_info_path}")
            return True

        except json.JSONDecodeError as e:
            print(f"Warning: Failed to parse metadata JSON for build_info.json: {e}")
            return False

    except subprocess.TimeoutExpired:
        print(f"Warning: Timeout generating build_info.json")
        return False
    except Exception as e:
        print(f"Warning: Exception generating build_info.json: {e}")
        return False


def _apply_board_specific_config(
    board: Board,
    platformio_ini_path: Path,
    example: str,
    additional_defines: list[str] | None = None,
    additional_include_dirs: list[str] | None = None,
    additional_libs: list[str] | None = None,
) -> bool:
    """Apply board-specific build configuration from Board class."""
    # Use centralized path management
    paths = FastLEDPaths(board.board_name)
    paths.ensure_directories_exist()

    # Generate platformio.ini content using the enhanced Board method
    config_content = board.to_platformio_ini(
        additional_defines=additional_defines,
        additional_include_dirs=additional_include_dirs,
        additional_libs=additional_libs,
        include_platformio_section=True,
        core_dir=str(paths.core_dir),
        packages_dir=str(paths.packages_dir),
        project_root=str(_PROJECT_ROOT),
    )
    platformio_ini_path.write_text(config_content)

    # Log applied configurations for debugging
    if board.build_flags:
        print(f"Applied build_flags: {board.build_flags}")
    if board.defines:
        print(f"Applied defines: {board.defines}")
    if additional_defines:
        print(f"Applied additional defines: {additional_defines}")
    if additional_include_dirs:
        print(f"Applied additional include dirs: {additional_include_dirs}")
    if board.platform_packages:
        print(f"Using platform_packages: {board.platform_packages}")

    return True


class FastLEDPaths:
    """Centralized path management for FastLED board-specific directories and files."""

    def __init__(self, board_name: str, project_root: Path | None = None) -> None:
        self.board_name = board_name
        self.project_root = project_root or _resolve_project_root()
        self.home_dir = Path.home()

        # Base FastLED directory
        self.fastled_root = self.home_dir / ".fastled"

    @property
    def build_dir(self) -> Path:
        """Project-local build directory for this board."""
        return self.project_root / ".build" / "pio" / self.board_name

    @property
    def platform_lock_file(self) -> Path:
        """Platform-specific build lock file."""
        return self.build_dir.parent / f"{self.board_name}.lock"

    @property
    def global_package_lock_file(self) -> Path:
        """Global package installation lock file."""
        packages_lock_root = self.fastled_root / "pio" / "packages"
        return packages_lock_root / f"{self.board_name}_global.lock"

    @property
    def core_dir(self) -> Path:
        """PlatformIO core directory (build cache, platforms)."""
        return self.fastled_root / "compile" / "pio" / self.board_name

    @property
    def packages_dir(self) -> Path:
        """PlatformIO packages directory (toolchains, frameworks)."""
        return self.fastled_root / "packages" / self.board_name

    def ensure_directories_exist(self) -> None:
        """Create all necessary directories."""
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self.global_package_lock_file.parent.mkdir(parents=True, exist_ok=True)
        self.core_dir.mkdir(parents=True, exist_ok=True)
        self.packages_dir.mkdir(parents=True, exist_ok=True)


class GlobalPackageLock:
    """A FileLock for global package installation per board. Acquired once during first build and released after completion."""

    def __init__(self, platform_name: str) -> None:
        self.platform_name = platform_name

        # Use centralized path management
        self.paths = FastLEDPaths(platform_name)
        self.lock_file = self.paths.global_package_lock_file
        self._file_lock = FileLock(self.lock_file)  # type: ignore
        self._is_acquired = False

    def acquire(self) -> None:
        """Acquire the global package installation lock for this board."""
        if self._is_acquired:
            return  # Already acquired

        start_time = time.time()
        warning_shown = False

        while True:
            # Try to acquire with very short timeout (non-blocking)
            try:
                success = self._file_lock.acquire(timeout=0.1)
                if success:
                    self._is_acquired = True
                    print(
                        f"Acquired global package lock for platform {self.platform_name}"
                    )
                    return
            except Timeout:
                # Handle timeout exceptions as failed acquisition (continue loop)
                pass  # Continue the loop to check elapsed time and try again
            except Exception as e:
                # If acquire fails for other reasons, re-raise
                raise

            # Check if we should show warning (after 1 second)
            elapsed = time.time() - start_time
            if not warning_shown and elapsed >= 1.0:
                yellow = "\033[33m"
                reset = "\033[0m"
                print(
                    f"{yellow}Platform {self.platform_name} is waiting to acquire global package lock at {self.lock_file.parent}{reset}"
                )
                warning_shown = True

            # Check for timeout (after 10 seconds - longer for package installation)
            if elapsed >= 10.0:
                raise TimeoutError(
                    f"Failed to acquire global package lock for platform {self.platform_name} within 10 seconds. "
                    f"Lock file: {self.lock_file}. "
                    f"This may indicate another process is installing packages or a deadlock occurred."
                )

            # Small sleep to prevent excessive CPU usage while allowing interrupts
            time.sleep(0.1)

    def release(self) -> None:
        """Release the global package installation lock."""
        if not self._is_acquired:
            return  # Not acquired

        try:
            self._file_lock.release()
            self._is_acquired = False
            print(f"Released global package lock for platform {self.platform_name}")
        except Exception as e:
            warnings.warn(
                f"Failed to release global package lock for {self.platform_name}: {e}"
            )

    def is_acquired(self) -> bool:
        """Check if the lock is currently acquired."""
        return self._is_acquired


# Remove duplicate dataclass definitions - use the ones from compiler.py


def _resolve_project_root() -> Path:
    """Resolve the FastLED project root directory."""
    current = Path(
        __file__
    ).parent.parent.parent.resolve()  # Go up from ci/compiler/pio.py
    while current != current.parent:
        if (current / "src" / "FastLED.h").exists():
            return current
        current = current.parent
    raise RuntimeError("Could not find FastLED project root")


def _create_building_banner(example: str) -> str:
    """Create a building banner for the given example."""
    banner_text = f"BUILDING {example}"
    border_char = "="
    padding = 2
    text_width = len(banner_text)
    total_width = text_width + (padding * 2)

    top_border = border_char * (total_width + 4)
    middle_line = (
        f"{border_char} {' ' * padding}{banner_text}{' ' * padding} {border_char}"
    )
    bottom_border = border_char * (total_width + 4)

    banner = f"{top_border}\n{middle_line}\n{bottom_border}"

    # Apply blue color using ANSI escape codes
    blue_color = "\033[34m"
    reset_color = "\033[0m"
    return f"{blue_color}{banner}{reset_color}"


def _get_example_error_message(project_root: Path, example: str) -> str:
    """Generate appropriate error message for missing example.

    Args:
        project_root: FastLED project root directory
        example: Example name or path that was not found

    Returns:
        Error message describing where the example was expected
    """
    example_path = Path(example)

    if example_path.is_absolute():
        return f"Example directory not found: {example}"
    elif "/" in example or "\\" in example:
        return f"Example directory not found: {example_path.resolve()}"
    else:
        return f"Example not found: {project_root / 'examples' / example}"


def _copy_example_source(project_root: Path, build_dir: Path, example: str) -> bool:
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
    elif "/" in example or "\\" in example:
        # Relative path - resolve relative to current directory
        example_path = example_path.resolve()
        if not example_path.exists():
            return False
    else:
        # Just a name - resolve to examples directory
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
            # Recursively copy subdirectories
            dest_subdir = sketch_dir / file_path.name
            shutil.copytree(file_path, dest_subdir)
            try:
                rel_source = file_path.relative_to(Path.cwd())
                rel_dest = dest_subdir.relative_to(Path.cwd())
                print(f"Copied directory {rel_source} to {rel_dest}")
            except ValueError:
                print(f"Copied directory {file_path} to {dest_subdir}")

    # Create or update stub main.cpp that includes the .ino files
    main_cpp_content = _generate_main_cpp(ino_files)
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


def _generate_main_cpp(ino_files: list[str]) -> str:
    """Generate stub main.cpp content that includes .ino files from sketch directory.

    Args:
        ino_files: List of .ino filenames to include

    Returns:
        Content for main.cpp file
    """
    includes: list[str] = []
    for ino_file in sorted(ino_files):
        includes.append(f"#include <Arduino.h>")
        includes.append(f'#include "sketch/{ino_file}"')

    include_lines = "\n".join(includes)

    int_main = """
int main() {{
    setup();
    while (true) {{
        loop();
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


def _copy_boards_directory(project_root: Path, build_dir: Path) -> bool:
    """Copy boards directory to the build directory."""
    boards_src = project_root / "ci" / "boards"
    boards_dst = build_dir / "boards"

    if not boards_src.exists():
        warnings.warn(f"Boards directory not found: {boards_src}")
        return False

    if boards_dst.exists():
        shutil.rmtree(boards_dst)

    try:
        shutil.copytree(boards_src, boards_dst)
    except Exception as e:
        warnings.warn(f"Failed to copy boards directory: {e}")
        return False

    return True


def _copy_fastled_library(project_root: Path, build_dir: Path) -> bool:
    """Create symlink to FastLED library in the build directory."""
    lib_dir = build_dir / "lib" / "FastLED"
    lib_parent = build_dir / "lib"

    # Remove existing FastLED directory if it exists
    if lib_dir.exists():
        if lib_dir.is_symlink():
            lib_dir.unlink()
        else:
            shutil.rmtree(lib_dir)

    # Create lib directory if it doesn't exist
    lib_parent.mkdir(parents=True, exist_ok=True)

    # Create symlink to FastLED source
    fastled_src_path = project_root / "src"
    try:
        # Convert to absolute path for cross-platform compatibility
        fastled_src_absolute = fastled_src_path.resolve()
        lib_dir.symlink_to(fastled_src_absolute, target_is_directory=True)
        # Calculate relative paths for cleaner output
        try:
            rel_lib_dir = lib_dir.relative_to(Path.cwd())
            rel_src_path = fastled_src_path.relative_to(Path.cwd())
            print(f"Created symlink: {rel_lib_dir} -> {rel_src_path}")
        except ValueError:
            # Fallback to absolute paths if relative calculation fails
            print(f"Created symlink: {lib_dir} -> {fastled_src_absolute}")
    except OSError as e:
        warnings.warn(f"Failed to create symlink (trying copy fallback): {e}")
        # Fallback to copy if symlink fails (e.g., no admin privileges on Windows)
        try:
            shutil.copytree(fastled_src_path, lib_dir, dirs_exist_ok=True)
            # Calculate relative paths for cleaner output
            try:
                rel_lib_dir = lib_dir.relative_to(Path.cwd())
                print(f"Fallback: Copied FastLED library to {rel_lib_dir}")
            except ValueError:
                # Fallback to absolute paths if relative calculation fails
                print(f"Fallback: Copied FastLED library to {lib_dir}")
        except Exception as copy_error:
            warnings.warn(f"Failed to copy FastLED library: {copy_error}")
            return False

    # Note: library.json is not needed since we manually set include path in platformio.ini

    return True


def _init_platformio_build(
    board: Board,
    verbose: bool,
    example: str,
    additional_defines: list[str] | None = None,
    additional_include_dirs: list[str] | None = None,
    additional_libs: list[str] | None = None,
) -> InitResult:
    """Initialize the PlatformIO build directory. Assumes lock is already held by caller."""
    project_root = _resolve_project_root()
    build_dir = project_root / ".build" / "pio" / board.board_name

    # Lock is already held by build() - no need to acquire again

    # Setup the build directory.
    build_dir.mkdir(parents=True, exist_ok=True)
    platformio_ini = build_dir / "platformio.ini"

    # Ensure platform is installed if needed
    if not _ensure_platform_installed(board):
        return InitResult(
            success=False,
            output=f"Failed to install platform for {board.board_name}",
            build_dir=build_dir,
        )

    # Clone board and add sketch directory include path (enables "shared/file.h" style includes)
    board_with_sketch_include = board.clone()
    if board_with_sketch_include.build_flags is None:
        board_with_sketch_include.build_flags = []
    else:
        board_with_sketch_include.build_flags = list(
            board_with_sketch_include.build_flags
        )
    board_with_sketch_include.build_flags.append("-Isrc/sketch")

    # Optimization report generation is available but OFF by default
    # To enable optimization reports, add these flags to your board configuration:
    # - "-fopt-info-all=optimization_report.txt" for detailed optimization info
    # - "-Wl,-Map,firmware.map" for memory map analysis
    #
    # Note: The infrastructure is in place to support optimization reports when needed

    # Apply board-specific configuration
    if not _apply_board_specific_config(
        board_with_sketch_include,
        platformio_ini,
        example,
        additional_defines,
        additional_include_dirs,
        additional_libs,
    ):
        return InitResult(
            success=False,
            output=f"Failed to apply board configuration for {board.board_name}",
            build_dir=build_dir,
        )

    # Print building banner first
    print(_create_building_banner(example))

    ok_copy_src = _copy_example_source(project_root, build_dir, example)
    if not ok_copy_src:
        error_msg = _get_example_error_message(project_root, example)
        warnings.warn(error_msg)
        return InitResult(
            success=False,
            output=error_msg,
            build_dir=build_dir,
        )

    # Copy FastLED library
    ok_copy_fastled = _copy_fastled_library(project_root, build_dir)
    if not ok_copy_fastled:
        warnings.warn(f"Failed to copy FastLED library")
        return InitResult(
            success=False, output=f"Failed to copy FastLED library", build_dir=build_dir
        )

    # Copy boards directory
    ok_copy_boards = _copy_boards_directory(project_root, build_dir)
    if not ok_copy_boards:
        warnings.warn(f"Failed to copy boards directory")
        return InitResult(
            success=False,
            output=f"Failed to copy boards directory",
            build_dir=build_dir,
        )

    # Final platformio.ini is already written by _apply_board_specific_config
    # No need to write it again

    # Run initial build with LDF enabled to set up the environment
    run_cmd: list[str] = ["pio", "run", "--project-dir", str(build_dir)]
    if verbose:
        run_cmd.append("--verbose")

    # Print platformio.ini content for the initialization build
    platformio_ini_path = build_dir / "platformio.ini"
    if platformio_ini_path.exists():
        print()  # Add newline before configuration section
        print("=" * 60)
        print("PLATFORMIO.INI CONFIGURATION:")
        print("=" * 60)
        ini_content = platformio_ini_path.read_text()
        print(ini_content)
        print("=" * 60)
        print()  # Add newline after configuration section

    print(f"Running initial build command: {subprocess.list2cmdline(run_cmd)}")

    # Start timer for this example
    start_time = time.time()

    # Create formatter for path substitution and timestamping
    formatter = create_sketch_path_formatter(example)

    running_process = RunningProcess(
        run_cmd, cwd=build_dir, auto_run=True, output_formatter=formatter
    )
    # Output is transformed by the formatter, but we need to print it
    while line := running_process.get_next_line():
        if isinstance(line, EndOfStream):
            break
        # Print the transformed line
        print(line)

    running_process.wait()

    if running_process.returncode != 0:
        return InitResult(
            success=False,
            output=f"Initial build failed: {running_process.stdout}",
            build_dir=build_dir,
        )

    # After successful build, configuration is already properly set up
    # Board configuration includes all necessary settings

    # Generate build_info.json after successful initialization build
    _generate_build_info_json_from_existing_build(build_dir, board)

    return InitResult(success=True, output="", build_dir=build_dir)


class PioCompiler(Compiler):
    def __init__(
        self,
        board: Board | str,
        verbose: bool,
        additional_defines: list[str] | None = None,
        additional_include_dirs: list[str] | None = None,
        additional_libs: list[str] | None = None,
    ) -> None:
        # Call parent constructor
        super().__init__()

        # Convert string to Board object if needed
        if isinstance(board, str):
            self.board = create_board(board)
        else:
            self.board = board
        self.verbose = verbose
        self.additional_defines = additional_defines
        self.additional_include_dirs = additional_include_dirs
        self.additional_libs = additional_libs

        # Use centralized path management
        self.paths = FastLEDPaths(self.board.board_name)
        self.build_dir: Path = self.paths.build_dir

        # Ensure all directories exist
        self.paths.ensure_directories_exist()

        # Create the counting file lock in constructor
        self._platform_lock = CountingFileLock(self.board.board_name, self.build_dir)

        # Create the global package installation lock
        self._global_package_lock = GlobalPackageLock(self.board.board_name)

        self.initialized = False
        self.executor = ThreadPoolExecutor(max_workers=1)

    def _internal_init_build_no_lock(self, example: str) -> InitResult:
        """Initialize the PlatformIO build directory once with the first example."""
        if self.initialized:
            return InitResult(
                success=True, output="Already initialized", build_dir=self.build_dir
            )

        # Initialize with the actual first example being built
        result = _init_platformio_build(
            self.board,
            self.verbose,
            example,
            self.additional_defines,
            self.additional_include_dirs,
            self.additional_libs,
        )
        if result.success:
            self.initialized = True
        return result

    def cancel_all(self) -> None:
        """Cancel all builds."""
        self.executor.shutdown(wait=False, cancel_futures=True)

    def build(self, examples: list[str]) -> list[Future[SketchResult]]:
        """Build a list of examples with proper lock management."""
        if not examples:
            return []

        # Acquire the lock for N work units
        self._platform_lock.acquire(len(examples))

        # Acquire the global package lock for the first build (package installation)
        self._global_package_lock.acquire()

        futures: list[Future[SketchResult]] = []
        global_lock_released = (
            threading.Event()
        )  # Track if global lock has been released

        def create_decrement_callback() -> Callable[[Future[SketchResult]], None]:
            """Create callback that decrements lock count when future completes."""

            def decrement_callback(future: Future[SketchResult]) -> None:
                self._platform_lock.decrement()

                # Release global package lock when first build completes (only once)
                if not global_lock_released.is_set():
                    global_lock_released.set()
                    self._global_package_lock.release()

            return decrement_callback

        # Submit all builds
        for example in examples:
            future = self.executor.submit(self._internal_build_no_lock, example)

            # Add callback to decrement lock count on completion or cancellation
            future.add_done_callback(create_decrement_callback())

            futures.append(future)

        return futures

    def _build_internal(self, example: str) -> SketchResult:
        """Internal build method without lock management."""
        # Print building banner first
        print(_create_building_banner(example))

        # Copy example source to build directory
        project_root = _resolve_project_root()
        ok_copy_src = _copy_example_source(project_root, self.build_dir, example)
        if not ok_copy_src:
            error_msg = _get_example_error_message(project_root, example)
            return SketchResult(
                success=False,
                output=error_msg,
                build_dir=self.build_dir,
                example=example,
            )

        # Run PlatformIO build
        run_cmd: list[str] = [
            "pio",
            "run",
            "--project-dir",
            str(self.build_dir),
            "--disable-auto-clean",
        ]
        if self.verbose:
            run_cmd.append("--verbose")

        print(f"Running command: {subprocess.list2cmdline(run_cmd)}")

        # Create formatter for path substitution and timestamping
        formatter = create_sketch_path_formatter(example)

        running_process = RunningProcess(
            run_cmd, cwd=self.build_dir, auto_run=True, output_formatter=formatter
        )
        try:
            # Output is transformed by the formatter, but we need to print it
            while line := running_process.get_next_line(timeout=60):
                if isinstance(line, EndOfStream):
                    break
                # Print the transformed line
                print(line)
        except OSError as e:
            # Handle output encoding issues on Windows
            print(f"Output encoding issue: {e}")
            pass

        running_process.wait()

        success = running_process.returncode == 0

        # Print SUCCESS/FAILED message immediately in worker thread to avoid race conditions
        if success:
            green_color = "\033[32m"
            reset_color = "\033[0m"
            print(f"{green_color}SUCCESS: {example}{reset_color}")
        else:
            red_color = "\033[31m"
            reset_color = "\033[0m"
            print(f"{red_color}FAILED: {example}{reset_color}")

        # Check if build was successful
        build_success = running_process.returncode == 0

        # Generate build_info.json after successful build
        if build_success:
            _generate_build_info_json_from_existing_build(self.build_dir, self.board)

        return SketchResult(
            success=success,
            output=running_process.stdout,
            build_dir=self.build_dir,
            example=example,
        )

    def _internal_build_no_lock(self, example: str) -> SketchResult:
        """Build a specific example without lock management. Only call from build()."""
        if not self.initialized:
            init_result = self._internal_init_build_no_lock(example)
            if not init_result.success:
                # Print FAILED message immediately in worker thread
                red_color = "\033[31m"
                reset_color = "\033[0m"
                print(f"{red_color}FAILED: {example}{reset_color}")

                return SketchResult(
                    success=False,
                    output=init_result.output,
                    build_dir=init_result.build_dir,
                    example=example,
                )
            # If initialization succeeded and we just built the example, return success
            # Print SUCCESS message immediately in worker thread
            green_color = "\033[32m"
            reset_color = "\033[0m"
            print(f"{green_color}SUCCESS: {example}{reset_color}")

            return SketchResult(
                success=True,
                output="Built during initialization",
                build_dir=self.build_dir,
                example=example,
            )

        # No lock management - caller (build) handles locks
        return self._build_internal(example)

    def clean(self) -> None:
        """Clean build artifacts for this platform (acquire platform lock)."""
        print(f"Cleaning build artifacts for platform {self.board.board_name}...")

        # Acquire the platform lock to ensure no other builds are running
        self._platform_lock.acquire(1)

        try:
            # Remove the local build directory
            if self.build_dir.exists():
                print(f"Removing build directory: {self.build_dir}")
                shutil.rmtree(self.build_dir)
                print(f"✅ Cleaned local build artifacts for {self.board.board_name}")
            else:
                print(
                    f"✅ No build directory found for {self.board.board_name} (already clean)"
                )
        finally:
            # Always decrement the lock
            self._platform_lock.decrement()

    def clean_all(self) -> None:
        """Clean all build artifacts (local and global packages) for this platform."""
        print(f"Cleaning all artifacts for platform {self.board.board_name}...")

        # Acquire both platform and global package locks
        self._platform_lock.acquire(1)
        self._global_package_lock.acquire()

        try:
            # Clean local build artifacts first
            if self.build_dir.exists():
                print(f"Removing build directory: {self.build_dir}")
                shutil.rmtree(self.build_dir)
                print(f"✅ Cleaned local build artifacts for {self.board.board_name}")
            else:
                print(f"✅ No build directory found for {self.board.board_name}")

            # Clean global packages directory
            if self.paths.packages_dir.exists():
                print(f"Removing global packages directory: {self.paths.packages_dir}")
                shutil.rmtree(self.paths.packages_dir)
                print(f"✅ Cleaned global packages for {self.board.board_name}")
            else:
                print(
                    f"✅ No global packages directory found for {self.board.board_name}"
                )

            # Clean global core directory (build cache, platforms)
            if self.paths.core_dir.exists():
                print(f"Removing global core directory: {self.paths.core_dir}")
                shutil.rmtree(self.paths.core_dir)
                print(f"✅ Cleaned global core cache for {self.board.board_name}")
            else:
                print(f"✅ No global core directory found for {self.board.board_name}")

        finally:
            # Always release locks
            self._global_package_lock.release()
            self._platform_lock.decrement()

    def deploy(
        self, example: str, upload_port: str | None = None, monitor: bool = False
    ) -> SketchResult:
        """Deploy (upload) a specific example to the target device.

        Args:
            example: Name of the example to deploy
            upload_port: Optional specific port for upload (e.g., "/dev/ttyUSB0", "COM3")
            monitor: If True, attach to device monitor after successful upload
        """
        print(f"Deploying {example} to {self.board.board_name}...")

        # Acquire platform lock for this operation
        self._platform_lock.acquire(1)
        lock_released = False

        try:
            # Ensure the build is initialized and the example is built
            if not self.initialized:
                init_result = self._internal_init_build_no_lock(example)
                if not init_result.success:
                    return SketchResult(
                        success=False,
                        output=init_result.output,
                        build_dir=init_result.build_dir,
                        example=example,
                    )
            else:
                # Build the example first (ensures it's up to date)
                build_result = self._build_internal(example)
                if not build_result.success:
                    return build_result

            # Run PlatformIO upload command
            upload_cmd: list[str] = [
                "pio",
                "run",
                "--project-dir",
                str(self.build_dir),
                "--target",
                "upload",
            ]

            if upload_port:
                upload_cmd.extend(["--upload-port", upload_port])

            if self.verbose:
                upload_cmd.append("--verbose")

            print(f"Running upload command: {subprocess.list2cmdline(upload_cmd)}")

            # Create formatter for upload output
            formatter = create_sketch_path_formatter(example)

            running_process = RunningProcess(
                upload_cmd,
                cwd=self.build_dir,
                auto_run=True,
                output_formatter=formatter,
            )
            try:
                # Output is transformed by the formatter, but we need to print it
                while line := running_process.get_next_line(timeout=60):
                    if isinstance(line, EndOfStream):
                        break
                    # Print the transformed line
                    print(line)
            except OSError as e:
                # Handle output encoding issues on Windows
                print(f"Upload encoding issue: {e}")
                pass

            running_process.wait()

            # Check if upload was successful
            upload_success = running_process.returncode == 0
            upload_output = running_process.stdout

            if not upload_success:
                return SketchResult(
                    success=False,
                    output=upload_output,
                    build_dir=self.build_dir,
                    example=example,
                )

            # Upload completed successfully - release the lock before monitor
            print(f"✅ Upload completed successfully for {example}")
            self._platform_lock.decrement()
            lock_released = True

            # If monitor is requested and upload was successful, start monitor
            if monitor:
                print(
                    f"📡 Starting monitor for {example} on {self.board.board_name}..."
                )
                print("📝 Press Ctrl+C to exit monitor")

                monitor_cmd: list[str] = [
                    "pio",
                    "device",
                    "monitor",
                    "--project-dir",
                    str(self.build_dir),
                ]

                if upload_port:
                    monitor_cmd.extend(["--port", upload_port])

                print(
                    f"Running monitor command: {subprocess.list2cmdline(monitor_cmd)}"
                )

                # Start monitor process (no lock needed for monitoring)
                monitor_process = RunningProcess(
                    monitor_cmd, cwd=self.build_dir, auto_run=True
                )
                try:
                    while line := monitor_process.get_next_line(
                        timeout=None
                    ):  # No timeout for monitor
                        if isinstance(line, EndOfStream):
                            break
                        print(line)  # No timestamp for monitor output
                except KeyboardInterrupt:
                    print("\n📡 Monitor stopped by user")
                    monitor_process.terminate()
                except OSError as e:
                    print(f"Monitor encoding issue: {e}")
                    pass

                monitor_process.wait()

            return SketchResult(
                success=True,
                output=upload_output,
                build_dir=self.build_dir,
                example=example,
            )

        finally:
            # Only decrement the lock if it hasn't been released yet
            if not lock_released:
                self._platform_lock.decrement()

    def check_usb_permissions(self) -> tuple[bool, str]:
        """Check if USB device access is properly configured on Linux.

        Checks multiple methods for USB device access:
        1. PlatformIO udev rules
        2. User group membership (dialout, uucp, plugdev)
        3. Alternative udev rules files

        Returns:
            Tuple of (has_access, status_message)
        """
        if platform.system() != "Linux":
            return True, "Not applicable on non-Linux systems"

        access_methods: list[str] = []

        # Check 1: PlatformIO udev rules
        udev_rules_path = Path("/etc/udev/rules.d/99-platformio-udev.rules")
        if udev_rules_path.exists():
            access_methods.append("PlatformIO udev rules")

        # Check 2: User group membership
        user_groups = self._get_user_groups()
        usb_groups = ["dialout", "uucp", "plugdev", "tty"]
        user_usb_groups = [group for group in usb_groups if group in user_groups]
        if user_usb_groups:
            access_methods.append(f"Group membership: {', '.join(user_usb_groups)}")

        # Check 3: Alternative udev rules
        alt_udev_files = [
            "/etc/udev/rules.d/99-arduino.rules",
            "/etc/udev/rules.d/50-platformio-udev.rules",
            "/lib/udev/rules.d/99-platformio-udev.rules",
        ]
        for alt_file in alt_udev_files:
            if Path(alt_file).exists():
                access_methods.append(f"Alternative udev rules: {Path(alt_file).name}")

        # Check 4: Root user (always has access)
        if self._is_root_user():
            access_methods.append("Root user privileges")

        if access_methods:
            status = f"USB access available via: {'; '.join(access_methods)}"
            return True, status
        else:
            return False, "No USB device access methods detected"

    def _get_user_groups(self) -> list[str]:
        """Get list of groups the current user belongs to."""
        try:
            result = subprocess.run(["groups"], capture_output=True, text=True)
            if result.returncode == 0:
                return result.stdout.strip().split()
            return []
        except Exception:
            return []

    def _is_root_user(self) -> bool:
        """Check if running as root user."""
        try:
            import os

            return os.geteuid() == 0
        except Exception:
            return False

    def check_udev_rules(self) -> bool:
        """Check if PlatformIO udev rules are installed on Linux.

        DEPRECATED: Use check_usb_permissions() instead for comprehensive checking.

        Returns:
            True if any USB access method is available, False otherwise
        """
        has_access, _ = self.check_usb_permissions()
        return has_access

    def install_usb_permissions(self) -> bool:
        """Install platform-specific USB permissions (e.g., udev rules on Linux).

        Returns:
            True if installation succeeded, False otherwise
        """
        if platform.system() != "Linux":
            print("INFO: udev rules are only needed on Linux systems")
            return True

        udev_url = "https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules"
        udev_rules_path = "/etc/udev/rules.d/99-platformio-udev.rules"

        print("📡 Downloading PlatformIO udev rules...")

        try:
            # Download the udev rules
            with urllib.request.urlopen(udev_url) as response:
                udev_content = response.read().decode("utf-8")

            # Write to temporary file first
            temp_file = "/tmp/99-platformio-udev.rules"
            with open(temp_file, "w") as f:
                f.write(udev_content)

            print("💾 Installing udev rules (requires sudo)...")

            # Use sudo to copy to system location
            result = subprocess.run(
                ["sudo", "cp", temp_file, udev_rules_path],
                capture_output=True,
                text=True,
            )

            if result.returncode != 0:
                print(f"ERROR: Failed to install udev rules: {result.stderr}")
                return False

            # Clean up temp file
            os.unlink(temp_file)

            print("✅ PlatformIO udev rules installed successfully!")
            print("⚠️  To complete the installation, run one of the following:")
            print("   sudo service udev restart")
            print("   # or")
            print("   sudo udevadm control --reload-rules")
            print("   sudo udevadm trigger")
            print(
                "⚠️  You may also need to restart your system for the changes to take effect."
            )

            return True

        except Exception as e:
            print(f"ERROR: Failed to install udev rules: {e}")
            return False


def run_pio_build(
    board: Board | str,
    examples: list[str],
    verbose: bool = False,
    additional_defines: list[str] | None = None,
    additional_include_dirs: list[str] | None = None,
) -> list[Future[SketchResult]]:
    """Run build for specified examples and platform using new PlatformIO system.

    Args:
        board: Board class instance or board name string (resolved via create_board())
        examples: List of example names to build
        verbose: Enable verbose output
        additional_defines: Additional defines to add to build flags (e.g., ["FASTLED_DEFINE=0", "DEBUG=1"])
        additional_include_dirs: Additional include directories to add to build flags (e.g., ["src/platforms/sub", "external/libs"])
    """
    pio = PioCompiler(board, verbose, additional_defines, additional_include_dirs)
    return pio.build(examples)
