import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Tuple

from ci.paths import PROJECT_ROOT

BUILD_DIR = PROJECT_ROOT / "tests" / ".build"
BUILD_DIR.mkdir(parents=True, exist_ok=True)

"""
 clang \
   --target=wasm32 \
   -O3 \
   -flto \
   -nostdlib \
   -Wl,--no-entry \
   -Wl,--export-all \
   -Wl,--lto-O3 \
+  -Wl,-z,stack-size=$[8 * 1024 * 1024] \ # Set maximum stack size to 8MiB
   -o add.wasm \
   add.c
"""


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

    if WASM_BUILD:
        wasm_flags = [
            "--target=wasm32",
            "-O3",
            "-flto",
            # "-nostdlib",
            # "-Wl,--no-entry",
            # "-Wl,--export-all",
            # "-Wl,--lto-O3",
            # "-Wl,-z,stack-size=8388608",  # 8 * 1024 * 1024 (8MiB)
        ]
        os.environ["CFLAGS"] = " ".join(wasm_flags)
        os.environ["CXXFLAGS"] = " ".join(wasm_flags)

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
        CC_PATH = CC_PATH.with_suffix(".cmd")
        CXX_PATH = CXX_PATH.with_suffix(".cmd")
        AR_PATH = AR_PATH.with_suffix(".cmd")
        CC_PATH.write_text(f'@echo off\n"{ZIG}" cc %* 2>&1\n')
        CXX_PATH.write_text(f'@echo off\n"{ZIG}" c++ %* 2>&1\n')
        AR_PATH.write_text(f'@echo off\n"{ZIG}" ar %* 2>&1\n')
    else:
        CC_PATH.write_text(f'#!/bin/bash\n"{ZIG}" cc "$@"\n')
        CXX_PATH.write_text(f'#!/bin/bash\n"{ZIG}" c++ "$@"\n')
        AR_PATH.write_text(f'#!/bin/bash\n"{ZIG}" ar "$@"\n')
        CC_PATH.chmod(0o755)
        CXX_PATH.chmod(0o755)

    if WASM_BUILD:
        wasm_flags = [
            # "--target=wasm32",
            # "-O3",
            # "-flto",
            # "-nostdlib",
            "-Wl,--no-entry",
            # "-Wl,--export-all",
            # "-Wl,--lto-O3",
            "-Wl,-z,stack-size=8388608",  # 8 * 1024 * 1024 (8MiB)
        ]
    os.environ["CFLAGS"] = " ".join(wasm_flags)
    os.environ["CXXFLAGS"] = " ".join(wasm_flags)

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


def run_command(command: str, cwd=None, capture: bool = False) -> tuple[str, str]:
    process = subprocess.run(
        command,
        shell=True,
        cwd=cwd,
        capture_output=capture,
    )

    if capture:
        stdout = process.stdout.decode("utf-8")
        stderr = process.stderr.decode("utf-8")
    else:
        stdout = stderr = ""

    if process.returncode != 0:
        print(f"Error executing command: {command}")
        if capture:
            print("STDOUT:")
            print(stdout)
            print("STDERR:")
            print(stderr)
        print(f"Return code: {process.returncode}")
        time.sleep(1)
        exit(1)

    return stdout, stderr


def compile_fastled_library() -> None:
    if USE_ZIG:
        print("USING ZIG COMPILER")
        zig_prog = shutil.which("zig")
        assert zig_prog is not None, "Zig compiler not found in PATH."
        use_zig_compiler()
    elif USE_CLANG:
        print("USING CLANG COMPILER")
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
                "-DCMAKE_C_COMPILER_TARGET=wasm32-wasi",
                "-DCMAKE_CXX_COMPILER_TARGET=wasm32-wasi",
                "-DCMAKE_C_COMPILER_WORKS=TRUE",
                "-DCMAKE_CXX_COMPILER_WORKS=TRUE",
                "-DCMAKE_SYSTEM_NAME=Generic",
                "-DCMAKE_CROSSCOMPILING=TRUE",
                "-DCMAKE_EXE_LINKER_FLAGS=-Wl,--no-entry -Wl,--export-all -Wl,--lto-O3 -Wl,-z,stack-size=8388608",
            ]
        )
    cmake_configure_command = subprocess.list2cmdline(cmake_configure_command_list)
    run_command(cmake_configure_command, cwd=BUILD_DIR)

    # Build the project
    cmake_build_command = f"cmake --build {BUILD_DIR}"
    run_command(cmake_build_command)

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


def get_build_info(args: argparse.Namespace) -> dict[str, str | dict[str, str]]:
    return {
        "USE_ZIG": str(USE_ZIG),
        "USE_CLANG": str(USE_CLANG),
        "WASM_BUILD": str(WASM_BUILD),
        "CC": os.environ.get("CC", ""),
        "CXX": os.environ.get("CXX", ""),
        "AR": os.environ.get("AR", ""),
        "CFLAGS": os.environ.get("CFLAGS", ""),
        "CXXFLAGS": os.environ.get("CXXFLAGS", ""),
        "ARGS": {
            "use_zig": str(args.use_zig),
            "use_clang": str(args.use_clang),
            "wasm": str(args.wasm),
        },
    }


def should_clean_build(build_info: dict[str, str | dict[str, str]]) -> bool:
    build_info_file = BUILD_DIR / "build_info.json"
    if not build_info_file.exists():
        return True

    with open(build_info_file, "r") as f:
        old_build_info = json.load(f)

    return old_build_info != build_info


def update_build_info(build_info: dict[str, str | dict[str, str]]):
    build_info_file = BUILD_DIR / "build_info.json"
    with open(build_info_file, "w") as f:
        json.dump(build_info, f, indent=2)


def main() -> None:
    global USE_ZIG, USE_CLANG, WASM_BUILD

    args = parse_arguments()
    USE_ZIG = args.use_zig
    USE_CLANG = args.use_clang or args.wasm  # Use Clang for WASM builds
    WASM_BUILD = args.wasm

    os.chdir(str(HERE))
    print(f"Current directory: {Path('.').absolute()}")

    build_info = get_build_info(args)
    if args.clean or should_clean_build(build_info):
        clean_build_directory()

    compile_fastled_library()
    update_build_info(build_info)
    print("FastLED library compiled successfully.")


if __name__ == "__main__":
    main()
