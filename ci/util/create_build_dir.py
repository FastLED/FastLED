# pyright: reportUnknownMemberType=false
"""
Create build directory for project.
"""

import json
import os
import shutil
import subprocess
import time
import warnings
from pathlib import Path
from typing import Any, Callable, Dict

from ci.boards import Board  # type: ignore
from ci.util.locked_print import locked_print


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


def insert_tool_aliases(meta_json: Dict[str, Dict[str, Any]]) -> None:
    for board in meta_json.keys():
        aliases: dict[str, str | None] = {}
        cc_path_value = meta_json[board].get("cc_path")
        resolved_cc_path: Path | None = None
        if cc_path_value:
            try:
                candidate = Path(str(cc_path_value))
                if candidate.is_absolute() and candidate.exists():
                    resolved_cc_path = candidate
                elif candidate.exists():
                    resolved_cc_path = candidate.resolve()
                else:
                    which_result = shutil.which(
                        candidate.name if candidate.name else str(candidate)
                    )
                    if which_result:
                        resolved_cc_path = Path(which_result)
            except Exception:
                resolved_cc_path = None

        # Try to infer toolchain bin directory and prefix from either CC or GDB path
        tool_bin_dir: Path | None = None
        tool_prefix: str = ""
        tool_suffix: str = ""

        if resolved_cc_path and resolved_cc_path.exists():
            cc_base = resolved_cc_path.name
            # If cc_path points at a real gcc binary, derive prefix/suffix from it.
            # If it's a wrapper (e.g. cached_CC.cmd) without "gcc" in the name,
            # fall back to using gdb_path to derive the actual toolchain prefix/suffix.
            if "gcc" in cc_base:
                tool_bin_dir = resolved_cc_path.parent
                tool_prefix = cc_base.split("gcc")[0]
                tool_suffix = resolved_cc_path.suffix
            else:
                resolved_cc_path = None  # Force gdb-based fallback below
        if resolved_cc_path is None:
            gdb_path_value = meta_json[board].get("gdb_path")
            if gdb_path_value:
                try:
                    gdb_path = Path(str(gdb_path_value))
                    if not gdb_path.exists():
                        which_gdb = shutil.which(gdb_path.name)
                        if which_gdb:
                            gdb_path = Path(which_gdb)
                    if gdb_path.exists():
                        tool_bin_dir = gdb_path.parent
                        gdb_base = gdb_path.name
                        # Derive prefix like 'arm-none-eabi-' from 'arm-none-eabi-gdb'
                        tool_prefix = (
                            gdb_base.split("gdb")[0] if "gdb" in gdb_base else ""
                        )
                        tool_suffix = gdb_path.suffix
                except Exception:
                    pass

        tools = [
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
        ]

        if tool_bin_dir is not None:
            for tool in tools:
                name = f"{tool_prefix}{tool}" + tool_suffix
                tool_path = tool_bin_dir / name
                if tool_path.exists():
                    aliases[tool] = str(tool_path)
                else:
                    which_result = shutil.which(name)
                    aliases[tool] = str(Path(which_result)) if which_result else None
        else:
            # Fallback: resolve via PATH only
            for tool in tools:
                which_result = shutil.which(tool)
                aliases[tool] = str(Path(which_result)) if which_result else None

        meta_json[board]["aliases"] = aliases


def remove_readonly(func: Callable[..., Any], path: str, _: Any) -> None:
    "Clear the readonly bit and reattempt the removal"
    if os.name == "nt":
        os.system(f"attrib -r {path}")
    else:
        try:
            os.chmod(path, 0o777)
        except Exception:
            print(f"Error removing readonly attribute from {path}")

    func(path)


def robust_rmtree(path: Path, max_retries: int, delay: float) -> bool:
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


def safe_file_removal(file_path: Path, max_retries: int) -> bool:
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
        # STRICT: Explicit retry parameters - NO defaults allowed
        if not robust_rmtree(srcdir, max_retries=5, delay=0.1):
            locked_print(
                f"[Thread {thread_id}] Warning: Failed to remove lib directory {srcdir}, continuing anyway"
            )

    platformio_ini = builddir / "platformio.ini"
    if platformio_ini.exists():
        locked_print(
            f"[Thread {thread_id}] Removing existing platformio.ini: {platformio_ini}"
        )
        # STRICT: Explicit retry parameter - NO defaults allowed
        if not safe_file_removal(platformio_ini, max_retries=3):
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
            # STRICT: Explicit retry parameters - NO defaults allowed
            if not robust_rmtree(dst_dir, max_retries=5, delay=0.1):
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
        cmd_list.append(f"--project-option=lib_deps={','.join(extra_packages)}")
    if no_install_deps:
        cmd_list.append("--no-install-dependencies")

    # Add CCACHE configuration script
    ccache_script = builddir / "ccache_config.py"
    if not ccache_script.exists():
        locked_print(
            f"[Thread {thread_id}] Creating CCACHE configuration script at {ccache_script}"
        )
        with open(ccache_script, "w") as f:
            f.write(
                '''"""Configure CCACHE for PlatformIO builds."""

import os
import platform
import subprocess
from pathlib import Path

Import("env")

def is_ccache_available():
    """Check if ccache is available in the system."""
    try:
        subprocess.run(["ccache", "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def get_ccache_path():
    """Get the full path to ccache executable."""
    if platform.system() == "Windows":
        # On Windows, look in chocolatey's bin directory
        ccache_paths = [
            "C:\\ProgramData\\chocolatey\\bin\\ccache.exe",
            os.path.expanduser("~\\scoop\\shims\\ccache.exe")
        ]
        for path in ccache_paths:
            if os.path.exists(path):
                return path
    else:
        # On Unix-like systems, use which to find ccache
        try:
            return subprocess.check_output(["which", "ccache"]).decode().strip()
        except subprocess.CalledProcessError:
            pass
    return None

def configure_ccache(env):
    """Configure CCACHE for the build environment."""
    if not is_ccache_available():
        print("CCACHE is not available. Skipping CCACHE configuration.")
        return

    ccache_path = get_ccache_path()
    if not ccache_path:
        print("Could not find CCACHE executable. Skipping CCACHE configuration.")
        return

    print(f"Found CCACHE at: {ccache_path}")

    # Set up CCACHE environment variables if not already set
    if "CCACHE_DIR" not in os.environ:
        # STRICT: PROJECT_DIR must be explicitly set - NO fallbacks allowed
        project_dir_for_ccache = env.get("PROJECT_DIR")
        if not project_dir_for_ccache:
            raise RuntimeError(
                "CRITICAL: PROJECT_DIR environment variable is required for CCACHE_DIR setup. "
                "Please set PROJECT_DIR to the project root directory."
            )
        ccache_dir = os.path.join(project_dir_for_ccache, ".ccache")
        os.environ["CCACHE_DIR"] = ccache_dir
        Path(ccache_dir).mkdir(parents=True, exist_ok=True)

    # Configure CCACHE for this build
    # STRICT: PROJECT_DIR must be explicitly set - NO fallbacks allowed
    project_dir = env.get("PROJECT_DIR")
    if not project_dir:
        raise RuntimeError(
            "CRITICAL: PROJECT_DIR environment variable is required but not set. "
            "Please set PROJECT_DIR to the project root directory."
        )
    os.environ["CCACHE_BASEDIR"] = project_dir
    os.environ["CCACHE_COMPRESS"] = "true"
    os.environ["CCACHE_COMPRESSLEVEL"] = "6"
    os.environ["CCACHE_MAXSIZE"] = "400M"

    # Wrap compiler commands with ccache
    # STRICT: CC and CXX must be explicitly set - NO fallbacks allowed
    original_cc = env.get("CC")
    if not original_cc:
        raise RuntimeError(
            "CRITICAL: CC environment variable is required but not set. "
            "Please set CC to the C compiler path (e.g., gcc, clang)."
        )
    original_cxx = env.get("CXX")
    if not original_cxx:
        raise RuntimeError(
            "CRITICAL: CXX environment variable is required but not set. "
            "Please set CXX to the C++ compiler path (e.g., g++, clang++)."
        )

    # Don't wrap if already wrapped
    if "ccache" not in original_cc:
        env.Replace(
            CC=f"{ccache_path} {original_cc}",
            CXX=f"{ccache_path} {original_cxx}",
        )
        print(f"Wrapped CC: {env.get('CC')}")
        print(f"Wrapped CXX: {env.get('CXX')}")

    # Show CCACHE stats
    subprocess.run([ccache_path, "--show-stats"], check=False)

configure_ccache(env)'''
            )

    # Get absolute paths for scripts
    project_root = Path.cwd()
    ci_flags_script = (project_root / "ci" / "ci-flags.py").resolve().as_posix()
    ccache_script = (builddir / "ccache_config.py").resolve().as_posix()

    # Create a list of scripts with pre: prefix
    script_list = [f"pre:{ci_flags_script}", f"pre:{ccache_script}"]

    # Add any additional scripts
    if extra_scripts:
        # Convert to absolute path and use Unix-style separators
        extra_scripts_path = str(Path(extra_scripts).resolve().as_posix())
        if not extra_scripts_path.startswith("pre:"):
            extra_scripts_path = f"pre:{extra_scripts_path}"
        script_list.append(extra_scripts_path)

    # Add the scripts as a list
    cmd_list.append(f"--project-option=extra_scripts=[{','.join(script_list)}]")

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

    # Print the location of the generated platformio.ini file
    platformio_ini_path = builddir / "platformio.ini"
    locked_print(f"Writing to platformio.ini {platformio_ini_path}")

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
