import os
import time
from concurrent.futures import Future, ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

from ci.boards import Board
from ci.compile_for_board import compile_examples, errors_happened
from ci.cpu_count import cpu_count
from ci.create_build_dir import create_build_dir
from ci.locked_print import locked_print

# Board initialization doesn't take a lot of memory or cpu so it's safe to run in parallel
PARRALLEL_PROJECT_INITIALIZATION = (
    os.environ.get("PARRALLEL_PROJECT_INITIALIZATION", "1") == "1"
)


@dataclass
class ConcurrentRunArgs:
    projects: list[Board]
    examples: list[Path]
    skip_init: bool
    defines: list[str]
    extra_packages: list[str]
    libs: list[str] | None
    build_dir: str | None
    extra_scripts: str | None
    cwd: str | None
    board_dir: str | None
    build_flags: list[str] | None
    verbose: bool = False


def concurrent_run(
    args: ConcurrentRunArgs,
) -> int:
    projects = args.projects
    examples = args.examples
    skip_init = args.skip_init
    defines = args.defines
    extra_packages = args.extra_packages
    build_dir = args.build_dir
    extra_scripts = args.extra_scripts
    cwd = args.cwd
    start_time = time.time()
    first_project = projects[0]
    prev_cwd: str | None = None
    board_dir = args.board_dir
    libs = args.libs
    if cwd:
        prev_cwd = os.getcwd()
        locked_print(f"Changing to directory {cwd}")
        os.chdir(cwd)
    create_build_dir(
        board=first_project,
        defines=defines,
        no_install_deps=skip_init,
        extra_packages=extra_packages,
        build_dir=build_dir,
        board_dir=board_dir,
        build_flags=args.build_flags,
        extra_scripts=extra_scripts,
    )
    verbose = args.verbose
    # This is not memory/cpu bound but is instead network bound so we can run one thread
    # per board to speed up the process.
    parallel_init_workers = 1 if not PARRALLEL_PROJECT_INITIALIZATION else len(projects)
    # Initialize the build directories for all boards
    with ThreadPoolExecutor(max_workers=parallel_init_workers) as executor:
        future_to_board: dict[Future, Board] = {}
        for board in projects:
            future = executor.submit(
                create_build_dir,
                board,
                defines,
                skip_init,
                extra_packages,
                build_dir,
                board_dir,
                args.build_flags,
                extra_scripts,
            )
            future_to_board[future] = board
        for future in as_completed(future_to_board):
            board = future_to_board[future]
            success, msg = future.result()
            if not success:
                locked_print(
                    f"Error initializing build_dir for board {board.board_name}:\n{msg}"
                )
                # cancel all other tasks
                for f in future_to_board:
                    f.cancel()
                return 1
            else:
                locked_print(
                    f"Finished initializing build_dir for board {board.board_name}"
                )
    init_end_time = time.time()
    init_time = (init_end_time - start_time) / 60
    locked_print(f"\nAll build directories initialized in {init_time:.2f} minutes.")
    errors: list[str] = []
    # Run the compilation process
    num_cpus = max(1, min(cpu_count(), len(projects)))
    with ThreadPoolExecutor(max_workers=num_cpus) as executor:
        future_to_board = {
            executor.submit(
                compile_examples,
                board,
                examples,
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
    return 0
