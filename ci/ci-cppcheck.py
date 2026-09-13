"""Run static analysis on an fbuild-compiled board via its compile database.

fbuild emits `compile_commands.json` into the board's release directory
(`.build/fbuild/<board>/.fbuild/build/release/`), or generates it on demand
with `fbuild <dir> build -e <board> --target compiledb`. The database feeds
`clang-tool-chain-tidy`; check selection lives in the repo-root
`.clang-tidy` file.
"""

import argparse
import json
import sys
from pathlib import Path

from running_process import RunningProcess

from ci.util.fbuild_compiledb import ensure_compile_commands, was_compiled_with_fbuild


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run static analysis on the project")
    parser.add_argument("board", nargs="?", help="Board to check, optional")
    return parser.parse_args(argv)


def resolve_board_name(board_input: str) -> str:
    """Resolve board name using the same logic as ci-compile.py.

    This handles cases where a board has both a board_name (canonical name used for
    build directories) and a real_board_name (vendor board identifier).

    For example: 'blackpill_f411ce' (real_board_name) resolves to 'stm32f411ce' (board_name).

    Args:
        board_input: The board name or alias provided by the user

    Returns:
        The canonical board_name used for build directories
    """
    from ci.boards import create_board

    board = create_board(board_input)
    return board.board_name


def _list_compiled_boards(project_root: Path, build_root: Path) -> list[str]:
    """Names of every board directory under `.build/` that fbuild has compiled."""
    boards: list[str] = []
    for parent in (build_root / "fbuild", build_root):
        if not parent.is_dir():
            continue
        for item in sorted(parent.iterdir()):
            if not item.is_dir() or item.name in boards:
                continue
            if was_compiled_with_fbuild(project_root, build_root, item.name):
                boards.append(item.name)
    return boards


def _load_src_files_from_compile_db(compile_db: Path, project_root: Path) -> list[str]:
    """Return the list of source files in ``compile_db`` under ``<project_root>/src/``.

    Filters out third-party / framework TUs so we only analyze FastLED's own
    code.

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
    ``apt-get install`` / system-package dependency. See FastLED#2302 /
    #2303.

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
    cp = RunningProcess.run(cmd)
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
    else:
        compiled = _list_compiled_boards(project_root, build)
        if not compiled:
            print(f"No fbuild-compiled boards found in {build}")
            print("This usually means no boards have been compiled yet.")
            print("Try running: bash compile uno --examples Blink")
            return 1
        canonical_board_name = compiled[0]
        print(f"Auto-detected board: {canonical_board_name}")

    if not was_compiled_with_fbuild(project_root, build, canonical_board_name):
        print(
            f"No fbuild build found for board '{args.board or canonical_board_name}' "
            f"(canonical name: '{canonical_board_name}')"
        )
        print("This usually means the board hasn't been compiled yet.")
        print(f"Try running: bash compile {canonical_board_name} --examples Blink")
        return 1

    compile_db = ensure_compile_commands(project_root, build, canonical_board_name)
    if compile_db is None:
        print(
            f"ERROR: could not obtain fbuild compile_commands.json for "
            f"'{canonical_board_name}'. fbuild should emit this via "
            f"`fbuild build -e {canonical_board_name} --target compiledb`; "
            f"verify the fbuild version and the env name. Tracking: FastLED#2303.",
            file=sys.stderr,
        )
        return 1
    return run_static_analysis_against_compile_db(compile_db, project_root)


if __name__ == "__main__":
    sys.exit(main())
