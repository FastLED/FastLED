import argparse
import os
import subprocess
import sys
from pathlib import Path


MINIMUM_REPORT_SEVERTIY = "medium"
MINIMUM_FAIL_SEVERTIY = "high"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run cppcheck on the project")
    parser.add_argument("board", nargs="?", help="Board to check, optional")
    return parser.parse_args()


def find_platformio_project_dir(build_root: Path, board_name: str) -> Path | None:
    """Find a directory containing platformio.ini file for the specified board.

    Handles both old and new build directory structures:
    Old: .build/uno/Blink/platformio.ini
    New: .build/fled/examples/uno/Blink/platformio.ini

    Returns the directory containing platformio.ini, or None if not found.
    """
    if not build_root.exists():
        return None

    # Search patterns for different build structures
    search_patterns = [
        # New structure: .build/fled/examples/{board}/**/*.ini
        build_root / "fled" / "examples" / board_name,
        # Old structure: .build/{board}/**/*.ini
        build_root / board_name,
        # Fallback: search all subdirectories for board name
    ]

    # Try each search pattern
    for base_dir in search_patterns:
        if not base_dir.exists():
            continue

        # Look for platformio.ini in this directory and its subdirectories
        project_dir = find_platformio_ini_in_tree(base_dir)
        if project_dir:
            return project_dir

    # Last resort: search the entire build directory for board-related platformio.ini
    for platformio_ini in build_root.rglob("platformio.ini"):
        # Check if this platformio.ini is for our board by looking at the path
        if board_name in str(platformio_ini):
            print(f"Found platformio.ini in {platformio_ini.parent}")
            return platformio_ini.parent

    return None


def find_platformio_ini_in_tree(base_dir: Path) -> Path | None:
    """Recursively search for platformio.ini in a directory tree."""
    if not base_dir.exists():
        return None

    # Check direct subdirectories first (most common case)
    for subdir in base_dir.iterdir():
        if subdir.is_dir():
            platformio_ini = subdir / "platformio.ini"
            if platformio_ini.exists():
                print(f"Found platformio.ini in {subdir}")
                return subdir

    # Check if there's a platformio.ini directly in the base directory
    if (base_dir / "platformio.ini").exists():
        print(f"Found platformio.ini directly in {base_dir}")
        return base_dir

    return None


def main() -> int:
    args = parse_args()
    here = Path(__file__).parent
    project_root = here.parent
    build = project_root / ".build"

    if not build.exists():
        print(f"Build directory {build} not found")
        return 1

    if args.board:
        # Search for the specified board
        project_dir = find_platformio_project_dir(build, args.board)
        if not project_dir:
            print(f"No platformio.ini found for board '{args.board}'")
            print("This usually means the board hasn't been compiled yet.")
            print(f"Try running: uv run ci/ci-compile.py {args.board} --examples Blink")
            return 1
    else:
        # Auto-detect available board by searching for any platformio.ini
        project_dir = None

        # Try to find any platformio.ini file in the build directory
        for platformio_ini in build.rglob("platformio.ini"):
            project_dir = platformio_ini.parent
            print(f"Auto-detected project directory: {project_dir}")
            break

        if not project_dir:
            print(f"No platformio.ini files found in {build}")
            print("This usually means no boards have been compiled yet.")
            print("Try running: uv run ci/ci-compile.py uno --examples Blink")
            return 1

    print(f"Running pio check in {project_dir}")
    os.chdir(str(project_dir))

    # Run pio check command
    cp = subprocess.run(
        [
            "pio",
            "check",
            "--skip-packages",
            "--src-filters=+<src/>",
            f"--severity={MINIMUM_REPORT_SEVERTIY}",
            f"--fail-on-defect={MINIMUM_FAIL_SEVERTIY}",
            "--flags",
            "--inline-suppr",
        ],
    )
    return cp.returncode


if __name__ == "__main__":
    sys.exit(main())
