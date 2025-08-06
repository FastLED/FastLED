#!/usr/bin/env python3
"""
PlatformIO Builder for FastLED

Provides a clean interface for building FastLED projects with PlatformIO.
"""

import shutil
import subprocess
import warnings
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

from dirsync import sync  # type: ignore

from ci.util.boards import ALL, Board, get_board
from ci.util.running_process import EndOfStream, RunningProcess


_EXECUTOR = ThreadPoolExecutor(max_workers=1)

_HERE = Path(__file__).parent.resolve()
_PROJECT_ROOT = _HERE.parent.parent.resolve()
_LIB_LDF_MODE = "chain"

assert (_PROJECT_ROOT / "library.json").exists(), (
    f"Library JSON not found at {_PROJECT_ROOT / 'library.json'}"
)


def _get_platform_family(board: Board) -> str:
    """Detect platform family from Board.platform."""
    if not board.platform:
        return "unknown"

    platform = board.platform.lower()
    if "atmel" in platform:
        return "avr"
    elif "espressif" in platform:
        return "esp"
    elif "apollo3" in platform:
        return "apollo3"
    elif "native" in platform:
        return "native"
    elif "rpi" in platform or "raspberrypi" in platform:
        return "rpi"
    else:
        return "custom"


def _ensure_platform_installed(board: Board) -> bool:
    """Ensure the required platform is installed for the board."""
    if not board.platform_needs_install:
        return True

    # Platform installation is handled by existing platform management code
    # This is a placeholder for future platform installation logic
    print(f"Platform installation needed for {board.board_name}: {board.platform}")
    return True


def _apply_board_specific_config(board: Board, platformio_ini_path: Path) -> bool:
    """Apply board-specific build configuration from Board class."""
    # Board.to_platformio_ini() already handles this comprehensively
    # This function mainly for validation and logging

    build_config = BuildConfig(board=board)
    config_content = build_config.to_platformio_ini()
    platformio_ini_path.write_text(config_content)

    # Log applied configurations for debugging
    if board.build_flags:
        print(f"Applied build_flags: {board.build_flags}")
    if board.defines:
        print(f"Applied defines: {board.defines}")
    if board.platform_packages:
        print(f"Using platform_packages: {board.platform_packages}")

    return True


@dataclass
class InitResult:
    success: bool
    output: str
    build_dir: Path

    @property
    def platformio_ini(self) -> Path:
        return self.build_dir / "platformio.ini"


@dataclass
class BuildResult:
    success: bool
    output: str
    build_dir: Path
    example: str


@dataclass
class BuildConfig:
    board: Board  # Use Board class instead of individual fields

    def to_platformio_ini(self) -> str:
        """Generate platformio.ini content using Board configuration."""
        out: list[str] = []
        out.append(f"[env:{self.board.board_name}]")

        # Use Board's comprehensive configuration
        board_config = self.board.to_platformio_ini()
        # Extract everything after the section header
        lines = board_config.split("\n")[1:]  # Skip [env:...] line
        out.extend(line for line in lines if line.strip())

        # Add FastLED-specific configurations
        out.append(f"lib_ldf_mode = {_LIB_LDF_MODE}")
        out.append("lib_archive = true")
        out.append(f"lib_deps = symlink://{_PROJECT_ROOT}")

        return "\n".join(out)


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


def _copy_example_source(project_root: Path, build_dir: Path, example: str) -> bool:
    """Copy example source to the build directory with sketch subdirectory structure.

    Args:
        project_root: FastLED project root directory
        build_dir: Build directory for the target
        example: Name of the example to copy

    Returns:
        True if successful, False if example not found
    """
    # Configure example source
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

    # Copy all files from example directory to sketch subdirectory
    ino_files: list[str] = []
    for file_path in example_path.iterdir():
        if file_path.is_file():
            if "fastled_js" in str(file_path):
                # skip fastled_js output folder.
                continue
            shutil.copy2(file_path, sketch_dir)
            print(f"Copied {file_path} to {sketch_dir}")
            if file_path.suffix == ".ino":
                ino_files.append(file_path.name)

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
        print(f"Created symlink: {lib_dir} -> {fastled_src_absolute}")
    except OSError as e:
        warnings.warn(f"Failed to create symlink (trying copy fallback): {e}")
        # Fallback to copy if symlink fails (e.g., no admin privileges on Windows)
        try:
            shutil.copytree(fastled_src_path, lib_dir, dirs_exist_ok=True)
            print(f"Fallback: Copied FastLED library to {lib_dir}")
        except Exception as copy_error:
            warnings.warn(f"Failed to copy FastLED library: {copy_error}")
            return False

    # Note: library.json is not needed since we manually set include path in platformio.ini

    return True


def _init_platformio_build(board: Board, verbose: bool, example: str) -> InitResult:
    """Initialize the PlatformIO build directory."""
    project_root = _resolve_project_root()
    build_dir = project_root / ".build" / "test_platformio" / board.board_name

    # Setup the build directory.
    build_dir.mkdir(parents=True, exist_ok=True)
    platformio_ini = build_dir / "platformio.ini"

    # Platform-specific handling
    platform_family = _get_platform_family(board)
    print(f"Detected platform family: {platform_family} for board {board.board_name}")

    # Ensure platform is installed if needed
    if not _ensure_platform_installed(board):
        return InitResult(
            success=False,
            output=f"Failed to install platform for {board.board_name}",
            build_dir=build_dir,
        )

    # Apply board-specific configuration
    if not _apply_board_specific_config(board, platformio_ini):
        return InitResult(
            success=False,
            output=f"Failed to apply board configuration for {board.board_name}",
            build_dir=build_dir,
        )

    ok_copy_src = _copy_example_source(project_root, build_dir, example)
    if not ok_copy_src:
        warnings.warn(f"Example not found: {project_root / 'examples' / example}")
        return InitResult(
            success=False,
            output=f"Example not found: {project_root / 'examples' / example}",
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

    print(f"Running initial build command: {subprocess.list2cmdline(run_cmd)}")

    running_process = RunningProcess(run_cmd, cwd=build_dir, auto_run=True)
    while line := running_process.get_next_line():
        if isinstance(line, EndOfStream):
            break
        assert isinstance(line, str)
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

    return InitResult(success=True, output="", build_dir=build_dir)


class PlatformIoBuilder:
    def __init__(self, board: Board | str, verbose: bool):
        # Convert string to Board object if needed
        if isinstance(board, str):
            self.board = get_board(board)
        else:
            self.board = board
        self.verbose = verbose
        self.build_dir: Path | None = None
        self.initialized = False

    def init_build(self) -> InitResult:
        """Initialize the PlatformIO build directory once."""
        if self.initialized and self.build_dir is not None:
            return InitResult(
                success=True, output="Already initialized", build_dir=self.build_dir
            )

        # Initialize with specific parameters
        result = _init_platformio_build(self.board, self.verbose, "Blink")
        if result.success:
            self.build_dir = result.build_dir
            self.initialized = True
        return result

    def build(self, example: str) -> BuildResult:
        """Build a specific example."""
        if not self.initialized:
            init_result = self.init_build()
            if not init_result.success:
                return BuildResult(
                    success=False,
                    output=init_result.output,
                    build_dir=init_result.build_dir,
                    example=example,
                )

        if self.build_dir is None:
            return BuildResult(
                success=False,
                output="Build directory not initialized",
                build_dir=Path(),
                example=example,
            )

        # Copy example source to build directory
        project_root = _resolve_project_root()
        ok_copy_src = _copy_example_source(project_root, self.build_dir, example)
        if not ok_copy_src:
            return BuildResult(
                success=False,
                output=f"Example not found: {project_root / 'examples' / example}",
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

        running_process = RunningProcess(run_cmd, cwd=self.build_dir, auto_run=True)
        try:
            while line := running_process.get_next_line(timeout=60):
                if isinstance(line, EndOfStream):
                    break
                print(line)
        except OSError as e:
            # Handle output encoding issues on Windows
            print(f"Output encoding issue: {e}")
            pass

        running_process.wait()

        return BuildResult(
            success=running_process.returncode == 0,
            output=running_process.stdout,
            build_dir=self.build_dir,
            example=example,
        )


def run_pio_build(
    board: Board | str, examples: list[str], verbose: bool = False
) -> list[Future[BuildResult]]:
    """Run build for specified examples and platform using new PlatformIO system.

    Args:
        board: Board class instance or board name string (resolved via get_board())
        examples: List of example names to build
        verbose: Enable verbose output
    """
    pio = PlatformIoBuilder(board, verbose)
    futures: list[Future[BuildResult]] = []
    for example in examples:
        futures.append(_EXECUTOR.submit(pio.build, example))
    return futures
