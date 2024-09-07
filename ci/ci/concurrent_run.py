import concurrent.futures
import os
import time

from ci.compile_for_board import compile_examples, errors_happened
from ci.cpu_count import cpu_count
from ci.create_build_dir import create_build_dir
from ci.locked_print import locked_print
from ci.project_options import ProjectOptions

# Project initialization doesn't take a lot of memory or cpu so it's safe to run in parallel
PARRALLEL_PROJECT_INITIALIZATION = (
    os.environ.get("PARRALLEL_PROJECT_INITIALIZATION", "1") == "1"
)


def run(
    boards: list[str],
    examples: list[str],
    skip_init: bool,
    defines: list[str],
    extra_packages: list[str],
    build_dir: str | None,
    project_options: dict[str, ProjectOptions],
) -> int:
    start_time = time.time()
    # Necessary to create the first project alone, so that the necessary root directories
    # are created and the subsequent projects can be created in parallel.
    first_board = boards[0]
    first_board_options = project_options.get(first_board)
    create_build_dir(
        first_board,
        project_options=first_board_options,
        defines=defines,
        no_install_deps=skip_init,
        extra_packages=extra_packages,
        build_dir=build_dir,
    )
    # This is not memory/cpu bound but is instead network bound so we can run one thread
    # per board to speed up the process.
    parallel_init_workers = 1 if not PARRALLEL_PROJECT_INITIALIZATION else len(boards)
    # Initialize the build directories for all boards
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=parallel_init_workers
    ) as executor:
        future_to_board: dict[concurrent.futures.Future, str] = {}
        for board in boards:
            future = executor.submit(
                create_build_dir,
                board,
                project_options.get(board),
                defines,
                skip_init,
                extra_packages,
                build_dir,
            )
            future_to_board[future] = board
        for future in concurrent.futures.as_completed(future_to_board):
            board = future_to_board[future]
            success, msg = future.result()
            if not success:
                locked_print(f"Error initializing build_dir for board {board}:\n{msg}")
                # cancel all other tasks
                for f in future_to_board:
                    f.cancel()
                return 1
            else:
                locked_print(f"Finished initializing build_dir for board {board}")
    init_end_time = time.time()
    init_time = (init_end_time - start_time) / 60
    locked_print(f"\nAll build directories initialized in {init_time:.2f} minutes.")
    errors: list[str] = []
    # Run the compilation process
    num_cpus = max(1, min(cpu_count(), len(boards)))
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_cpus) as executor:
        future_to_board = {
            executor.submit(compile_examples, board, examples, build_dir): board
            for board in boards
        }
        for future in concurrent.futures.as_completed(future_to_board):
            board = future_to_board[future]
            success, msg = future.result()
            if not success:
                msg = f"Compilation failed for board {board}: {msg}"
                errors.append(msg)
                locked_print(f"Compilation failed for board {board}: {msg}.\nStopping.")
                for f in future_to_board:
                    f.cancel()
                break
    total_time = (time.time() - start_time) / 60
    if errors_happened():
        locked_print("\nDone. Errors happened during compilation.")
        locked_print("\n".join(errors))
        return 1
    locked_print(
        f"\nDone. Built all projects for all boards in {total_time:.2f} minutes."
    )
    return 0
