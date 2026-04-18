import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

from ci.util.fbuild_compiledb import ensure_compile_commands, was_compiled_with_fbuild


MINIMUM_REPORT_SEVERTIY = "medium"
MINIMUM_FAIL_SEVERTIY = "high"


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run cppcheck on the project")
    parser.add_argument("board", nargs="?", help="Board to check, optional")
    return parser.parse_args(argv)


def resolve_board_name(board_input: str) -> str:
    """Resolve board name using the same logic as ci-compile.py.

    This handles cases where a board has both a board_name (canonical name used for
    build directories) and a real_board_name (PlatformIO board identifier).

    For example: 'blackpill_f411ce' (real_board_name) resolves to 'stm32f411ce' (board_name).

    Args:
        board_input: The board name or alias provided by the user

    Returns:
        The canonical board_name used for build directories
    """
    from ci.boards import create_board

    board = create_board(board_input)
    return board.board_name


def find_platformio_project_dir(build_root: Path, board_name: str) -> Path | None:
    """Find a directory containing platformio.ini file for the specified board.

    Handles multiple build directory structures:
    Modern: .build/pio/uno/Blink/platformio.ini
    Legacy: .build/fled/examples/uno/Blink/platformio.ini
    Old: .build/uno/Blink/platformio.ini

    Returns the directory containing platformio.ini, or None if not found.
    """
    if not build_root.exists():
        return None

    # Search patterns for different build structures
    search_patterns = [
        # Modern PlatformIO structure: .build/pio/{board}/**/*.ini
        build_root / "pio" / board_name,
        # Legacy structure: .build/fled/examples/{board}/**/*.ini
        build_root / "fled" / "examples" / board_name,
        # Old structure: .build/{board}/**/*.ini
        build_root / board_name,
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


def _load_src_files_from_compile_db(compile_db: Path, project_root: Path) -> list[str]:
    """Return the list of source files in ``compile_db`` under ``<project_root>/src/``.

    Filters out third-party / framework TUs so we only analyze FastLED's own
    code, matching the legacy ``pio check --src-filters=+<src/>`` scope.

    Per the `JSON Compilation Database spec
    <https://clang.llvm.org/docs/JSONCompilationDatabase.html>`_, the
    ``file`` field may be absolute OR relative to the entry's ``directory``
    field (which itself is absolute). Resolve relatives against
    ``directory`` (falling back to ``compile_db.parent`` when ``directory``
    is missing) rather than against the Python CWD — otherwise fbuild
    emitting a single relative path would silently drop its TU from the
    analysis set.
    """
    src_root = (project_root / "src").resolve()
    with compile_db.open("r", encoding="utf-8") as fh:
        entries = json.load(fh)
    files: list[str] = []
    for entry in entries:
        raw = entry.get("file")
        if not raw:
            continue
        raw_path = Path(raw)
        if not raw_path.is_absolute():
            directory = entry.get("directory") or compile_db.parent
            raw_path = Path(directory) / raw_path
        try:
            resolved = raw_path.resolve()
        except OSError:
            continue
        try:
            resolved.relative_to(src_root)
        except ValueError:
            continue
        files.append(str(resolved))
    return files


def run_static_analysis_against_compile_db(compile_db: Path, project_root: Path) -> int:
    """Run clang-tidy against a compile_commands.json via ``clang-tool-chain-tidy``.

    Routes entirely through the ``clang-tool-chain`` wrapper (lazily validates
    and fetches its bundled toolchain on first use), so there is no
    ``apt-get install`` / system-package dependency. Replaces the legacy
    ``pio check`` pathway, which fails during CMake configure on
    fbuild-compiled boards because PIO-side metadata (partition CSVs etc.)
    isn't populated — see FastLED#2302 / #2303.

    Check selection is governed by the repo-root ``.clang-tidy`` file, which
    already enables the cppcheck-equivalent families (``bugprone-*``,
    ``clang-analyzer-*``, ``performance-*``, selected ``modernize-*`` /
    ``readability-*``). No inline ``--checks`` override — the file is the
    single source of truth.
    """
    files = _load_src_files_from_compile_db(compile_db, project_root)
    if not files:
        print(
            f"No src/ translation units found in {compile_db} — nothing to "
            f"analyze. (Compile DB may cover only framework code for this "
            f"board.)"
        )
        return 0

    print(
        f"Running clang-tool-chain-tidy against {len(files)} src/ files using "
        f"compile DB: {compile_db}"
    )
    cmd = [
        "uv",
        "run",
        "clang-tool-chain-tidy",
        "-p",
        str(compile_db.parent),
        *files,
    ]
    cp = subprocess.run(cmd)
    return cp.returncode


def main() -> int:
    args = parse_args()
    here = Path(__file__).parent
    project_root = here.parent
    build = project_root / ".build"

    if not build.exists():
        print(f"Build directory {build} not found")
        return 1

    if args.board:
        # Resolve board name to canonical board_name (handles aliases like blackpill_f411ce → stm32f411ce)
        canonical_board_name = resolve_board_name(args.board)
        if canonical_board_name != args.board:
            print(
                f"Resolved board '{args.board}' to canonical name '{canonical_board_name}'"
            )

        # fbuild-backed boards: route cppcheck through the compile DB fbuild
        # emits. Avoids `pio check`'s CMake-configure failure mode (missing
        # partition CSVs etc.) while giving us real cppcheck coverage, not a
        # skip. See FastLED#2303.
        if was_compiled_with_fbuild(build, canonical_board_name):
            compile_db = ensure_compile_commands(
                project_root, build, canonical_board_name
            )
            if compile_db is None:
                print(
                    f"ERROR: could not obtain fbuild compile_commands.json for "
                    f"'{canonical_board_name}'. fbuild >= 2.1 should emit this "
                    f"via `fbuild build -e {canonical_board_name} --target "
                    f"compiledb`; verify the fbuild version and the env name. "
                    f"Tracking: FastLED#2303.",
                    file=sys.stderr,
                )
                return 1
            return run_static_analysis_against_compile_db(compile_db, project_root)

        # Search for the specified board using the canonical name
        project_dir = find_platformio_project_dir(build, canonical_board_name)
        if not project_dir:
            print(
                f"No platformio.ini found for board '{args.board}' (canonical name: '{canonical_board_name}')"
            )
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
