import shutil
import subprocess
from pathlib import Path

from ci.locked_print import locked_print
from ci.project import Project


def create_build_dir(
    project: Project,
    defines: list[str],
    no_install_deps: bool,
    extra_packages: list[str],
    build_dir: str | None,
) -> tuple[bool, str]:
    """Create the build directory for the given board."""
    board = project.board_name
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
    if project.platform:
        cmd_list.append(f"--project-option=platform={project.platform}")
    if project.platform_packages:
        cmd_list.append(
            f"--project-option=platform_packages={project.platform_packages}"
        )
    if project.framework:
        cmd_list.append(f"--project-option=framework={project.framework}")
    if project.board_build_core:
        cmd_list.append(f"--project-option=board_build.core={project.board_build_core}")
    if project.board_build_filesystem_size:
        cmd_list.append(
            f"--project-option=board_build.filesystem_size={project.board_build_filesystem_size}"
        )
    if defines:
        build_flags = " ".join(f"-D {define}" for define in defines)
        cmd_list.append(f"--project-option=build_flags={build_flags}")
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
