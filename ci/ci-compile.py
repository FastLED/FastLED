"""
Runs the compilation process for all examples on all boards in parallel.
Build artifacts are recycled within a board group so that subsequent ino
files are built faster.
"""

import os
import sys
import subprocess
import concurrent.futures
import time
from pathlib import Path
from threading import Lock

ERROR_HAPPENED = False
FIRST_JOB_LOCK = Lock()
IS_GITHUB = "GITHUB_WORKFLOW" in os.environ


EXAMPLES = [
    "Apa102HD",
    "Blink",
    "ColorPalette",
    "ColorTemperature",
    "Cylon",
    "DemoReel100",
    "Fire2012",
    "FirstLight",
    "Multiple/MultipleStripsInOneArray",
    "Multiple/ArrayOfLedArrays",
    "Noise",
    "NoisePlayground",
    "NoisePlusPalette",
    "Pacifica",
    "Pride2015",
    "RGBCalibrate",
    "RGBSetDemo",
    "TwinkleFox",
    "XYMatrix",
]

BOARDS = ["uno", "esp32dev", "esp01", "yun", "digix", "teensy30"]
PRINT_LOCK = Lock()


def locked_print(*args, **kwargs):
    """Print with a lock to prevent garbled output for multiple threads."""
    with PRINT_LOCK:
        print(*args, **kwargs)


def compile_for_board_and_example(board: str, example: str):
    """Compile the given example for the given board."""
    builddir = Path(".build") / board
    builddir = builddir.absolute()
    builddir.mkdir(parents=True, exist_ok=True)
    srcdir = builddir / "src"
    # Remove the previous *.ino file if it exists, everything else is recycled
    # to speed up the next build.
    if srcdir.exists():
        subprocess.run(["rm", "-rf", str(srcdir)], check=True)

    locked_print(f"*** Building example {example} for board {board} ***")
    result = subprocess.run(
        [
            "pio",
            "ci",
            "--board",
            board,
            "--lib=ci",
            "--lib=src",
            "--keep-build-dir",
            f"--build-dir={builddir}",
            f"examples/{example}/*ino",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )

    locked_print(result.stdout)
    if result.returncode != 0:
        locked_print(f"*** Error compiling example {example} for board {board} ***")
        return False
    locked_print(f"*** Finished building example {example} for board {board} ***")
    return True


# Function to process task queues for each board
def process_queue(board: str, examples: list[str]):
    """Process the task queue for the given board."""
    global ERROR_HAPPENED  # pylint: disable=global-statement
    is_first = True
    for example in examples:
        if ERROR_HAPPENED:
            return True
        locked_print(f"\n*** Building examples for board {board} ***")
        if is_first and IS_GITHUB:
            with FIRST_JOB_LOCK:
                # Github runners are memory limited and the first job is the most
                # memory intensive since all the artifacts are being generated in parallel.
                success = compile_for_board_and_example(board, example)
        else:
            success = compile_for_board_and_example(board, example)
        is_first = False
        if not success:
            ERROR_HAPPENED = True
            return False
    return True


def main() -> int:
    """Main function."""
    start_time = time.time()

    # Set the working directory to the script's parent directory.
    script_dir = Path(__file__).parent.resolve()
    os.chdir(script_dir.parent)
    os.environ["PLATFORMIO_EXTRA_SCRIPTS"] = "pre:lib/ci/ci-flags.py"

    task_queues = {board: EXAMPLES.copy() for board in BOARDS}
    num_cpus = min(os.cpu_count(), len(BOARDS))

    # Run the compilation process
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_cpus) as executor:
        future_to_board = {
            executor.submit(process_queue, board, task_queues[board]): board
            for board in BOARDS
        }
        for future in concurrent.futures.as_completed(future_to_board):
            board = future_to_board[future]
            if not future.result():
                locked_print(f"Compilation failed for board {board}. Stopping.")
                break

    total_time = (time.time() - start_time) / 60
    if ERROR_HAPPENED:
        locked_print("\nDone. Errors happened during compilation.")
        return 1
    locked_print(
        f"\nDone. Built all projects for all boards in {total_time:.2f} minutes."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
