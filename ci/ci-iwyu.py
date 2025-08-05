#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false

import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import List


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run include-what-you-use on the project"
    )
    parser.add_argument("board", nargs="?", help="Board to check, optional")
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Automatically apply suggested fixes using fix_includes",
    )
    parser.add_argument(
        "--mapping-file",
        action="append",
        help="Additional mapping file to use (can be specified multiple times)",
    )
    parser.add_argument(
        "--max-line-length",
        type=int,
        default=100,
        help="Maximum line length for suggestions (default: 100)",
    )
    parser.add_argument("--verbose", action="store_true", help="Enable verbose output")
    return parser.parse_args()


def find_platformio_project_dir(board_dir: Path) -> Path | None:
    """Find a directory containing platformio.ini file in the board's build directory.

    With the new pio ci build system, the structure is:
    .build/uno/Blink/platformio.ini
    .build/uno/SomeExample/platformio.ini

    We need to find one of these example directories that has a platformio.ini file.
    """
    if not board_dir.exists():
        return None

    # Look for subdirectories containing platformio.ini
    for subdir in board_dir.iterdir():
        if subdir.is_dir():
            platformio_ini = subdir / "platformio.ini"
            if platformio_ini.exists():
                print(f"Found platformio.ini in {subdir}")
                return subdir

    # Fallback: check if there's a platformio.ini directly in the board directory (old system)
    if (board_dir / "platformio.ini").exists():
        print(f"Found platformio.ini directly in {board_dir}")
        return board_dir

    return None


def check_iwyu_available() -> bool:
    """Check if include-what-you-use is available in the system"""
    try:
        result = subprocess.run(
            ["include-what-you-use", "--version"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        return result.returncode == 0
    except (
        subprocess.CalledProcessError,
        FileNotFoundError,
        subprocess.TimeoutExpired,
    ):
        return False


def run_iwyu_on_cpp_tests(args: argparse.Namespace) -> int:
    """Run IWYU on the C++ test suite using CMake integration"""
    here = Path(__file__).parent
    project_root = here.parent

    print("Running include-what-you-use on C++ test suite...")

    # Use the existing test infrastructure with --check flag
    cmd = [
        "uv",
        "run",
        "test.py",
        "--cpp",
        "--check",  # This enables IWYU via CMake
        "--clang",
        "--no-interactive",
    ]

    if args.verbose:
        cmd.append("--verbose")

    try:
        result = subprocess.run(cmd, cwd=project_root)
        return result.returncode
    except subprocess.CalledProcessError as e:
        print(f"IWYU analysis failed with return code {e.returncode}")
        return e.returncode


def run_iwyu_on_platformio_project(project_dir: Path, args: argparse.Namespace) -> int:
    """Run IWYU on a PlatformIO project"""
    print(f"Running include-what-you-use in {project_dir}")
    os.chdir(str(project_dir))

    # Build mapping file arguments
    mapping_args: List[str] = []

    # Add FastLED mapping files if they exist
    project_root = project_dir
    while project_root.parent != project_root:  # Find project root
        if (project_root / "ci" / "iwyu").exists():
            break
        project_root = project_root.parent

    fastled_mapping = project_root / "ci" / "iwyu" / "fastled.imp"
    stdlib_mapping = project_root / "ci" / "iwyu" / "stdlib.imp"

    if fastled_mapping.exists():
        mapping_args.extend(["--mapping_file", str(fastled_mapping)])

    if stdlib_mapping.exists():
        mapping_args.extend(["--mapping_file", str(stdlib_mapping)])

    # Add user-specified mapping files
    if args.mapping_file:
        for mapping in args.mapping_file:
            mapping_args.extend(["--mapping_file", mapping])

    # Build IWYU command
    iwyu_cmd = [
        "include-what-you-use",
        f"--max_line_length={args.max_line_length}",
        "--quoted_includes_first",
        "--no_comments",
    ]

    if args.verbose:
        iwyu_cmd.append("--verbose=3")
    else:
        iwyu_cmd.append("--verbose=1")

    iwyu_cmd.extend(mapping_args)

    # Run through PlatformIO's check system with IWYU
    pio_cmd = [
        "pio",
        "check",
        "--skip-packages",
        "--src-filters=+<src/>",
        "--tool=include-what-you-use",
        "--flags",
    ] + iwyu_cmd[1:]  # Skip the include-what-you-use binary name

    try:
        result = subprocess.run(pio_cmd)
        return result.returncode
    except subprocess.CalledProcessError as e:
        print(f"PlatformIO IWYU check failed with return code {e.returncode}")
        return e.returncode


def apply_iwyu_fixes(source_dir: Path) -> int:
    """Apply IWYU fixes using fix_includes tool"""
    try:
        # Check if fix_includes is available
        subprocess.run(["fix_includes", "--help"], capture_output=True, check=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print(
            "Warning: fix_includes tool not found. Install IWYU tools to use --fix option."
        )
        return 1

    # Find all .h and .cpp files
    cpp_files: List[Path] = []
    for pattern in ["**/*.cpp", "**/*.h", "**/*.hpp"]:
        cpp_files.extend(source_dir.glob(pattern))

    if not cpp_files:
        print("No C++ files found to fix")
        return 0

    print(f"Applying IWYU fixes to {len(cpp_files)} files...")

    # Run fix_includes on the source directory
    cmd = ["fix_includes", "--update_comments", str(source_dir)]

    try:
        result = subprocess.run(cmd)
        if result.returncode == 0:
            print("IWYU fixes applied successfully")
        else:
            print(f"fix_includes failed with return code {result.returncode}")
        return result.returncode
    except subprocess.CalledProcessError as e:
        print(f"Failed to apply IWYU fixes: {e}")
        return e.returncode


def main() -> int:
    args = parse_args()
    here = Path(__file__).parent
    project_root = here.parent

    # Check if IWYU is available
    if not check_iwyu_available():
        print("Error: include-what-you-use not found in PATH")
        print("Install it with:")
        print("  Ubuntu/Debian: sudo apt install iwyu")
        print("  macOS: brew install include-what-you-use")
        print("  Or build from source: https://include-what-you-use.org/")
        return 1

    print("Found include-what-you-use")

    # If no board specified, run on C++ test suite
    if not args.board:
        print("No board specified, running IWYU on C++ test suite")
        return run_iwyu_on_cpp_tests(args)

    # Run on specific board
    build = project_root / ".build"

    if not build.exists():
        print(f"Build directory {build} not found")
        print("Run a compilation first: ./compile [board] --examples [example]")
        return 1

    board_dir = build / args.board
    if not board_dir.exists():
        print(f"Board {args.board} not found in {build}")
        print("Available boards:")
        for d in build.iterdir():
            if d.is_dir():
                print(f"  {d.name}")
        return 1

    project_dir = find_platformio_project_dir(board_dir)
    if not project_dir:
        print(f"No platformio.ini found in {board_dir} or its subdirectories")
        print("This usually means the board hasn't been compiled yet.")
        print(f"Try running: ./compile {args.board} --examples Blink")
        return 1

    # Run IWYU on the PlatformIO project
    result = run_iwyu_on_platformio_project(project_dir, args)

    # Apply fixes if requested and analysis succeeded
    if args.fix and result == 0:
        src_dir = project_dir / "src"
        if src_dir.exists():
            fix_result = apply_iwyu_fixes(src_dir)
            if fix_result != 0:
                result = fix_result

    return result


if __name__ == "__main__":
    sys.exit(main())
