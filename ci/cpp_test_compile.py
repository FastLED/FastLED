import os
import shutil
import subprocess
import sys
from atexit import register
from pathlib import Path

from ci.paths import PROJECT_ROOT

BUILD_DIR = PROJECT_ROOT / "tests" / ".build"
BUILD_DIR.mkdir(parents=True, exist_ok=True)

HERE = Path(__file__).resolve().parent

WEIRD_FILE = HERE / "-"  # Sometimes appears on windows.


def delete_weird_file():
    if WEIRD_FILE.exists():
        WEIRD_FILE.unlink()


register(delete_weird_file)


def write_compiler_stubs():
    ZIG = shutil.which("zig")
    assert ZIG is not None, "Zig compiler not found in PATH."
    CC_PATH = BUILD_DIR / "cc"
    CXX_PATH = BUILD_DIR / "c++"
    if sys.platform == "win32":
        CC_PATH = CC_PATH.with_suffix(".bat")
        CXX_PATH = CXX_PATH.with_suffix(".bat")
        CC_PATH.write_text(f'@echo off\n"{ZIG}" cc %*\n')
        CXX_PATH.write_text(f'@echo off\n"{ZIG}" c++ %*\n')
    else:
        CC_PATH.write_text(f'#!/bin/bash\n"{ZIG}" cc "$@"\n')
        CXX_PATH.write_text(f'#!/bin/bash\n"{ZIG}" c++ "$@"\n')
        CC_PATH.chmod(0o755)
        CXX_PATH.chmod(0o755)
    return CC_PATH, CXX_PATH


def run_command(command: str, cwd=None):
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=True,
        text=True,
        cwd=cwd,
    )
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        print(f"Error executing command: {command}")
        print("STDOUT:")
        print(stdout)
        print("STDERR:")
        print(stderr)
        print(f"Return code: {process.returncode}")
        exit(1)
    return stdout, stderr


def compile_fastled_library():
    zig_prog = shutil.which("zig")
    assert zig_prog is not None, "Zig compiler not found in PATH."

    cc, cxx = write_compiler_stubs()

    # use the system path, so on windows this looks like "C:\Program Files\Zig\zig.exe"
    cc_path = cc.resolve()
    cxx_path = cxx.resolve()
    if sys.platform == "win32":
        cc_path = str(cc_path).replace("/", "\\")
        cxx_path = str(cxx_path).replace("/", "\\")

    # print out the paths
    print(f"CC: {cc_path}")
    print(f"CXX: {cxx_path}")
    # sys.exit(1)

    # Set environment variables for C and C++ compilers
    os.environ["CC"] = str(cc_path)
    os.environ["CXX"] = str(cxx_path)

    # Configure CMake
    cmake_configure_command = (
        f'cmake -S {PROJECT_ROOT / "tests"} -B {BUILD_DIR} -G "Ninja" '
        f"-DCMAKE_VERBOSE_MAKEFILE=ON"
    )
    stdout, stderr = run_command(cmake_configure_command)
    print(stdout)
    print(stderr)

    # Build the project
    cmake_build_command = f"cmake --build {BUILD_DIR}"
    stdout, stderr = run_command(cmake_build_command)
    print(stdout)
    print(stderr)

    print("FastLED library compiled successfully.")


def main():
    os.chdir(str(HERE))
    print(f"Current directory: {Path('.').absolute()}")
    compile_fastled_library()


if __name__ == "__main__":
    main()
