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
import argparse


IS_GITHUB = "GITHUB_ACTIONS" in os.environ
FIRST_BUILD_LOCK = Lock()
USE_FIRST_BUILD_LOCK = IS_GITHUB

# Project initialization doesn't take a lot of memory or cpu so it's safe to run in parallel
PARRALLEL_PROJECT_INITIALIZATION = os.environ.get("PARRALLEL_PROJECT_INITIALIZATION", "1") == "1"

# An open source version of the esp-idf 5.1 platform for the ESP32 that
# gives esp32 boards the same build environment as the Arduino 2.3.1+.
ESP32_IDF_5_1 = "https://github.com/zackees/platform-espressif32#Arduino/IDF5"

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

BOARDS = [
    "uno",  # Build is faster if this is first, because it's used for global init.
    "esp32dev",
    "esp01",  # ESP8266
    "esp32-c3-devkitm-1",
    # "esp32-c6-devkitc-1", # doesn't work yet.
    "esp32-s3-devkitc-1",
    "yun",
    "digix",
    "teensy30"
]



CUSTOM_PROJECT_OPTIONS = {
    "esp32dev": f"platform={ESP32_IDF_5_1}",
    #"esp01": f"platform={ESP32_IDF_5_1}",
    "esp32-c3-devkitm-1": f"platform={ESP32_IDF_5_1}",
    "esp32-c6-devkitc-1": f"platform={ESP32_IDF_5_1}",
    "esp32-s3-devkitc-1": f"platform={ESP32_IDF_5_1}",
}

ERROR_HAPPENED = False
PRINT_LOCK = Lock()

def cpu_count() -> int:
    """Get the number of CPUs."""
    if "GITHUB_ACTIONS" in os.environ:
        return 4
    return os.cpu_count()
    


def locked_print(*args, **kwargs):
    """Print with a lock to prevent garbled output for multiple threads."""
    with PRINT_LOCK:
        print(*args, **kwargs)


def compile_for_board_and_example(board: str, example: str) -> tuple[bool, str]:
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
    cmd_list = [
        "pio",
        "ci",
        "--board",
        board,
        "--lib=ci",
        "--lib=src",
        "--keep-build-dir",
        f"--build-dir={builddir}",
        f"examples/{example}/*ino",
    ]
    result = subprocess.run(
        cmd_list,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )

    locked_print(result.stdout)
    if result.returncode != 0:
        locked_print(f"*** Error compiling example {example} for board {board} ***")
        return False, result.stdout
    locked_print(f"*** Finished building example {example} for board {board} ***")
    return True, result.stdout

def create_build_dir(board: str, project_options: str | None) -> tuple[bool, str]:
    """Create the build directory for the given board."""
    locked_print(f"*** Initializing environment for board {board} ***")
    builddir = Path(".build") / board
    builddir = builddir.absolute()
    builddir.mkdir(parents=True, exist_ok=True)

    cmd_list = [
        "pio",
        "project",
        "init",
        "--project-dir",
        str(builddir),
        "--board",
        board,
    ]
    if project_options:
        cmd_list.append(f'--project-option={project_options}')
    result = subprocess.run(
        cmd_list,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    stdout = result.stdout

    locked_print(result.stdout)
    if result.returncode != 0:
        locked_print(f"*** Error setting up project for board {board} ***")
        return False, stdout
    locked_print(f"*** Finished initializing environment for board {board} ***")
    return True, stdout


# Function to process task queues for each board
def compile_examples(board: str, examples: list[str]) -> tuple[bool, str]:
    """Process the task queue for the given board."""
    global ERROR_HAPPENED  # pylint: disable=global-statement
    is_first = True
    for example in examples:
        if ERROR_HAPPENED:
            return True
        locked_print(f"\n*** Building {example} for board {board} ***")
        if is_first:
            locked_print(f"*** Building for first example {example} board {board} ***")
        if is_first and USE_FIRST_BUILD_LOCK:
            with FIRST_BUILD_LOCK:
                # Github runners are memory limited and the first job is the most
                # memory intensive since all the artifacts are being generated in parallel.
                success, message = compile_for_board_and_example(board=board, example=example)
        else:
            success, message = compile_for_board_and_example(board=board, example=example)
        is_first = False
        if not success:
            ERROR_HAPPENED = True
            return False, f"Error building {example} for board {board}. stdout:\n{message}"
    return True, ""


def parse_args():
    parser = argparse.ArgumentParser(description="Compile FastLED examples for various boards.")
    parser.add_argument("--boards", type=str, help="Comma-separated list of boards to compile for")
    parser.add_argument("--examples", type=str, help="Comma-separated list of examples to compile")
    return parser.parse_args()


def run(boards: list[str], examples: list[str]) -> int:
    start_time = time.time()
    # Necessary to create the first project alone, so that the necessary root directories
    # are created and the subsequent projects can be created in parallel.
    create_build_dir(boards[0], CUSTOM_PROJECT_OPTIONS.get(boards[0]))
    # This is not memory/cpu bound but is instead network bound so we can run one thread
    # per board to speed up the process.
    parallel_init_workers = 1 if not PARRALLEL_PROJECT_INITIALIZATION else len(boards)
    # Initialize the build directories for all boards
    with concurrent.futures.ThreadPoolExecutor(max_workers=parallel_init_workers) as executor:
        future_to_board = {
            executor.submit(create_build_dir, board, CUSTOM_PROJECT_OPTIONS.get(board)): board
            for board in boards
        }
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
    locked_print("\nAll build directories initialized.")
    errors: list[str] = []
    # Run the compilation process
    num_cpus = max(1, min(cpu_count(), len(boards)))
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_cpus) as executor:
        future_to_board = {
            executor.submit(compile_examples, board, examples): board
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
    if ERROR_HAPPENED:
        locked_print("\nDone. Errors happened during compilation.")
        locked_print("\n".join(errors))
        return 1
    locked_print(
        f"\nDone. Built all projects for all boards in {total_time:.2f} minutes."
    )
    return 0


def main() -> int:
    """Main function."""
    args = parse_args()
    # Set the working directory to the script's parent directory.
    script_dir = Path(__file__).parent.resolve()
    locked_print(f"Changing working directory to {script_dir.parent}")
    os.chdir(script_dir.parent)
    os.environ["PLATFORMIO_EXTRA_SCRIPTS"] = "pre:lib/ci/ci-flags.py"

    boards = args.boards.split(',') if args.boards else BOARDS
    examples = args.examples.split(',') if args.examples else EXAMPLES

    invalid_boards = set(boards) - set(BOARDS)
    invalid_examples = set(examples) - set(EXAMPLES)
    
    if invalid_boards:
        locked_print(f"Error: Invalid boards specified: {', '.join(invalid_boards)}")
    if invalid_examples:
        locked_print(f"Error: Invalid examples specified: {', '.join(invalid_examples)}")

    return run(boards, examples)



if __name__ == "__main__":
    sys.exit(main())
