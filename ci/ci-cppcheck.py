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


def main() -> int:
    args = parse_args()
    here = Path(__file__).parent
    project_root = here.parent
    build = project_root / ".build"

    if not build.exists():
        print(f"Build directory {build} not found")
        return 1

    if args.board:
        board_dir = build / args.board
        if not board_dir.exists():
            print(f"Board {args.board} not found")
            return 1

        project_dir = find_platformio_project_dir(board_dir)
        if not project_dir:
            print(f"No platformio.ini found in {board_dir} or its subdirectories")
            print("This usually means the board hasn't been compiled yet.")
            print("Try running: ./compile {args.board} --examples Blink")
            return 1
    else:
        # Change to the first subdirectory in .build
        subdirs = [d for d in build.iterdir() if d.is_dir()]
        if not subdirs:
            print(f"No subdirectories found in {build}")
            return 1

        if len(subdirs) != 1:
            print(
                f"Expected exactly one subdirectory in {build}, instead got {[d.name for d in subdirs]}"
            )
            return 1

        board_dir = subdirs[0]
        project_dir = find_platformio_project_dir(board_dir)
        if not project_dir:
            print(f"No platformio.ini found in {board_dir} or its subdirectories")
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
