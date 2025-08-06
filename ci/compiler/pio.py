#!/usr/bin/env python3
"""
PlatformIO Builder for FastLED

Provides a clean interface for building FastLED projects with PlatformIO.
"""

import os
import shutil
import subprocess
import threading
import time
import warnings
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Dict

from dirsync import sync  # type: ignore
from filelock import FileLock, Timeout  # type: ignore

from ci.util.boards import ALL, Board, create_board
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


def _apply_board_specific_config(
    board: Board,
    platformio_ini_path: Path,
    example: str,
    additional_defines: list[str] | None = None,
    additional_include_dirs: list[str] | None = None,
) -> bool:
    """Apply board-specific build configuration from Board class."""
    # Use centralized path management
    paths = FastLEDPaths(board.board_name)
    paths.ensure_directories_exist()

    # Generate platformio.ini content using the enhanced Board method
    config_content = board.to_platformio_ini(
        example=example,
        additional_defines=additional_defines,
        additional_include_dirs=additional_include_dirs,
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


@dataclass
class InitResult:
    success: bool
    output: str
    build_dir: Path

    @property
    def platformio_ini(self) -> Path:
        return self.build_dir / "platformio.ini"


@dataclass
class SketchResult:
    success: bool
    output: str
    build_dir: Path
    example: str


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

    # Apply board-specific configuration
    if not _apply_board_specific_config(
        board_with_sketch_include,
        platformio_ini,
        example,
        additional_defines,
        additional_include_dirs,
    ):
        return InitResult(
            success=False,
            output=f"Failed to apply board configuration for {board.board_name}",
            build_dir=build_dir,
        )

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
    print(_create_building_banner(example))

    # Start timer for this example
    start_time = time.time()

    running_process = RunningProcess(run_cmd, cwd=build_dir, auto_run=True)
    while line := running_process.get_next_line():
        if isinstance(line, EndOfStream):
            break
        assert isinstance(line, str)
        # Add timestamp to each line
        elapsed = time.time() - start_time
        print(f"{elapsed:.2f} {line}")

    running_process.wait()

    if running_process.returncode != 0:
        return InitResult(
            success=False,
            output=f"Initial build failed: {running_process.stdout}",
            build_dir=build_dir,
        )

    # After successful build, configuration is already properly set up
    # Board configuration includes all necessary settings

    return InitResult(success=True, output="", build_dir=build_dir)


class PioCompiler:
    def __init__(
        self,
        board: Board | str,
        verbose: bool,
        additional_defines: list[str] | None = None,
        additional_include_dirs: list[str] | None = None,
    ):
        # Convert string to Board object if needed
        if isinstance(board, str):
            self.board = create_board(board)
        else:
            self.board = board
        self.verbose = verbose
        self.additional_defines = additional_defines
        self.additional_include_dirs = additional_include_dirs

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
        print(_create_building_banner(example))

        # Start timer for this example
        start_time = time.time()

        running_process = RunningProcess(run_cmd, cwd=self.build_dir, auto_run=True)
        try:
            while line := running_process.get_next_line(timeout=60):
                if isinstance(line, EndOfStream):
                    break
                # Add timestamp to each line
                elapsed = time.time() - start_time
                print(f"{elapsed:.2f} {line}")
        except OSError as e:
            # Handle output encoding issues on Windows
            elapsed = time.time() - start_time
            print(f"{elapsed:.2f} Output encoding issue: {e}")
            pass

        running_process.wait()

        return SketchResult(
            success=running_process.returncode == 0,
            output=running_process.stdout,
            build_dir=self.build_dir,
            example=example,
        )

    def _internal_build_no_lock(self, example: str) -> SketchResult:
        """Build a specific example without lock management. Only call from build()."""
        if not self.initialized:
            init_result = self._internal_init_build_no_lock(example)
            if not init_result.success:
                return SketchResult(
                    success=False,
                    output=init_result.output,
                    build_dir=init_result.build_dir,
                    example=example,
                )
            # If initialization succeeded and we just built the example, return success
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
