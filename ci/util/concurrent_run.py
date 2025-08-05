# pyright: reportUnknownMemberType=false
"""
Concurrent run utilities.
"""

import os
import subprocess
import time
from concurrent.futures import Future, ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

from ci.compiler.compile_for_board import errors_happened
from ci.compiler.pio_run_compiler import PioRunCompiler
from ci.util.boards import Board  # type: ignore
from ci.util.cpu_count import cpu_count
from ci.util.locked_print import locked_print


# Board initialization doesn't take a lot of memory or cpu so it's safe to run in parallel
PARRALLEL_PROJECT_INITIALIZATION = (
    os.environ.get("PARRALLEL_PROJECT_INITIALIZATION", "0") == "1"
)


def compile_examples_pio_run(
    board: Board,
    examples: list[Path],
    build_dir: str | None,
    verbose_on_failure: bool,
    libs: list[str] | None,
    defines: list[str] | None = None,
    clean_first: bool = False,
    incremental: bool = True,
    keep: bool = False,
) -> tuple[bool, str]:
    """Compile examples using efficient pio run system."""
    try:
        compiler = PioRunCompiler(build_dir)
        return compiler.compile_examples(
            board=board,
            examples=examples,
            defines=defines,
            verbose=verbose_on_failure,
            clean_first=clean_first,
            incremental=incremental,
            keep=keep,
        )
    except Exception as e:
        return False, f"Exception in pio run compilation: {e}"


def _banner_print(msg: str) -> None:
    """Print a banner message."""
    # will produce
    #######
    # msg #
    #######
    lines = msg.splitlines()
    for line in lines:
        print("#" * (len(line) + 4))
        print(f"# {line} #")
        print("#" * (len(line) + 4))


@dataclass
class ConcurrentRunArgs:
    projects: list[Board]
    examples: list[Path]
    skip_init: bool
    defines: list[str]
    customsdk: str | None
    extra_packages: list[str]
    libs: list[str] | None
    build_dir: str | None
    extra_scripts: str | None
    cwd: str | None
    board_dir: str | None
    build_flags: list[str] | None
    verbose: bool = False
    extra_examples: dict[Board, list[Path]] | None = None
    symbols: bool = False
    # Pio run options
    clean_projects: bool = False
    disable_incremental: bool = False
    keep: bool = False


def concurrent_run(
    args: ConcurrentRunArgs,
) -> int:
    projects = args.projects
    examples = args.examples
    skip_init = args.skip_init
    defines = args.defines
    customsdk = args.customsdk
    extra_packages = args.extra_packages
    build_dir = args.build_dir
    extra_scripts = args.extra_scripts
    cwd = args.cwd
    start_time = time.time()
    first_project = projects[0]
    prev_cwd: str | None = None
    board_dir = args.board_dir
    libs = args.libs
    extra_examples: dict[Board, list[Path]] = args.extra_examples or {}
    if cwd:
        prev_cwd = os.getcwd()
        locked_print(f"Changing to directory {cwd}")
        os.chdir(cwd)

    # Legacy create_build_dir removed - pio run system handles its own initialization

    verbose = args.verbose
    # Legacy build directory initialization removed - pio run system handles its own initialization
    errors: list[str] = []
    # Run the compilation process
    num_cpus = max(1, min(cpu_count(), len(projects)))

    # Use efficient pio run build system
    locked_print("Using efficient pio run build system")

    with ThreadPoolExecutor(max_workers=num_cpus) as executor:
        future_to_board: Dict[Future[Any], Board] = {}

        for board in projects:
            # Use pio run system with additional parameters
            future = executor.submit(
                compile_examples_pio_run,
                board,
                examples + extra_examples.get(board, []),
                build_dir,
                verbose,
                libs,
                defines=defines,  # Pass defines for pio run
                clean_first=args.clean_projects,  # Clean first if requested
                incremental=not args.disable_incremental,  # Incremental flag
                keep=args.keep,  # Pass keep flag
            )
            future_to_board[future] = board

        for future in as_completed(future_to_board):
            board = future_to_board[future]
            success, msg = future.result()
            if not success:
                msg = f"Compilation failed for board {board.board_name}: {msg}"
                errors.append(msg)
                locked_print(
                    f"Compilation failed for board {board.board_name}: {msg}.\nStopping."
                )
                for f in future_to_board:
                    f.cancel()
                break
    if prev_cwd:
        locked_print(f"Changing back to directory {prev_cwd}")
        os.chdir(prev_cwd)
    if errors_happened():
        locked_print("\nDone. Errors happened during compilation.")
        locked_print("\n".join(errors))
        return 1

    # Run symbol analysis if requested
    if args.symbols:
        locked_print("\nRunning symbol analysis on compiled outputs...")
        symbol_analysis_errors: List[str] = []

        for board in projects:
            try:
                locked_print(f"Running symbol analysis for board: {board.board_name}")

                # Run the symbol analysis tool for this board
                cmd = [
                    "uv",
                    "run",
                    "ci/util/symbol_analysis.py",
                    "--board",
                    board.board_name,
                ]

                result = subprocess.run(
                    cmd, capture_output=True, text=True, cwd=cwd or os.getcwd()
                )

                if result.returncode != 0:
                    error_msg = f"Symbol analysis failed for board {board.board_name}: {result.stderr}"
                    symbol_analysis_errors.append(error_msg)
                    locked_print(f"ERROR: {error_msg}")
                else:
                    locked_print(
                        f"Symbol analysis completed for board: {board.board_name}"
                    )
                    # Print the symbol analysis output
                    if result.stdout:
                        print(result.stdout)

            except Exception as e:
                error_msg = f"Exception during symbol analysis for board {board.board_name}: {e}"
                symbol_analysis_errors.append(error_msg)
                locked_print(f"ERROR: {error_msg}")

        if symbol_analysis_errors:
            locked_print(
                f"\nSymbol analysis completed with {len(symbol_analysis_errors)} error(s):"
            )
            for error in symbol_analysis_errors:
                locked_print(f"  - {error}")
        else:
            locked_print(
                f"\nSymbol analysis completed successfully for all {len(projects)} board(s)."
            )

    return 0
