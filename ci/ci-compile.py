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
import shutil


IS_GITHUB = "GITHUB_ACTIONS" in os.environ
FIRST_BUILD_LOCK = Lock()
USE_FIRST_BUILD_LOCK = IS_GITHUB

# Project initialization doesn't take a lot of memory or cpu so it's safe to run in parallel
PARRALLEL_PROJECT_INITIALIZATION = os.environ.get("PARRALLEL_PROJECT_INITIALIZATION", "1") == "1"

# An open source version of the esp-idf 5.1 platform for the ESP32 that
# gives esp32 boards the same build environment as the Arduino 2.3.1+.

# Set to a specific release, we may want to update this in the future.
ESP32_IDF_5_1 = "https://github.com/pioarduino/platform-espressif32/releases/download/51.03.04/platform-espressif32.zip"
ESP32_IDF_5_1_LATEST = "https://github.com/pioarduino/platform-espressif32.git#develop"
# Top of trunk.
#ESP32_IDF_5_1 = "https://github.com/pioarduino/platform-espressif32"

# Old fork that we were using
# ESP32_IDF_5_1 = "https://github.com/zackees/platform-espressif32#Arduino/IDF5"


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

# Default boards to compile for. You can use boards not defined here but
# if the board isn't part of the officially supported platformio boards then
# you will need to add the board to the ~/.platformio/platforms directory.
# prior to running this script. This happens automatically as of 2024-08-20
# with the github workflow scripts.
BOARDS = [
    "uno",  # Build is faster if this is first, because it's used for global init.
    "esp32dev",
    "esp01",  # ESP8266
    "esp32-c3-devkitm-1",
    # "esp32-c6-devkitc-1",
    # "esp32-c2-devkitm-1",
    "esp32-s3-devkitc-1",
    "yun",
    "digix",
    "teensy30",
    "teensy41"
]



CUSTOM_PROJECT_OPTIONS = {
    "esp32dev": [f"platform={ESP32_IDF_5_1}"],
    #"esp01": f"platform={ESP32_IDF_5_1}",
    "esp32-c2-devkitm-1": [f"platform={ESP32_IDF_5_1_LATEST}"],
    "esp32-c3-devkitm-1": [f"platform={ESP32_IDF_5_1}"],
    "esp32-c6-devkitc-1": [f"platform={ESP32_IDF_5_1}"],
    "esp32-s3-devkitc-1": [f"platform={ESP32_IDF_5_1}"],
    "esp32-h2-devkitm-1": [f"platform={ESP32_IDF_5_1_LATEST}"],
    "adafruit_feather_nrf52840_sense": ["platform=nordicnrf52"],
    "rpipico2": [
        #"platform=https://github.com/earlephilhower/arduino-pico.git",
        "platform=https://github.com/maxgerhardt/platform-raspberrypi.git",
        "platform_packages=framework-arduinopico@https://github.com/earlephilhower/arduino-pico.git",
        "framework=arduino",
        "board_build.core=earlephilhower",
        "board_build.filesystem_size=0.5m"
    ],
    "uno_r4_wifi": [
        "platform=renesas-ra",
        "board=uno_r4_wifi"
    ],
    "nano_every": [
        "platform=atmelmegaavr"
    ]
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


def compile_for_board_and_example(board: str, example: str, build_dir: str | None) -> tuple[bool, str]:
    """Compile the given example for the given board."""
    builddir = Path(build_dir) / board if build_dir else Path(".build") / board
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
    ]
    cmd_list.append(f"examples/{example}/*ino")
    cmd_str = subprocess.list2cmdline(cmd_list)
    locked_print(f"Running command: {cmd_str}")
    result = subprocess.run(
        cmd_list,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )


    stdout = result.stdout
    # replace all instances of "lib/src" => "src" so intellisense can find the files
    # with one click.
    stdout = stdout.replace("lib/src", "src")
    locked_print(stdout)
    if result.returncode != 0:
        locked_print(f"*** Error compiling example {example} for board {board} ***")
        return False, stdout
    locked_print(f"*** Finished building example {example} for board {board} ***")
    return True, stdout

def create_build_dir(board: str, project_options: list[str] | None, defines: list[str], no_install_deps: bool, extra_packages: list[str], build_dir: str | None) -> tuple[bool, str]:
    """Create the build directory for the given board."""
    locked_print(f"*** Initializing environment for board {board} ***")
    builddir = Path(build_dir) / board if build_dir else Path(".build") / board
    builddir.mkdir(parents=True, exist_ok=True)
    # if lib directory (where FastLED lives) exists, remove it. This is necessary to run on
    # recycled build directories for fastled to update. This is a fast operation.
    srcdir = builddir / "lib"
    if srcdir.exists():
        shutil.rmtree(srcdir)
    platformio_ini = builddir / "platformio.ini"
    if platformio_ini.exists():
        try:
            platformio_ini.unlink()
        except OSError as e:
            locked_print(f"Error removing {platformio_ini}: {e}")
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
        for project_option in project_options:
            cmd_list.append(f'--project-option={project_option}')
    if defines:
        build_flags = ' '.join(f'-D {define}' for define in defines)
        cmd_list.append(f'--project-option=build_flags={build_flags}')
    if extra_packages:
        cmd_list.append(f'--project-option=lib_deps={",".join(extra_packages)}')
    if no_install_deps:
        cmd_list.append("--no-install-dependencies")
    cmd_str = subprocess.list2cmdline(cmd_list)
    locked_print(f"Running command: {cmd_str}")
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
def compile_examples(board: str, examples: list[str], build_dir: str | None) -> tuple[bool, str]:
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
                success, message = compile_for_board_and_example(board=board, example=example, build_dir=build_dir)
        else:
            success, message = compile_for_board_and_example(board=board, example=example, build_dir=build_dir)
        is_first = False
        if not success:
            ERROR_HAPPENED = True
            return False, f"Error building {example} for board {board}. stdout:\n{message}"
    return True, ""


def parse_args():
    parser = argparse.ArgumentParser(description="Compile FastLED examples for various boards.")
    parser.add_argument("--boards", type=str, help="Comma-separated list of boards to compile for")
    parser.add_argument("--examples", type=str, help="Comma-separated list of examples to compile")
    parser.add_argument("--skip-init", action="store_true", help="Skip the initialization step")
    parser.add_argument("--defines", type=str, help="Comma-separated list of compiler definitions")
    parser.add_argument("--extra-packages", type=str, help="Comma-separated list of extra packages to install")
    parser.add_argument("--build-dir", type=str, help="Override the default build directory")
    parser.add_argument("--no-project-options", action="store_true", help="Don't use custom project options")
    return parser.parse_args()


def run(boards: list[str], examples: list[str], skip_init: bool, defines: list[str], extra_packages: list[str], build_dir: str | None) -> int:
    start_time = time.time()
    # Necessary to create the first project alone, so that the necessary root directories
    # are created and the subsequent projects can be created in parallel.
    first_board = boards[0]
    first_board_options = CUSTOM_PROJECT_OPTIONS.get(first_board)
    create_build_dir(
        first_board,
        project_options=first_board_options,
        defines=defines,
        no_install_deps=skip_init,
        extra_packages=extra_packages,
        build_dir=build_dir)
    # This is not memory/cpu bound but is instead network bound so we can run one thread
    # per board to speed up the process.
    parallel_init_workers = 1 if not PARRALLEL_PROJECT_INITIALIZATION else len(boards)
    # Initialize the build directories for all boards
    with concurrent.futures.ThreadPoolExecutor(max_workers=parallel_init_workers) as executor:

        future_to_board: dict[concurrent.futures.Future, str] = {}
        for board in boards:
            future = executor.submit(create_build_dir, board, CUSTOM_PROJECT_OPTIONS.get(board), defines, skip_init, extra_packages, build_dir)
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
    skip_init = args.skip_init
    # Set the working directory to the script's parent directory.
    script_dir = Path(__file__).parent.resolve()
    locked_print(f"Changing working directory to {script_dir.parent}")
    os.chdir(script_dir.parent)
    os.environ["PLATFORMIO_EXTRA_SCRIPTS"] = "pre:lib/ci/ci-flags.py"
    if args.no_project_options:
        CUSTOM_PROJECT_OPTIONS.clear()

    boards = args.boards.split(',') if args.boards else BOARDS
    examples = args.examples.split(',') if args.examples else EXAMPLES
    defines: list[str] = []
    if args.defines:
        defines.extend(args.defines.split(','))
    extract_packages: list[str] = []
    if args.extra_packages:
        extract_packages.extend(args.extra_packages.split(','))
    build_dir = args.build_dir
    rtn = run(boards=boards, examples=examples, skip_init=skip_init, defines=defines, extra_packages=extract_packages, build_dir=build_dir)
    return rtn



if __name__ == "__main__":
    sys.exit(main())
