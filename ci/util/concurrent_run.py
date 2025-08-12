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

from ci.boards import Board  # type: ignore
from ci.compiler.compile_for_board import compile_examples, errors_happened
from ci.util.cpu_count import cpu_count
from ci.util.create_build_dir import create_build_dir
from ci.util.locked_print import locked_print


# Board initialization doesn't take a lot of memory or cpu so it's safe to run in parallel
PARRALLEL_PROJECT_INITIALIZATION = (
    os.environ.get("PARRALLEL_PROJECT_INITIALIZATION", "0") == "1"
)


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

    start_time = time.time()
    create_build_dir(
        board=first_project,
        defines=defines,
        customsdk=customsdk,
        no_install_deps=skip_init,
        extra_packages=extra_packages,
        build_dir=build_dir,
        board_dir=board_dir,
        build_flags=args.build_flags,
        extra_scripts=extra_scripts,
    )
    diff = time.time() - start_time

    msg = f"Build directory created in {diff:.2f} seconds for board"
    locked_print(msg)

    verbose = args.verbose
    # This is not memory/cpu bound but is instead network bound so we can run one thread
    # per board to speed up the process.
    parallel_init_workers = 1 if not PARRALLEL_PROJECT_INITIALIZATION else len(projects)
    # Initialize the build directories for all boards
    locked_print(
        f"Initializing build directories for {len(projects)} boards with {parallel_init_workers} parallel workers"
    )
    with ThreadPoolExecutor(max_workers=parallel_init_workers) as executor:
        future_to_board: Dict[Future[Any], Board] = {}
        for board in projects:
            locked_print(
                f"Submitting build directory initialization for board: {board.board_name}"
            )
            future = executor.submit(
                create_build_dir,
                board,
                defines,
                customsdk,
                skip_init,
                extra_packages,
                build_dir,
                board_dir,
                args.build_flags,
                extra_scripts,
            )
            future_to_board[future] = board

        completed_boards = 0
        failed_boards = 0
        for future in as_completed(future_to_board):
            board = future_to_board[future]
            try:
                success, msg = future.result()
                if not success:
                    locked_print(
                        f"ERROR: Failed to initialize build_dir for board {board.board_name}:\n{msg}"
                    )
                    failed_boards += 1
                    # cancel all other tasks
                    for f in future_to_board:
                        if not f.done():
                            f.cancel()
                            locked_print(
                                "Cancelled initialization for remaining boards due to failure"
                            )
                    return 1
                else:
                    completed_boards += 1
                    locked_print(
                        f"SUCCESS: Finished initializing build_dir for board {board.board_name} ({completed_boards}/{len(projects)})"
                    )
            except Exception as e:
                locked_print(
                    f"EXCEPTION: Build directory initialization failed for board {board.board_name}: {e}"
                )
                failed_boards += 1
                # cancel all other tasks
                for f in future_to_board:
                    if not f.done():
                        f.cancel()
                        locked_print(
                            "Cancelled initialization for remaining boards due to exception"
                        )
                return 1
    init_end_time = time.time()
    init_time = (init_end_time - start_time) / 60
    locked_print(f"\nAll build directories initialized in {init_time:.2f} minutes.")
    errors: list[str] = []
    # Run the compilation process
    num_cpus = max(1, min(cpu_count(), len(projects)))
    with ThreadPoolExecutor(max_workers=num_cpus) as executor:
        future_to_board: Dict[Future[Any], Board] = {
            executor.submit(
                compile_examples,
                board,
                examples + extra_examples.get(board, []),
                build_dir,
                verbose,
                libs=libs,
            ): board
            for board in projects
        }
        for future in as_completed(future_to_board):
            board = future_to_board[future]
            success, msg = future.result()
            if not success:
                msg = f"Compilation failed for board {board}: {msg}"
                errors.append(msg)
                locked_print(f"Compilation failed for board {board}: {msg}.\nStopping.")
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
