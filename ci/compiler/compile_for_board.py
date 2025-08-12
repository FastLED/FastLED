# pyright: reportUnknownMemberType=false
"""
Compilation support for individual boards.
"""

import os
import shutil
import subprocess
import time
from pathlib import Path
from threading import Lock
from typing import List

from ci.boards import Board  # type: ignore
from ci.util.locked_print import locked_print


ERROR_HAPPENED = False


IS_GITHUB = "GITHUB_ACTIONS" in os.environ
FIRST_BUILD_LOCK = Lock()
USE_FIRST_BUILD_LOCK = IS_GITHUB


def errors_happened() -> bool:
    """Return whether any errors happened during the build."""
    return ERROR_HAPPENED


def _fastled_js_is_parent_directory(p: Path) -> bool:
    """Check if fastled_js is a parent directory of the given path."""
    # Check if fastled_js is a parent directory of p
    return "fastled_js" in str(p.absolute())


def compile_for_board_and_example(
    board: Board,
    example: Path,
    build_dir: str | None,
    verbose_on_failure: bool,
    libs: list[str] | None,
) -> tuple[bool, str]:
    """Compile the given example for the given board."""
    global ERROR_HAPPENED  # pylint: disable=global-statement
    if board.board_name == "web":
        locked_print(f"Skipping web target for example {example}")
        return True, ""
    board_name = board.board_name
    use_pio_run = board.use_pio_run
    real_board_name = board.get_real_board_name()
    libs = libs or []
    builddir = (
        Path(build_dir) / board_name if build_dir else Path(".build") / board_name
    )
    builddir.mkdir(parents=True, exist_ok=True)
    srcdir = builddir / "src"
    # Remove the previous *.ino file if it exists, everything else is recycled
    # to speed up the next build.
    if srcdir.exists():
        shutil.rmtree(srcdir, ignore_errors=False)
    locked_print(f"*** Building example {example} for board {board_name} ***")
    cwd: str | None = None
    shell: bool = False
    # Copy all files from the example directory to the "src" directory
    for src_file in example.rglob("*"):
        if src_file.is_file():
            if _fastled_js_is_parent_directory(src_file):
                # Skip the fastled_js folder, it's not needed for the build.
                continue
            src_dir = src_file.parent
            path = src_dir.relative_to(example)
            dst_dir = srcdir / path
            os.makedirs(dst_dir, exist_ok=True)
            locked_print(f"Copying {src_file} to {dst_dir / src_file.name}")
            os.makedirs(srcdir, exist_ok=True)
            shutil.copy(src_file, dst_dir / src_file.name)
    # libs = ["src", "ci"]
    if use_pio_run:
        # we have to copy a few folders of pio ci in order to get this to work.
        for lib in libs:
            project_libdir = Path(lib)
            assert project_libdir.exists()
            build_lib = builddir / "lib" / lib
            shutil.rmtree(build_lib, ignore_errors=True)
            shutil.copytree(project_libdir, build_lib)

        cwd = str(builddir)
        cmd_list = [
            "pio",
            "run",
        ]
        # in this case we need to manually copy the example to the src directory
        # because platformio doesn't support building a single file.
        # ino_file = example / f"{example.name}.ino"
    else:
        cmd_list = [
            "pio",
            "ci",
            "--board",
            real_board_name,
            *[f"--lib={lib}" for lib in libs],
            "--keep-build-dir",
            f"--build-dir={builddir.as_posix()}",
        ]
        cmd_list.append(f"{example.as_posix()}/*ino")
    cmd_str = subprocess.list2cmdline(cmd_list)
    msg_lsit = [
        "\n\n******************************",
        f"* Running command in cwd: {cwd if cwd else os.getcwd()}",
        f"*     {cmd_str}",
        "******************************\n",
    ]
    msg = "\n".join(msg_lsit)
    locked_print(msg)

    # Start timing for the process
    start_time = time.time()

    # Run the process with real-time output capture and timing
    result = subprocess.Popen(
        cmd_list,
        cwd=cwd,
        shell=shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    # Capture output lines in real-time with timing
    stdout_lines: List[str] = []
    if result.stdout:
        for line in iter(result.stdout.readline, ""):
            if line:
                elapsed = time.time() - start_time
                # Format timing as seconds with 2 decimal places
                timing_prefix = f"{elapsed:5.2f} "
                timed_line = timing_prefix + line.rstrip()
                stdout_lines.append(
                    line.rstrip()
                )  # Store original line for return value
                locked_print(timed_line)

    # Wait for process to complete
    result.wait()

    # Join all stdout lines for the return value
    stdout = "\n".join(stdout_lines)

    # replace all instances of "lib/src" => "src" so intellisense can find the files
    # with one click.
    stdout = stdout.replace("lib/src", "src").replace("lib\\src", "src")
    if result.returncode != 0:
        if not verbose_on_failure:
            ERROR_HAPPENED = True
            return False, stdout
        if ERROR_HAPPENED:
            return False, ""
        ERROR_HAPPENED = True
        locked_print(
            f"*** Error compiling example {example} for board {board_name} ***"
        )
        # re-running command with verbose output to see what the defines are.
        cmd_list.append("-v")
        cmd_str = subprocess.list2cmdline(cmd_list)
        msg_lsit = [
            "\n\n******************************",
            "* Re-running failed command but with verbose output:",
            f"*     {cmd_str}",
            "******************************\n",
        ]
        msg = "\n".join(msg_lsit)
        locked_print(msg)

        # Start timing for the verbose re-run
        start_time_verbose = time.time()

        # Run the verbose process with real-time output capture and timing
        result = subprocess.Popen(
            cmd_list,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )

        # Capture output lines in real-time with timing for verbose run
        stdout_lines_verbose: List[str] = []
        if result.stdout:
            for line in iter(result.stdout.readline, ""):
                if line:
                    elapsed = time.time() - start_time_verbose
                    # Format timing as seconds with 2 decimal places
                    timing_prefix = f"{elapsed:5.2f} "
                    timed_line = timing_prefix + line.rstrip()
                    stdout_lines_verbose.append(
                        line.rstrip()
                    )  # Store original line for return value
                    locked_print(timed_line)

        # Wait for verbose process to complete
        result.wait()

        # Join all stdout lines for the return value
        stdout = "\n".join(stdout_lines_verbose)
        stdout = (
            stdout
            + "\n\nThis is a second attempt, but with verbose output, look above for compiler errors.\n"
        )
        return False, stdout
    locked_print(f"*** Finished building example {example} for board {board_name} ***")
    return True, stdout


# Function to process task queues for each board
def compile_examples(
    board: Board,
    examples: list[Path],
    build_dir: str | None,
    verbose_on_failure: bool,
    libs: list[str] | None,
) -> tuple[bool, str]:
    """Process the task queue for the given board."""
    global ERROR_HAPPENED  # pylint: disable=global-statement
    board_name = board.board_name
    is_first = True
    for example in examples:
        example = example.relative_to(Path(".").resolve())
        if ERROR_HAPPENED:
            return True, ""
        locked_print(f"\n*** Building {example} for board {board_name} ***")
        if is_first:
            locked_print(
                f"*** Building for first example {example} board {board_name} ***"
            )
        if is_first and USE_FIRST_BUILD_LOCK:
            with FIRST_BUILD_LOCK:
                # Github runners are memory limited and the first job is the most
                # memory intensive since all the artifacts are being generated in parallel.
                success, message = compile_for_board_and_example(
                    board=board,
                    example=example,
                    build_dir=build_dir,
                    verbose_on_failure=verbose_on_failure,
                    libs=libs,
                )
        else:
            success, message = compile_for_board_and_example(
                board=board,
                example=example,
                build_dir=build_dir,
                verbose_on_failure=verbose_on_failure,
                libs=libs,
            )
        is_first = False
        if not success:
            ERROR_HAPPENED = True
            return (
                False,
                f"Error building {example} for board {board_name}. stdout:\n{message}",
            )
    return True, ""
