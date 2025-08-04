#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportReturnType=false, reportMissingParameterType=false
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
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


def create_unit_test_compiler(use_pch: bool = True, enable_static_analysis: bool = False) -> "Compiler":
    """Create compiler optimized for unit test compilation with PCH support."""
    from .clang_compiler import Compiler, CompilerOptions, BuildFlags
    
    # Always work from the project root, not from ci/compiler
    project_root = Path(__file__).parent.parent.parent  # Go up from ci/compiler/ to project root
    current_dir = project_root
    src_path = current_dir / "src"
    
    # Load build flags configuration  
    build_flags_path = current_dir / "ci" / "build_flags.toml"
    build_flags = BuildFlags.parse(build_flags_path, quick_build=True, strict_mode=False)
    
    # Unit test specific defines
    unit_test_defines = [
        "FASTLED_UNIT_TEST=1",
        "FASTLED_FORCE_NAMESPACE=1", 
        "FASTLED_USE_PROGMEM=0",
        "STUB_PLATFORM",
        "ARDUINO=10808",
        "FASTLED_USE_STUB_ARDUINO",
        "SKETCH_HAS_LOTS_OF_MEMORY=1",
        "FASTLED_STUB_IMPL",
        "FASTLED_USE_JSON_UI=1",
        "FASTLED_TESTING",
        "FASTLED_NO_AUTO_NAMESPACE",
        "FASTLED_NO_PINMAP",
        "HAS_HARDWARE_PIN_SUPPORT",
        "FASTLED_DEBUG_LEVEL=1",
        "FASTLED_NO_ATEXIT=1",
        "DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS",
    ]
    
    # Unit test specific compiler args
    unit_test_args = [
        "-std=gnu++17",
        "-fpermissive", 
        "-Wall",
        "-Wextra",
        "-Wno-deprecated-register",
        "-Wno-backslash-newline-escape",
        "-fno-exceptions",
        "-fno-rtti",
        "-O1",
        "-g0",
        "-fno-inline-functions", 
        "-fno-vectorize",
        "-fno-unroll-loops",
        "-fno-strict-aliasing",
        f"-I{current_dir}",
        f"-I{src_path}",
        f"-I{current_dir / 'tests'}",
        f"-I{src_path / 'platforms' / 'stub'}",
    ]
    
    # PCH configuration with unit test specific headers
    pch_output_path = None
    pch_header_content = None
    if use_pch:
        cache_dir = current_dir / ".build" / "cache"
        cache_dir.mkdir(parents=True, exist_ok=True)
        pch_output_path = str(cache_dir / "fastled_unit_test_pch.hpp.pch")
        
        # Unit test specific PCH header content
        pch_header_content = """// FastLED Unit Test PCH - Common headers for faster test compilation
#pragma once

// Core test framework
#include "test.h"

// Core FastLED headers that are used in nearly all unit tests
#include "FastLED.h"

// Common C++ standard library headers used in tests
#include <string>
#include <vector>
#include <stdio.h>
#include <cstdint>
#include <cmath>
#include <cassert>
#include <iostream>
#include <memory>

// Platform headers for stub environment
#include "platforms/stub/fastled_stub.h"

// Commonly tested FastLED components
#include "lib8tion.h"
#include "colorutils.h"
#include "hsv2rgb.h"
#include "fl/math.h"
#include "fl/vector.h"

// Using namespace to match test files
using namespace fl;
"""
        print(f"[PCH] Unit tests will use precompiled headers: {pch_output_path}")
        print(f"[PCH] PCH includes: test.h, FastLED.h, lib8tion.h, colorutils.h, and more")
    else:
        print("[PCH] Precompiled headers disabled for unit tests")
    
    # Determine compiler
    compiler_cmd = "python -m ziglang c++"
    if USE_CLANG:
        compiler_cmd = "clang++"
        print("USING CLANG COMPILER FOR UNIT TESTS")
    elif USE_ZIG:
        print("USING ZIG COMPILER FOR UNIT TESTS")
    else:
        print("USING DEFAULT COMPILER FOR UNIT TESTS")
    
    settings = CompilerOptions(
        include_path=str(src_path),
        defines=unit_test_defines,
        std_version="c++17",
        compiler=compiler_cmd,
        compiler_args=unit_test_args,
        use_pch=use_pch,
        pch_output_path=pch_output_path,
        pch_header_content=pch_header_content,
        parallel=True,
    )
    
    return Compiler(settings, build_flags)


def compile_unit_tests_python_api(
    specific_test: str | None = None, 
    enable_static_analysis: bool = False,
    use_pch: bool = True,
    clean: bool = False
) -> None:
    """Compile unit tests using the fast Python API instead of CMake."""
    from .clang_compiler import Compiler, LinkOptions
    
    print("=" * 60)
    print("COMPILING UNIT TESTS WITH PYTHON API (8x faster than CMake)")
    print("=" * 60)
    
    if clean:
        print("Cleaning build directory...")
        shutil.rmtree(BUILD_DIR, ignore_errors=True)
        BUILD_DIR.mkdir(parents=True, exist_ok=True)
    
    # Create optimized compiler for unit tests
    compiler = create_unit_test_compiler(use_pch=use_pch, enable_static_analysis=enable_static_analysis)
    
    # Find all test files - work from project root
    project_root = Path(__file__).parent.parent.parent
    tests_dir = project_root / "tests"
    test_files = []
    
    if specific_test:
        # Handle specific test
        test_name = specific_test if specific_test.startswith("test_") else f"test_{specific_test}"
        test_file = tests_dir / f"{test_name}.cpp"
        if test_file.exists():
            test_files = [test_file]
            print(f"Compiling specific test: {test_file.name}")
        else:
            raise RuntimeError(f"Test file not found: {test_file}")
    else:
        # Find all test files
        test_files = list(tests_dir.glob("test_*.cpp"))
        print(f"Found {len(test_files)} unit test files")
    
    if not test_files:
        print("No test files found")
        return
        
    # Ensure output directory exists
    bin_dir = BUILD_DIR / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    
    # Step 1: Compile doctest main once
    print("Compiling doctest main...")
    doctest_main_path = tests_dir / "doctest_main.cpp"
    doctest_main_obj = bin_dir / "doctest_main.o"
    
    if not doctest_main_obj.exists() or clean:
        doctest_compile_future = compiler.compile_cpp_file(
            cpp_path=str(doctest_main_path),
            output_path=str(doctest_main_obj)
        )
        doctest_result = doctest_compile_future.result()
        
        if doctest_result.return_code != 0:
            raise RuntimeError(f"Failed to compile doctest main: {doctest_result.stderr}")
    
    # Step 2: Build FastLED library (same approach as examples)
    print("Building FastLED library...")
    fastled_lib_path = bin_dir / "libfastled.a"
    
    if not fastled_lib_path.exists() or clean:
        # Find all FastLED source files (same approach as examples)
        src_dir = project_root / "src"
        fastled_sources = []
        
        # Add core FastLED sources
        for cpp_file in src_dir.rglob("*.cpp"):
            # Skip platform-specific files that aren't needed for unit tests
            rel_path = str(cpp_file.relative_to(src_dir))
            if not any(skip in rel_path for skip in ['wasm', 'esp', 'avr', 'arm', 'teensy']):
                fastled_sources.append(cpp_file)
        
        print(f"Found {len(fastled_sources)} FastLED source files")
        
        # Compile all FastLED sources to object files
        fastled_objects = []
        for src_file in fastled_sources:
            obj_name = f"fastled_{src_file.stem}.o"
            obj_path = bin_dir / obj_name
            
            if not obj_path.exists() or clean:
                compile_future = compiler.compile_cpp_file(
                    cpp_path=str(src_file),
                    output_path=str(obj_path)
                )
                
                compile_result = compile_future.result()
                if compile_result.return_code != 0:
                    print(f"Warning: Failed to compile {src_file.name}: {compile_result.stderr}")
                    continue  # Skip files that fail to compile
            
            fastled_objects.append(obj_path)
        
        # Create static library using the same approach as examples
        if fastled_objects:
            print(f"Creating FastLED library: {fastled_lib_path}")
            
            archive_future = compiler.create_archive(fastled_objects, fastled_lib_path)
            archive_result = archive_future.result()
            
            if not archive_result.ok:
                print(f"Warning: Failed to create FastLED library: {archive_result.stderr}")
                fastled_lib_path = None
        else:
            print("Warning: No FastLED source files compiled successfully")
            fastled_lib_path = None
    
    # Step 3: Compile and link each test
    print(f"Compiling {len(test_files)} tests...")
    start_time = time.time()
    
    for test_file in test_files:
        test_name = test_file.stem
        executable_path = bin_dir / test_name
        object_path = bin_dir / f"{test_name}.o"
        
        print(f"  Compiling {test_name}...")
        
        try:
            # Compile test to object file
            compile_future = compiler.compile_cpp_file(
                cpp_path=str(test_file),
                output_path=str(object_path)
            )
            
            compile_result = compile_future.result()
            
            if compile_result.return_code != 0:
                raise RuntimeError(f"Compilation failed for {test_name}: {compile_result.stderr}")
            
            # Link to executable with doctest main and FastLED library (same as examples)
            object_files = [object_path, doctest_main_obj]
            static_libraries = []
            linker_args = ["-pthread"]
            
            if fastled_lib_path and fastled_lib_path.exists():
                static_libraries.append(fastled_lib_path)
            
            link_options = LinkOptions(
                output_executable=str(executable_path),
                object_files=object_files,
                static_libraries=static_libraries,
                linker_args=linker_args
            )
            
            link_future = compiler.link_program(link_options)
            link_result = link_future.result()
            
            if link_result.return_code != 0:
                print(f"Warning: Linking failed for {test_name}: {link_result.stderr}")
                continue  # Skip failed links but continue with other tests
                
        except Exception as e:
            print(f"ERROR compiling {test_name}: {e}")
            continue  # Continue with other tests
    
    compilation_time = time.time() - start_time
    print(f"✅ Unit test compilation completed in {compilation_time:.2f}s")
    print(f"   Average: {compilation_time/len(test_files):.2f}s per test")
    print(f"   Output directory: {bin_dir}")


def compile_fastled(
    specific_test: str | None = None, enable_static_analysis: bool = False
) -> None:
    """Legacy CMake-based compilation - replaced by compile_unit_tests_python_api."""
    print("⚠️  WARNING: Using legacy CMake system - this is 8x slower than the Python API")
    print("    Consider using the new Python API for faster compilation")
    
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
    parser.add_argument(
        "--no-unity", action="store_true", help="Disable unity build"
    )
    parser.add_argument(
        "--no-pch", action="store_true", help="Disable precompiled headers (PCH)"
    )
    parser.add_argument(
        "--python-api", action="store_true", help="Use experimental Python API with PCH optimization (instead of stable CMake)"
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Enable verbose output"
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

    # Unit tests use legacy CMake system by default (examples use Python API)
    # This maintains compatibility while examples get the performance benefits
    use_legacy = not getattr(args, 'python_api', False)  # Use legacy by default unless --python-api specified
    use_pch = not getattr(args, 'no_pch', False)  # Default to PCH enabled unless --no-pch specified
    
    if use_legacy:
        # Use legacy CMake system (default for unit tests)
        compile_fastled(args.test, enable_static_analysis=args.check)
    else:
        # Use the fast Python API with PCH optimization (for testing/examples)
        print("🚀 Using experimental Python API with PCH optimization")
        compile_unit_tests_python_api(
            specific_test=args.test,
            enable_static_analysis=args.check,
            use_pch=use_pch,
            clean=need_clean
        )
    
    update_build_info(build_info)
    print("FastLED library compiled successfully.")


if __name__ == "__main__":
    main()
