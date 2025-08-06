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

from ci.util.running_process import RunningProcess


_EXECUTOR = ThreadPoolExecutor(max_workers=1)


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
    board: str
    framework: str = "arduino"
    platform: str = "atmelavr"

    def to_platformio_ini(self, lib_ldf_mode: str = "chain") -> str:
        out: list[str] = []
        out.append(f"[env:{self.board}]")
        out.append(f"platform = {self.platform}")
        out.append(f"board = {self.board}")
        out.append(f"framework = {self.framework}")
        out.append(f"lib_ldf_mode = {lib_ldf_mode}")
        # Enable library archiving to create static libraries and avoid recompilation
        out.append("lib_archive = true")
        # Add manual include path for FastLED library
        if lib_ldf_mode == "off":
            # When LDF is off, we need to manually include FastLED source files
            # but with lib_archive=true, they'll be compiled into a static library
            out.append("build_flags = -I lib/FastLED")
            out.append(
                "build_src_filter = +<*> +<../lib/FastLED/**/*.cpp> +<../lib/FastLED/**/*.c>"
            )
        else:
            # When LDF is on, just provide the include path - LDF will handle the rest
            out.append("build_flags = -I lib/FastLED")
        return "\n".join(out)


def resolve_project_root() -> Path:
    """Resolve the FastLED project root directory."""
    current = Path(
        __file__
    ).parent.parent.parent.resolve()  # Go up from ci/compiler/pio.py
    while current != current.parent:
        if (current / "src" / "FastLED.h").exists():
            return current
        current = current.parent
    raise RuntimeError("Could not find FastLED project root")


def copy_example_source(project_root: Path, build_dir: Path, example: str) -> bool:
    """Copy example source to the build directory.

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

    # Copy example source to src directory (PlatformIO requirement)
    src_dir = build_dir / "src"
    if src_dir.exists():
        shutil.rmtree(src_dir)
    src_dir.mkdir(parents=True, exist_ok=True)

    # Copy all files from example directory
    for file_path in example_path.iterdir():
        if file_path.is_file():
            shutil.copy2(file_path, src_dir)

    return True


def copy_boards_directory(project_root: Path, build_dir: Path) -> bool:
    """Copy boards directory to the build directory."""
    boards_src = project_root / "boards"
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


def copy_fastled_library(project_root: Path, build_dir: Path) -> bool:
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


def init_platformio_build(board: str, verbose: bool, example: str) -> InitResult:
    """Initialize the PlatformIO build directory."""
    platform = board

    project_root = resolve_project_root()
    build_dir = project_root / ".build" / "test_platformio" / platform
    # Setup the build directory.
    build_dir.mkdir(parents=True, exist_ok=True)
    platformio_ini = build_dir / "platformio.ini"

    build_config = BuildConfig(board=platform, framework="arduino", platform="atmelavr")
    platformio_ini_content = build_config.to_platformio_ini()

    platformio_ini = build_dir / "platformio.ini"
    platformio_ini.write_text(platformio_ini_content)

    ok_copy_src = copy_example_source(project_root, build_dir, example)
    if not ok_copy_src:
        warnings.warn(f"Example not found: {project_root / 'examples' / example}")
        return InitResult(
            success=False,
            output=f"Example not found: {project_root / 'examples' / example}",
            build_dir=build_dir,
        )

    # Copy FastLED library
    ok_copy_fastled = copy_fastled_library(project_root, build_dir)
    if not ok_copy_fastled:
        warnings.warn(f"Failed to copy FastLED library")
        return InitResult(
            success=False, output=f"Failed to copy FastLED library", build_dir=build_dir
        )

    # Copy boards directory
    ok_copy_boards = copy_boards_directory(project_root, build_dir)
    if not ok_copy_boards:
        warnings.warn(f"Failed to copy boards directory")
        return InitResult(
            success=False,
            output=f"Failed to copy boards directory",
            build_dir=build_dir,
        )

    # Write final platformio.ini
    build_config = BuildConfig(board=platform, framework="arduino", platform="atmelavr")
    platformio_ini_content = build_config.to_platformio_ini(lib_ldf_mode="chain")
    platformio_ini.write_text(platformio_ini_content)

    # Now do an initial build with Blink to ensure everything works
    # Copy Blink example for the initial build
    ok_copy_blink = copy_example_source(project_root, build_dir, "Blink")
    if not ok_copy_blink:
        warnings.warn(f"Blink example not found: {project_root / 'examples' / 'Blink'}")
        return InitResult(
            success=False,
            output=f"Blink example not found: {project_root / 'examples' / 'Blink'}",
            build_dir=build_dir,
        )

    # Run initial build with LDF enabled to set up the environment
    run_cmd: list[str] = ["pio", "run", "--project-dir", str(build_dir)]
    if verbose:
        run_cmd.append("--verbose")

    print(f"Running initial build command: {subprocess.list2cmdline(run_cmd)}")

    running_process = RunningProcess(run_cmd, cwd=build_dir, auto_run=True)
    while line := running_process.get_next_line():
        if line is None:  # End of stream
            break
        print(line)

    running_process.wait()

    if running_process.returncode != 0:
        return InitResult(
            success=False,
            output=f"Initial build failed: {running_process.stdout}",
            build_dir=build_dir,
        )

    # After successful build, turn off the Library Dependency Finder for subsequent builds
    # but keep the manual include path for FastLED
    final_content_no_ldf = build_config.to_platformio_ini(lib_ldf_mode="off")
    platformio_ini.write_text(final_content_no_ldf)

    return InitResult(success=True, output="", build_dir=build_dir)


def run_platform_build(board: str, verbose: bool, example: str) -> BuildResult:
    """Run build for specified example and platform using new PlatformIO system."""
    platform = board

    project_root = resolve_project_root()
    build_dir = project_root / ".build" / "test_platformio" / platform
    # Setup the build directory.
    build_dir.mkdir(parents=True, exist_ok=True)

    build_result = init_platformio_build(board, verbose, example)
    if not build_result.success:
        warnings.warn(f"Build failed: {build_result.output}")
        return BuildResult(
            success=False,
            output=build_result.output,
            build_dir=build_result.build_dir,
            example=example,
        )

    build_dir = build_result.build_dir
    print(f"Build directory: {build_dir}")

    run_cmd: list[str] = ["pio", "run", "--project-dir", str(build_dir)]
    if verbose:
        run_cmd.append("--verbose")
    cwd = build_result.build_dir

    print(f"Running command: {subprocess.list2cmdline(run_cmd)}")

    running_process = RunningProcess(run_cmd, cwd=cwd, auto_run=True)
    while line := running_process.get_next_line():
        if line is None:  # End of stream
            break
        print(line)

    running_process.wait()
    result = BuildResult(
        success=running_process.returncode == 0,
        output=running_process.stdout,
        build_dir=build_dir,
        example=example,
    )
    print(f"Build result: {result.success}")
    if not result.success:
        warnings.warn(f"Build failed: {result.output}")
        return result
    return BuildResult(
        success=True, output=result.output, build_dir=build_dir, example=example
    )


class PlatformIoBuilder:
    def __init__(self, board: str, verbose: bool):
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
        result = init_platformio_build(self.board, self.verbose, "Blink")
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
        project_root = resolve_project_root()
        ok_copy_src = copy_example_source(project_root, self.build_dir, example)
        if not ok_copy_src:
            return BuildResult(
                success=False,
                output=f"Example not found: {project_root / 'examples' / example}",
                build_dir=self.build_dir,
                example=example,
            )

        # Run PlatformIO build
        run_cmd: list[str] = ["pio", "run", "--project-dir", str(self.build_dir)]
        if self.verbose:
            run_cmd.append("--verbose")

        print(f"Running command: {subprocess.list2cmdline(run_cmd)}")

        running_process = RunningProcess(run_cmd, cwd=self.build_dir, auto_run=True)
        try:
            while line := running_process.get_next_line():
                if line is None:  # End of stream
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
    board: str, examples: list[str], verbose: bool = False
) -> list[Future[BuildResult]]:
    """Run build for specified examples and platform using new PlatformIO system."""

    pio = PlatformIoBuilder(board, verbose)
    futures: list[Future[BuildResult]] = []
    for example in examples:
        futures.append(_EXECUTOR.submit(pio.build, example))
    return futures
