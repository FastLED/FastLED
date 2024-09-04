from pathlib import Path
import shutil
import subprocess
from ci.locked_print import locked_print



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