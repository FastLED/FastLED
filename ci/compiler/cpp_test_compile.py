#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportReturnType=false, reportMissingParameterType=false
import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Set, Tuple

from ci.util.paths import PROJECT_ROOT
from ci.util.running_process import RunningProcess


BUILD_DIR = PROJECT_ROOT / "tests" / ".build"
BUILD_DIR.mkdir(parents=True, exist_ok=True)
CACHE_DIR = PROJECT_ROOT / ".cache"
CACHE_DIR.mkdir(parents=True, exist_ok=True)
TEST_FILES_LIST = CACHE_DIR / "test_files_list.txt"


def get_test_files() -> Set[str]:
    """Get a set of all test files"""
    test_files = set()
    tests_dir = PROJECT_ROOT / "tests"
    if tests_dir.exists():
        for file_path in tests_dir.rglob("test_*.cpp"):
            # Store relative paths for consistency
            test_files.add(str(file_path.relative_to(PROJECT_ROOT)))
    return test_files


def check_test_files_changed() -> bool:
    """Check if test files have changed since last run"""
    try:
        current_files = get_test_files()

        if TEST_FILES_LIST.exists():
            # Read previous file list
            with open(TEST_FILES_LIST, "r") as f:
                previous_files = set(line.strip() for line in f if line.strip())

            # Compare file sets
            if current_files == previous_files:
                return False  # No changes
            else:
                print("Test files have changed, cleaning build directory...")
                return True  # Files changed
        else:
            # No previous file list, need to clean
            return True
    except Exception as e:
        print(f"Warning: Error checking test file changes: {e}")
        return True  # Default to cleaning on error


def save_test_files_list() -> None:
    """Save current test file list"""
    try:
        current_files = get_test_files()
        with open(TEST_FILES_LIST, "w") as f:
            for file_path in sorted(current_files):
                f.write(f"{file_path}\n")
    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"Warning: Failed to save test file list: {e}")


def clean_build_directory():
    print("Cleaning build directory...")
    shutil.rmtree(BUILD_DIR, ignore_errors=True)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    print("Build directory cleaned.")


HERE = Path(__file__).resolve().parent

WASM_BUILD = False
USE_ZIG = False
USE_CLANG = False


def _has_system_clang_compiler() -> bool:
    CLANG = shutil.which("clang")
    CLANGPP = shutil.which("clang++")
    LLVM_AR = shutil.which("llvm-ar")
    return CLANG is not None and CLANGPP is not None and LLVM_AR is not None


def use_clang_compiler() -> Tuple[Path, Path, Path]:
    assert _has_system_clang_compiler(), "Clang system compiler not found"
    CLANG = shutil.which("clang")
    CLANGPP = shutil.which("clang++")
    LLVM_AR = shutil.which("llvm-ar")
    assert CLANG is not None, "clang compiler not found"
    assert CLANGPP is not None, "clang++ compiler not found"
    assert LLVM_AR is not None, "llvm-ar not found"
    # Set environment variables for C and C++ compilers
    os.environ["CC"] = CLANG
    os.environ["CXX"] = CLANGPP
    os.environ["AR"] = LLVM_AR

    os.environ["CXXFLAGS"] = os.environ.get("CXXFLAGS", "") + " -ferror-limit=1"
    os.environ["CFLAGS"] = os.environ.get("CFLAGS", "") + " -ferror-limit=1"

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
    assert 0 == os.system("python -m ziglang version"), "Zig-clang compiler not found"
    python_path_str: str | None = shutil.which("python")
    assert python_path_str is not None, "python not found in PATH"
    python_path = Path(python_path_str).resolve()
    zig_command = f'"{python_path}" -m ziglang'
    # We are going to build up shell scripts that look like cc, c++, and ar. It will contain the actual build command.
    cc_path = BUILD_DIR / "cc"
    cxx_path = BUILD_DIR / "c++"
    ar_path = BUILD_DIR / "ar"
    if sys.platform == "win32":
        cc_path = cc_path.with_suffix(".cmd")
        cxx_path = cxx_path.with_suffix(".cmd")
        ar_path = ar_path.with_suffix(".cmd")
        cc_path.write_text(f"@echo off\n{zig_command} cc %* 2>&1\n")
        cxx_path.write_text(f"@echo off\n{zig_command} c++ %* 2>&1\n")
        ar_path.write_text(f"@echo off\n{zig_command} ar %* 2>&1\n")
    else:
        cc_cmd = f'#!/bin/bash\n{zig_command} cc "$@"\n'
        cxx_cmd = f'#!/bin/bash\n{zig_command} c++ "$@"\n'
        ar_cmd = f'#!/bin/bash\n{zig_command} ar "$@"\n'
        cc_path.write_text(cc_cmd)
        cxx_path.write_text(cxx_cmd)
        ar_path.write_text(ar_cmd)
        cc_path.chmod(0o755)
        cxx_path.chmod(0o755)
        ar_path.chmod(0o755)

    # if WASM_BUILD:
    #     wasm_flags = [
    #         # "--target=wasm32",
    #         # "-O3",
    #         # "-flto",
    #         # "-nostdlib",
    #         "-Wl,--no-entry",
    #         # "-Wl,--export-all",
    #         # "-Wl,--lto-O3",
    #         "-Wl,-z,stack-size=8388608",  # 8 * 1024 * 1024 (8MiB)
    #     ]
    # os.environ["CFLAGS"] = " ".join(wasm_flags)
    # os.environ["CXXFLAGS"] = " ".join(wasm_flags)

    cc, cxx = cc_path, cxx_path
    # use the system path, so on windows this looks like "C:\Program Files\Zig\zig.exe"
    cc_path: Path | str = cc.resolve()
    cxx_path: Path | str = cxx.resolve()
    if sys.platform == "win32":
        cc_path = str(cc_path).replace("/", "\\")
        cxx_path = str(cxx_path).replace("/", "\\")

    # print out the paths
    print(f"CC: {cc_path}")
    print(f"CXX: {cxx_path}")
    print(f"AR: {ar_path}")
    # sys.exit(1)

    # Set environment variables for C and C++ compilers
    os.environ["CC"] = str(cc_path)
    os.environ["CXX"] = str(cxx_path)
    os.environ["AR"] = str(ar_path)
    return cc_path, cxx_path, ar_path


def run_command(command: str, cwd: Path | None = None) -> None:
    process = RunningProcess(command, cwd=cwd)
    process.wait()
    if process.returncode != 0:
        print(f"{Path(__file__).name}: Error executing command: {command}")
        sys.exit(1)


def compile_fastled(
    specific_test: str | None = None, enable_static_analysis: bool = False
) -> None:
    if USE_ZIG:
        print("USING ZIG COMPILER")
        rtn = subprocess.run(
            "python -m ziglang version", shell=True, capture_output=True
        ).returncode
        zig_is_installed = rtn == 0
        assert zig_is_installed, (
            'Zig compiler not when using "python -m ziglang version" command'
        )
        use_zig_compiler()
    elif USE_CLANG:
        print("USING CLANG COMPILER")
        use_clang_compiler()
    else:
        # Using default GCC compiler - add verbose output similar to Clang
        print("USING GCC COMPILER (DEFAULT)")
        print(
            "⚠️  WARNING: GCC builds are ~5x slower than Clang due to poor unified compilation performance"
        )
        print("    Expected build time: 40+ seconds (vs 7 seconds for Clang)")
        print("    To use faster Clang: bash test --clang")
        print("    To force unified compilation: export FASTLED_ALL_SRC=1")
        # Display compiler paths similar to use_clang_compiler()
        gcc = shutil.which("gcc")
        gpp = shutil.which("g++")
        ar = shutil.which("ar")
        if gcc:
            print(f"CC: {gcc}")
            # Set environment variable for GCC similar to Clang
            os.environ["CC"] = gcc
        if gpp:
            print(f"CXX: {gpp}")
            # Set environment variable for GCC similar to Clang
            os.environ["CXX"] = gpp
        if ar:
            print(f"AR: {ar}")
            # Set environment variable for AR similar to Clang
            os.environ["AR"] = ar

            # Add GCC performance optimization flags to improve compilation speed
            # These flags significantly reduce compilation time for large unified builds
            gcc_perf_flags = (
                " -fmax-errors=10 -pipe -fno-var-tracking -fno-debug-types-section"
            )
            os.environ["CXXFLAGS"] = os.environ.get("CXXFLAGS", "") + gcc_perf_flags
            print(
                "Applied GCC performance optimizations: -pipe, -fno-var-tracking, -fno-debug-types-section"
            )

    # Apply CFLAGS for both GCC and other compilers
    os.environ["CFLAGS"] = os.environ.get("CFLAGS", "") + " -fmax-errors=10"

    cmake_configure_command_list: list[str] = [
        "cmake",
        "-S",
        str(PROJECT_ROOT / "tests"),
        "-B",
        str(BUILD_DIR),
        "-G",
        "Ninja",
        "-DCMAKE_VERBOSE_MAKEFILE=ON",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
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

    # Pass specific test name to CMake if specified
    if specific_test is not None:
        # Remove test_ prefix if present
        if specific_test.startswith("test_"):
            test_name = specific_test[5:]
        else:
            test_name = specific_test
        cmake_configure_command_list.append(f"-DSPECIFIC_TEST={test_name}")

    # Enable static analysis tools if requested
    if enable_static_analysis:
        cmake_configure_command_list.extend(
            ["-DFASTLED_ENABLE_IWYU=ON", "-DFASTLED_ENABLE_CLANG_TIDY=ON"]
        )
        print("🔍 Static analysis requested: IWYU and clang-tidy")
        print("   (CMake will check if tools are installed and warn if missing)")

    cmake_configure_command = subprocess.list2cmdline(cmake_configure_command_list)
    run_command(cmake_configure_command, cwd=BUILD_DIR)

    # Normalize test name for build command
    if specific_test is not None and specific_test.startswith("test_"):
        specific_test = specific_test[5:]

    # Build the project with explicit parallelization
    import multiprocessing

    cpu_count = multiprocessing.cpu_count()

    # Allow override via environment variable
    if os.environ.get("FASTLED_PARALLEL_JOBS"):
        parallel_jobs = int(os.environ["FASTLED_PARALLEL_JOBS"])
        print(f"Using custom parallel jobs: {parallel_jobs}")
    else:
        parallel_jobs = cpu_count * 2  # Use 2x CPU cores for better I/O utilization
        print(f"Building with {parallel_jobs} parallel jobs ({cpu_count} CPU cores)")

    # Add build timing and progress indicators
    import time

    if specific_test:
        print(f"Building specific test: test_{specific_test}")
        cmake_build_command = f"cmake --build {BUILD_DIR} --target test_{specific_test} --parallel {parallel_jobs}"
    else:
        print("Building all tests")
        cmake_build_command = f"cmake --build {BUILD_DIR} --parallel {parallel_jobs}"

    # Show expected timing for GCC builds
    if not USE_CLANG and not USE_ZIG:
        print(
            "🕐 GCC build starting - this may take 40+ seconds due to unified compilation overhead..."
        )
        print("    Progress indicators will show build steps as they complete")

    build_start_time = time.time()
    run_command(cmake_build_command)
    build_end_time = time.time()

    build_duration = build_end_time - build_start_time
    print(f"✅ FastLED library compiled successfully in {build_duration:.1f} seconds.")

    # Provide performance feedback
    if not USE_CLANG and not USE_ZIG and build_duration > 30:
        print(
            f"💡 Consider using Clang for faster builds: bash test --clang (expected ~7s vs {build_duration:.1f}s)"
        )
    elif build_duration > 60:
        print(
            f"⚠️  Build took longer than expected ({build_duration:.1f}s). Consider using --no-stack-trace if timeouts occur."
        )


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
    parser.add_argument(
        "--test",
        help="Specific test to compile (without test_ prefix)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Enable static analysis (IWYU, clang-tidy)",
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
            "specific_test": str(args.test) if args.test else "all",
        },
    }


def should_clean_build(build_info: dict[str, str | dict[str, str]]) -> bool:
    build_info_file = BUILD_DIR / "build_info.json"
    if not build_info_file.exists():
        return True

    with open(build_info_file, "r") as f:
        old_build_info = json.load(f)

    # If build parameters have changed, we need to rebuild
    if old_build_info != build_info:
        # Check if this is just a change in specific test target
        old_args = old_build_info.get("ARGS", {})
        new_args = build_info.get("ARGS", {})

        # Ensure ARGS is a dictionary
        if not isinstance(old_args, dict) or not isinstance(new_args, dict):
            return True

        # If only the specific test changed and everything else is the same,
        # we don't need to clean the build directory
        old_test = old_args.get("specific_test", "all")
        new_test = new_args.get("specific_test", "all")

        # Create copies without the specific_test field for comparison
        old_args_no_test = {k: v for k, v in old_args.items() if k != "specific_test"}
        new_args_no_test = {k: v for k, v in new_args.items() if k != "specific_test"}

        # If only the test target changed, don't clean
        if (
            old_args_no_test == new_args_no_test
            and old_build_info.get("USE_ZIG") == build_info.get("USE_ZIG")
            and old_build_info.get("USE_CLANG") == build_info.get("USE_CLANG")
            and old_build_info.get("WASM_BUILD") == build_info.get("WASM_BUILD")
            and old_build_info.get("CC") == build_info.get("CC")
            and old_build_info.get("CXX") == build_info.get("CXX")
            and old_build_info.get("AR") == build_info.get("AR")
            and old_build_info.get("CFLAGS") == build_info.get("CFLAGS")
            and old_build_info.get("CXXFLAGS") == build_info.get("CXXFLAGS")
        ):
            print(
                f"Build parameters unchanged, only test target changed: {old_test} -> {new_test}"
            )
            return False

        return True

    return False


def update_build_info(build_info: dict[str, str | dict[str, str]]):
    build_info_file = BUILD_DIR / "build_info.json"
    with open(build_info_file, "w") as f:
        json.dump(build_info, f, indent=2)


def main() -> None:
    global USE_ZIG, USE_CLANG, WASM_BUILD

    args = parse_arguments()
    USE_ZIG = args.use_zig  # use Zig's clang compiler
    USE_CLANG = args.use_clang  # Use pure Clang for WASM builds
    WASM_BUILD = args.wasm

    using_gcc = not USE_ZIG and not USE_CLANG and not WASM_BUILD
    if using_gcc:
        if not shutil.which("g++"):
            print(
                "gcc compiler not found in PATH, falling back zig's built in clang compiler"
            )
            USE_ZIG = True
            USE_CLANG = False

    if USE_CLANG:
        if not _has_system_clang_compiler():
            print(
                "Clang compiler not found in PATH, falling back to Zig-clang compiler"
            )
            USE_ZIG = True
            USE_CLANG = False

    os.chdir(str(HERE))
    print(f"Current directory: {Path('.').absolute()}")

    # Auto-detection for --clean based on test file changes
    need_clean = args.clean
    if not need_clean:
        # Only check for changes if --clean wasn't explicitly specified
        need_clean = check_test_files_changed()

    build_info = get_build_info(args)
    if need_clean or should_clean_build(build_info):
        clean_build_directory()
        # Save the file list after cleaning
        save_test_files_list()
    elif args.clean:
        # If --clean was explicitly specified but not needed according to build info,
        # still clean and save file list
        clean_build_directory()
        save_test_files_list()

    compile_fastled(args.test, enable_static_analysis=args.check)
    update_build_info(build_info)
    print("FastLED library compiled successfully.")


if __name__ == "__main__":
    main()
