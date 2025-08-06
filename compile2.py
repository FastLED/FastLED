#!/usr/bin/env python3
"""
FastLED Target Build Script

Convenience script to run a full build test on any target platform using
the new PlatformIO testing system.
"""

import shutil
import subprocess
import sys
import argparse
import warnings
from dataclasses import dataclass
from pathlib import Path

from dirsync import sync  # type: ignore

from ci.util.running_process import RunningProcess


@dataclass
class BuildConfig:
    board: str
    framework: str = "arduino"
    platform: str = "atmelavr"

    def to_platformio_ini(self) -> str:
        out: list[str] = []
        out.append(f"[env:{self.board}]")
        out.append(f"platform = {self.platform}")
        out.append(f"board = {self.board}")
        out.append(f"framework = {self.framework}")
        return "\n".join(out)


@dataclass
class BuildResult:
    success: bool
    output: str
    build_dir: Path

    @property
    def platformio_ini(self) -> Path:
        return self.build_dir / "platformio.ini"





@dataclass
class Args:
    board: str
    examples: list[str]
    verbose: bool

    @staticmethod
    def parse_args() -> "Args":
        parser = argparse.ArgumentParser(description="FastLED Target Build Script")
        parser.add_argument(
            "board", help="Board to build for (required)"
        )
        parser.add_argument(
            "examples", nargs="+", help="One or more examples to build (required)"
        )
        parser.add_argument(
            "--verbose", action="store_true", help="Enable verbose output"
        )
        return Args(**vars(parser.parse_args()))


def resolve_project_root() -> Path:
    """Resolve the FastLED project root directory."""
    current = Path(__file__).parent.resolve()
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

    # Copy library.json to the lib directory (not inside FastLED symlink)
    library_json_src = project_root / "library.json"
    library_json_dst = lib_dir / "library.json"
    try:
        shutil.copy2(library_json_src, library_json_dst)
    except Exception as e:
        warnings.warn(f"Failed to copy library.json: {e}")
        return False
    return True


def init_platformio_build(board: str, verbose: bool, example: str) -> BuildResult:
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
        return BuildResult(
            success=False,
            output=f"Example not found: {project_root / 'examples' / example}",
            build_dir=build_dir,
        )

    # Copy FastLED library
    ok_copy_fastled = copy_fastled_library(project_root, build_dir)
    if not ok_copy_fastled:
        warnings.warn(f"Failed to copy FastLED library")
        return BuildResult(
            success=False, output=f"Failed to copy FastLED library", build_dir=build_dir
        )
    
    # Copy boards directory
    ok_copy_boards = copy_boards_directory(project_root, build_dir)
    if not ok_copy_boards:
        warnings.warn(f"Failed to copy boards directory")
        return BuildResult(
            success=False, output=f"Failed to copy boards directory", build_dir=build_dir
        )
    # platformio_ini.write_text(_PLATFORMIO_INI)
    build_config = BuildConfig(board=platform, framework="arduino", platform="atmelavr")
    platformio_ini_content = build_config.to_platformio_ini()
    platformio_ini.write_text(platformio_ini_content)

    return BuildResult(success=True, output="", build_dir=build_dir)


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
        return build_result

    build_dir = build_result.build_dir
    print(f"Build directory: {build_dir}")

    run_cmd: list[str] = ["pio", "run", "--project-dir", str(build_dir)]
    if verbose:
        run_cmd.append("--verbose")
    cwd = build_result.build_dir

    print(f"Running command: {subprocess.list2cmdline(run_cmd)}")
    # result = subprocess.run(run_cmd, capture_output=True, text=True, cwd=cwd)

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
    )
    print(f"Build result: {result.success}")
    # print(f"Build output: {result.output}")
    if not result.success:
        warnings.warn(f"Build failed: {result.output}")
        return result
    return BuildResult(success=True, output=result.output, build_dir=build_dir)


def _run_build(build_dir: Path, verbose: bool) -> BuildResult:
    """Run PlatformIO build in the specified directory."""

    print(f"Build directory: {build_dir}")

    run_cmd: list[str] = ["pio", "run", "--project-dir", str(build_dir)]
    if verbose:
        run_cmd.append("--verbose")
    cwd = build_dir

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
    )
    print(f"Build result: {result.success}")
    if not result.success:
        warnings.warn(f"Build failed: {result.output}")
        return result
    return BuildResult(success=True, output=result.output, build_dir=build_dir)


class PlatformIoBuilder:
    def __init__(self, board: str, verbose: bool):
        self.board = board
        self.verbose = verbose
        self.build_dir: Path | None = None
        self.initialized = False

    def init_build(self) -> BuildResult:
        """Initialize the PlatformIO build directory once."""
        if self.initialized and self.build_dir is not None:
            return BuildResult(success=True, output="Already initialized", build_dir=self.build_dir)
        
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
                return init_result
        
        # Build with specific parameters
        return run_platform_build(self.board, self.verbose, example)


def main() -> int:
    """Main entry point."""

    args = Args.parse_args()

    # Create PlatformIO builder with only the parameters it needs
    builder = PlatformIoBuilder(board=args.board, verbose=args.verbose)
    
    # Initialize build environment
    init_result = builder.init_build()
    if not init_result.success:
        print(f"Failed to initialize build: {init_result.output}")
        return 1

    results: list[BuildResult] = []
    for example in args.examples:
        print(f"Building example: {example}")
        
        # Build the example
        result = builder.build(example)
        results.append(result)
        if not result.success:
            print(f"Failed to build {example}")
        else:
            print(f"Successfully built {example}")

    failed_builds = [(args.examples[i], result) for i, result in enumerate(results) if not result.success]
    if failed_builds:
        print(f"Failed to build {len(failed_builds)} out of {len(results)} examples")
        for example_name, result in failed_builds:
            print(f"Error building {example_name}: {result.output}")
        return 1

    print(f"Successfully built all {len(results)} examples")
    return 0


if __name__ == "__main__":
    sys.exit(main())
