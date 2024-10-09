import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Tuple

from ci.paths import PROJECT_ROOT

BUILD_DIR = PROJECT_ROOT / "tests" / ".build"
BUILD_DIR.mkdir(parents=True, exist_ok=True)



def clean_build_directory():
    print("Cleaning build directory...")
    shutil.rmtree(BUILD_DIR, ignore_errors=True)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    print("Build directory cleaned.")


HERE = Path(__file__).resolve().parent

WASM_BUILD = False
USE_ZIG = False
USE_CLANG = False


def use_clang_compiler() -> Tuple[Path, Path, Path]:
    CLANG = shutil.which("clang")
    CLANGPP = shutil.which("clang++")
    LLVM_AR = shutil.which("llvm-ar")
    assert CLANG is not None, "Clang compiler not found in PATH."
    assert CLANGPP is not None, "Clang++ compiler not found in PATH."
    assert LLVM_AR is not None, "llvm-ar not found in PATH."

    # Set environment variables for C and C++ compilers
    os.environ["CC"] = CLANG
    os.environ["CXX"] = CLANGPP
    os.environ["AR"] = LLVM_AR

    print(f"CC: {CLANG}")
    print(f"CXX: {CLANGPP}")
    print(f"AR: {LLVM_AR}")

    return Path(CLANG), Path(CLANGPP), Path(LLVM_AR)


def use_zig_compiler() -> Tuple[Path, Path, Path]:
    ZIG = shutil.which("zig")
    assert ZIG is not None, "Zig compiler not found in PATH."
    CC_PATH = BUILD_DIR / "cc"
    CXX_PATH = BUILD_DIR / "c++"
    AR_PATH = BUILD_DIR / "ar"
    if sys.platform == "win32":
        CC_PATH = CC_PATH.with_suffix(".bat")
        CXX_PATH = CXX_PATH.with_suffix(".bat")
        AR_PATH = AR_PATH.with_suffix(".bat")
        CC_PATH.write_text(f'@echo off\n"{ZIG}" cc %*\n')
        CXX_PATH.write_text(f'@echo off\n"{ZIG}" c++ %*\n')
        AR_PATH.write_text(f'@echo off\n"{ZIG}" ar %*\n')
    else:
        CC_PATH.write_text(f'#!/bin/bash\n"{ZIG}" cc "$@"\n')
        CXX_PATH.write_text(f'#!/bin/bash\n"{ZIG}" c++ "$@"\n')
        AR_PATH.write_text(f'#!/bin/bash\n"{ZIG}" ar "$@"\n')
        CC_PATH.chmod(0o755)
        CXX_PATH.chmod(0o755)

    cc, cxx = CC_PATH, CXX_PATH
    # use the system path, so on windows this looks like "C:\Program Files\Zig\zig.exe"
    cc_path: Path | str = cc.resolve()
    cxx_path: Path | str = cxx.resolve()
    if sys.platform == "win32":
        cc_path = str(cc_path).replace("/", "\\")
        cxx_path = str(cxx_path).replace("/", "\\")

    # print out the paths
    print(f"CC: {cc_path}")
    print(f"CXX: {cxx_path}")
    print(f"AR: {AR_PATH}")
    # sys.exit(1)

    # Set environment variables for C and C++ compilers
    os.environ["CC"] = str(cc_path)
    os.environ["CXX"] = str(cxx_path)
    os.environ["AR"] = str(AR_PATH)
    return CC_PATH, CXX_PATH, AR_PATH


def run_command(command: str, cwd=None) -> tuple[str, str]:
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


def compile_fastled_library() -> None:
    if USE_ZIG:
        zig_prog = shutil.which("zig")
        assert zig_prog is not None, "Zig compiler not found in PATH."
        use_zig_compiler()
    elif USE_CLANG:
        use_clang_compiler()

    cmake_configure_command_list: list[str] = [
        "cmake",
        "-S",
        str(PROJECT_ROOT / "tests"),
        "-B",
        str(BUILD_DIR),
        "-G",
        "Ninja",
        "-DCMAKE_VERBOSE_MAKEFILE=ON",
    ]

    if WASM_BUILD:
        cmake_configure_command_list.extend(
            [
                "-DCMAKE_C_COMPILER_TARGET=wasm32-freestanding",
                "-DCMAKE_CXX_COMPILER_TARGET=wasm32-freestanding",
                "-DCMAKE_C_COMPILER_WORKS=TRUE",
                "-DCMAKE_SYSTEM_NAME=Wasm",
            ]
        )
    cmake_configure_command = subprocess.list2cmdline(cmake_configure_command_list)
    stdout, stderr = run_command(cmake_configure_command, cwd=BUILD_DIR)
    print(stdout)
    print(stderr)

    # Build the project
    cmake_build_command = f"cmake --build {BUILD_DIR}"
    stdout, stderr = run_command(cmake_build_command)
    print(stdout)
    print(stderr)

    print("FastLED library compiled successfully.")


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Compile FastLED library with different compiler options."
    )
    parser.add_argument("--use-zig", action="store_true", help="Use Zig compiler")
    parser.add_argument("--use-clang", action="store_true", help="Use Clang compiler")
    parser.add_argument("--wasm", action="store_true", help="Build for WebAssembly")
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean the build directory before compiling",
    )
    return parser.parse_args()


def main() -> None:
    global USE_ZIG, USE_CLANG, WASM_BUILD

    args = parse_arguments()
    USE_ZIG = args.use_zig
    USE_CLANG = args.use_clang
    WASM_BUILD = args.wasm

    os.chdir(str(HERE))
    print(f"Current directory: {Path('.').absolute()}")

    if args.clean:
        clean_build_directory()

    compile_fastled_library()


if __name__ == "__main__":
    main()
