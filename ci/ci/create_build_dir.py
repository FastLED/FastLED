import shutil
import subprocess
import warnings
from pathlib import Path

from ci.boards import Board
from ci.locked_print import locked_print


def _install_global_package(package: str) -> None:
    # example pio pkg -g -p "https://github.com/maxgerhardt/platform-raspberrypi.git".
    locked_print(f"*** Installing {package} ***")
    cmd_list = [
        "pio",
        "pkg",
        "install",
        "-g",
        "-p",
        package,
    ]
    cmd_str = subprocess.list2cmdline(cmd_list)
    locked_print(f"Running command:\n\n{cmd_str}\n\n")
    result = subprocess.run(
        cmd_str,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=True,
    )
    locked_print(result.stdout)
    locked_print(f"*** Finished installing {package} ***")


def create_build_dir(
    board: Board,
    defines: list[str],
    no_install_deps: bool,
    extra_packages: list[str],
    build_dir: str | None,
    board_dir: str | None,
) -> tuple[bool, str]:
    """Create the build directory for the given board."""
    if board.defines:
        defines.extend(board.defines)
        # remove duplicates
        defines = list(set(defines))
    board_name = board.board_name
    real_board_name = board.get_real_board_name()
    locked_print(f"*** Initializing environment for {board_name} ***")
    # builddir = Path(build_dir) / board if build_dir else Path(".build") / board
    build_dir = build_dir or ".build"
    builddir = Path(build_dir) / board_name
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
    if board_dir:
        dst_dir = builddir / "boards"
        if dst_dir.exists():
            shutil.rmtree(dst_dir)
        shutil.copytree(str(board_dir), str(builddir / "boards"))
    if board.platform_needs_install:
        if board.platform:
            try:
                _install_global_package(board.platform)
            except subprocess.CalledProcessError as e:
                stdout = e.stdout
                return False, stdout
        else:
            warnings.warn("Platform install was specified but no platform was given.")

    cmd_list = [
        "pio",
        "project",
        "init",
        "--project-dir",
        builddir.as_posix(),
        "--board",
        real_board_name,
    ]
    if board.platform:
        cmd_list.append(f"--project-option=platform={board.platform}")
    if board.platform_packages:
        cmd_list.append(f"--project-option=platform_packages={board.platform_packages}")
    if board.framework:
        cmd_list.append(f"--project-option=framework={board.framework}")
    if board.board_build_core:
        cmd_list.append(f"--project-option=board_build.core={board.board_build_core}")
    if board.board_build_filesystem_size:
        cmd_list.append(
            f"--project-option=board_build.filesystem_size={board.board_build_filesystem_size}"
        )
    if defines:
        build_flags = " ".join(f"-D{define}" for define in defines)
        cmd_list.append(f"--project-option=build_flags={build_flags}")
    if extra_packages:
        cmd_list.append(f'--project-option=lib_deps={",".join(extra_packages)}')
    if no_install_deps:
        cmd_list.append("--no-install-dependencies")
    cmd_str = subprocess.list2cmdline(cmd_list)
    locked_print(f"\n\nRunning command:\n  {cmd_str}\n")
    result = subprocess.run(
        cmd_str,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    stdout = result.stdout
    locked_print(result.stdout)
    if result.returncode != 0:
        locked_print(f"*** Error setting up board {board_name} ***")
        return False, stdout
    locked_print(f"*** Finished initializing environment for board {board_name} ***")
    return True, stdout
