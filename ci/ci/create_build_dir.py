import json
import os
import shutil
import subprocess
import time
import warnings
from pathlib import Path

from ci.boards import Board  # type: ignore
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


def insert_tool_aliases(meta_json: dict[str, dict]) -> None:
    for board in meta_json.keys():
        aliases: dict[str, str | None] = {}
        cc_path = meta_json[board].get("cc_path")
        cc_path = Path(cc_path) if cc_path else None
        if cc_path:
            # get the prefix of the base name of the compiler.
            cc_base = cc_path.name
            parent = cc_path.parent
            prefix = cc_base.split("gcc")[0]
            suffix = cc_path.suffix
            # create the aliases
            for tool in [
                "gcc",
                "g++",
                "ar",
                "objcopy",
                "objdump",
                "size",
                "nm",
                "ld",
                "as",
                "ranlib",
                "strip",
                "c++filt",
                "readelf",
                "addr2line",
            ]:
                name = f"{prefix}{tool}" + suffix
                tool_path = Path(str(parent / name))
                if tool_path.exists():
                    aliases[tool] = str(tool_path)
                else:
                    aliases[tool] = None
        meta_json[board]["aliases"] = aliases


def remove_readonly(func, path, _):
    "Clear the readonly bit and reattempt the removal"
    if os.name == "nt":
        os.system(f"attrib -r {path}")
    else:
        try:
            os.chmod(path, 0o777)
        except Exception:
            print(f"Error removing readonly attribute from {path}")

    func(path)


def robust_rmtree(path: Path, max_retries: int = 5, delay: float = 0.1) -> bool:
    """
    Robustly remove a directory tree, handling race conditions and concurrent access.

    Args:
        path: Path to the directory to remove
        max_retries: Maximum number of retry attempts
        delay: Delay between retries in seconds

    Returns:
        True if removal was successful, False otherwise
    """
    if not path.exists():
        locked_print(f"Directory {path} doesn't exist, skipping removal")
        return True

    for attempt in range(max_retries):
        try:
            locked_print(
                f"Attempting to remove directory {path} (attempt {attempt + 1}/{max_retries})"
            )
            shutil.rmtree(path, onerror=remove_readonly)
            locked_print(f"Successfully removed directory {path}")
            return True
        except OSError as e:
            if attempt == max_retries - 1:
                locked_print(
                    f"Failed to remove directory {path} after {max_retries} attempts: {e}"
                )
                return False

            # Log the specific error and retry
            locked_print(
                f"Failed to remove directory {path} on attempt {attempt + 1}: {e}"
            )

            # Check if another process removed it
            if not path.exists():
                locked_print(f"Directory {path} was removed by another process")
                return True

            # Wait before retrying
            time.sleep(delay * (2**attempt))  # Exponential backoff

        except Exception as e:
            locked_print(f"Unexpected error removing directory {path}: {e}")
            return False

    return False


def safe_file_removal(file_path: Path, max_retries: int = 3) -> bool:
    """
    Safely remove a file with retry logic.

    Args:
        file_path: Path to the file to remove
        max_retries: Maximum number of retry attempts

    Returns:
        True if removal was successful, False otherwise
    """
    if not file_path.exists():
        return True

    for attempt in range(max_retries):
        try:
            file_path.unlink()
            locked_print(f"Successfully removed file {file_path}")
            return True
        except OSError as e:
            if attempt == max_retries - 1:
                locked_print(
                    f"Failed to remove file {file_path} after {max_retries} attempts: {e}"
                )
                return False

            locked_print(
                f"Failed to remove file {file_path} on attempt {attempt + 1}: {e}"
            )

            # Check if another process removed it
            if not file_path.exists():
                locked_print(f"File {file_path} was removed by another process")
                return True

            time.sleep(0.1 * (attempt + 1))

        except Exception as e:
            locked_print(f"Unexpected error removing file {file_path}: {e}")
            return False

    return False


def create_build_dir(
    board: Board,
    defines: list[str],
    customsdk: str | None,
    no_install_deps: bool,
    extra_packages: list[str],
    build_dir: str | None,
    board_dir: str | None,
    build_flags: list[str] | None,
    extra_scripts: str | None,
) -> tuple[bool, str]:
    """Create the build directory for the given board."""
    import threading

    # filter out "web" board because it's not a real board.
    if board.board_name == "web":
        locked_print(f"Skipping web target for board {board.board_name}")
        return True, ""
    if board.defines:
        defines.extend(board.defines)
        # remove duplicates
        defines = list(set(defines))
    board_name = board.board_name
    real_board_name = board.get_real_board_name()
    thread_id = threading.current_thread().ident
    locked_print(
        f"*** [Thread {thread_id}] Initializing environment for {board_name} ***"
    )
    # builddir = Path(build_dir) / board if build_dir else Path(".build") / board
    build_dir = build_dir or ".build"
    builddir = Path(build_dir) / board_name

    locked_print(f"[Thread {thread_id}] Creating build directory: {builddir}")
    try:
        builddir.mkdir(parents=True, exist_ok=True)
        locked_print(
            f"[Thread {thread_id}] Successfully created build directory: {builddir}"
        )
    except Exception as e:
        locked_print(
            f"[Thread {thread_id}] Error creating build directory {builddir}: {e}"
        )
        return False, f"Failed to create build directory: {e}"
    # if lib directory (where FastLED lives) exists, remove it. This is necessary to run on
    # recycled build directories for fastled to update. This is a fast operation.
    srcdir = builddir / "lib"
    if srcdir.exists():
        locked_print(f"[Thread {thread_id}] Removing existing lib directory: {srcdir}")
        if not robust_rmtree(srcdir):
            locked_print(
                f"[Thread {thread_id}] Warning: Failed to remove lib directory {srcdir}, continuing anyway"
            )

    platformio_ini = builddir / "platformio.ini"
    if platformio_ini.exists():
        locked_print(
            f"[Thread {thread_id}] Removing existing platformio.ini: {platformio_ini}"
        )
        if not safe_file_removal(platformio_ini):
            locked_print(
                f"[Thread {thread_id}] Warning: Failed to remove {platformio_ini}, continuing anyway"
            )

    if board_dir:
        dst_dir = builddir / "boards"
        locked_print(
            f"[Thread {thread_id}] Processing board directory: {board_dir} -> {dst_dir}"
        )

        if dst_dir.exists():
            locked_print(
                f"[Thread {thread_id}] Removing existing boards directory: {dst_dir}"
            )
            if not robust_rmtree(dst_dir):
                locked_print(
                    f"[Thread {thread_id}] Error: Failed to remove boards directory {dst_dir}"
                )
                return False, f"Failed to remove existing boards directory {dst_dir}"

        try:
            locked_print(
                f"[Thread {thread_id}] Copying board directory: {board_dir} -> {dst_dir}"
            )
            shutil.copytree(str(board_dir), str(dst_dir))
            locked_print(
                f"[Thread {thread_id}] Successfully copied board directory to {dst_dir}"
            )
        except Exception as e:
            locked_print(f"[Thread {thread_id}] Error copying board directory: {e}")
            return False, f"Failed to copy board directory: {e}"
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
    if build_flags is not None:
        for build_flag in build_flags:
            cmd_list.append(f"--project-option=build_flags={build_flag}")
    if defines:
        build_flags_str = " ".join(f"-D{define}" for define in defines)
        cmd_list.append(f"--project-option=build_flags={build_flags_str}")
    if board.customsdk:
        cmd_list.append(f"--project-option=custom_sdkconfig={customsdk}")
    if extra_packages:
        cmd_list.append(f'--project-option=lib_deps={",".join(extra_packages)}')
    if no_install_deps:
        cmd_list.append("--no-install-dependencies")
    if extra_scripts:
        p = Path(extra_scripts)
        cmd_list.append(f"--project-option=extra_scripts={p.resolve()}")
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
        locked_print(
            f"*** [Thread {thread_id}] Error setting up board {board_name} ***"
        )
        return False, stdout
    locked_print(
        f"*** [Thread {thread_id}] Finished initializing environment for board {board_name} ***"
    )

    # Print the contents of the generated platformio.ini file for debugging
    platformio_ini_path = builddir / "platformio.ini"
    if platformio_ini_path.exists():
        locked_print(
            f"\n*** Contents of {platformio_ini_path} after initialization ***"
        )
        try:
            with open(platformio_ini_path, "r") as f:
                ini_contents = f.read()
                locked_print(f"\n\n{ini_contents}\n\n")
        except Exception as e:
            locked_print(f"Error reading {platformio_ini_path}: {e}")
        locked_print(f"*** End of {platformio_ini_path} contents ***\n")
    else:
        locked_print(
            f"Warning: {platformio_ini_path} was not found after initialization"
        )

    # dumping enviorment variables to help debug.
    # this is the command: pio run --target envdump
    cwd = str(builddir.resolve())
    cmd_list = [
        "pio",
        "project",
        "metadata",
        "--json-output",
    ]
    cmd_str = subprocess.list2cmdline(cmd_list)
    stdout = subprocess.run(
        cmd_list,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    ).stdout

    try:
        data = json.loads(stdout)
        # now dump the values to the file at the root of the build directory.
        matadata_json = builddir / "build_info.json"
        try:
            insert_tool_aliases(data)
            formatted = json.dumps(data, indent=4, sort_keys=True)
            with open(matadata_json, "w") as f:
                f.write(formatted)
        except Exception:
            with open(matadata_json, "w") as f:
                f.write(stdout)
    except json.JSONDecodeError:
        msg = f"build_info.json will not be generated because of error because stdout does not look like a json file:\n#### STDOUT ####\n{stdout}\n#### END STDOUT ####\n"
        locked_print(msg)
    return True, stdout
