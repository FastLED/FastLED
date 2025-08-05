#!/usr/bin/env python3
"""
FastLED Target Build Script

Convenience script to run a full build test on any target platform using
the new PlatformIO testing system.
"""

import shutil
import subprocess
import sys
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


import argparse


@dataclass
class Args:
    example: str
    verbose: bool
    platform: str

    @staticmethod
    def parse_args() -> "Args":
        parser = argparse.ArgumentParser(description="FastLED Target Build Script")
        parser.add_argument(
            "--example", default="Blur", help="Example to build (default: Blur)"
        )
        parser.add_argument(
            "--verbose", action="store_true", help="Enable verbose output"
        )
        parser.add_argument(
            "--platform", default="uno", help="Platform to build for (default: uno)"
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


def copy_fastled_library(project_root: Path, build_dir: Path) -> bool:
    """Copy FastLED library to the build directory."""
    lib_dir = build_dir / "lib" / "FastLED"

    sync(project_root / "src", lib_dir, action="sync")

    if lib_dir.exists():
        shutil.rmtree(lib_dir)
    lib_dir.mkdir(parents=True, exist_ok=True)

    # Copy FastLED source files
    fastled_src_path = project_root / "src"
    shutil.copytree(fastled_src_path, lib_dir, dirs_exist_ok=True)

    # Copy library.json to the FastLED lib directory
    library_json_src = project_root / "library.json"
    library_json_dst = lib_dir / "library.json"
    try:
        shutil.copy2(library_json_src, library_json_dst)
    except Exception as e:
        warnings.warn(f"Failed to copy library.json: {e}")
        return False
    return True


def init_platformio_build(args: Args) -> BuildResult:
    """Initialize the PlatformIO build directory."""

    example = args.example
    verbose = args.verbose
    platform = args.platform

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
    # platformio_ini.write_text(_PLATFORMIO_INI)
    build_config = BuildConfig(board=platform, framework="arduino", platform="atmelavr")
    platformio_ini_content = build_config.to_platformio_ini()
    platformio_ini.write_text(platformio_ini_content)

    return BuildResult(success=True, output="", build_dir=build_dir)


def run_platform_build(args: Args) -> BuildResult:
    """Run build for specified example and platform using new PlatformIO system."""
    example = args.example
    verbose = args.verbose
    platform = args.platform

    project_root = resolve_project_root()
    build_dir = project_root / ".build" / "test_platformio" / platform
    # Setup the build directory.
    build_dir.mkdir(parents=True, exist_ok=True)

    build_result = init_platformio_build(args)
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


def main() -> int:
    """Main entry point."""

    args = Args.parse_args()

    result = run_platform_build(args)

    if result.success:
        return 0
    else:
        print(f"Error: {result.output}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
