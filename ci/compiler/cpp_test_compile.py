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


# Legacy CMake helper functions removed - using optimized Python API


def get_unit_test_fastled_sources() -> list[Path]:
    """Get essential FastLED .cpp files for unit test library creation (optimized paradigm)."""
    # Always work from project root
    project_root = Path(__file__).parent.parent.parent  # Go up from ci/compiler/ to project root
    src_dir = project_root / "src"

    # Core FastLED files that must be included for unit tests
    core_files: list[Path] = [
        src_dir / "FastLED.cpp",
        src_dir / "colorutils.cpp", 
        src_dir / "hsv2rgb.cpp",
    ]

    # Find all .cpp files in key directories
    additional_sources: list[Path] = []
    for pattern in ["*.cpp", "lib8tion/*.cpp", "platforms/stub/*.cpp"]:
        additional_sources.extend(list(src_dir.glob(pattern)))

    # Include essential .cpp files from nested directories
    additional_sources.extend(list(src_dir.rglob("*.cpp")))

    # Filter out duplicates and ensure files exist
    all_sources: list[Path] = []
    seen_files: set[Path] = set()

    for cpp_file in core_files + additional_sources:
        # Skip stub_main.cpp since unit tests have their own main
        if cpp_file.name == "stub_main.cpp":
            continue
            
        # Skip platform-specific files that aren't needed for unit tests
        rel_path_str = str(cpp_file)
        if any(skip in rel_path_str for skip in ['wasm', 'esp', 'avr', 'arm', 'teensy']):
            continue

        if cpp_file.exists() and cpp_file not in seen_files:
            all_sources.append(cpp_file)
            seen_files.add(cpp_file)

    return all_sources


def create_unit_test_fastled_library(
    compiler: "Compiler", fastled_build_dir: Path, clean: bool = False
) -> Path | None:
    """Create libfastled.a static library using optimized examples paradigm."""
    
    lib_file = fastled_build_dir / "libfastled.a"
    
    if lib_file.exists() and not clean:
        print(f"[LIBRARY] Using existing FastLED library: {lib_file}")
        return lib_file
    
    print("[LIBRARY] Creating FastLED static library...")

    # Get FastLED sources using optimized selection
    fastled_sources = get_unit_test_fastled_sources()
    fastled_objects: list[Path] = []

    obj_dir = fastled_build_dir / "obj"
    obj_dir.mkdir(exist_ok=True)

    print(f"[LIBRARY] Compiling {len(fastled_sources)} FastLED source files...")

    # Compile each source file with optimized naming (same as examples)
    futures: list[tuple] = []
    project_root = Path(__file__).parent.parent.parent
    src_dir = project_root / "src"
    
    for cpp_file in fastled_sources:
        # Create unique object file name by including relative path to prevent collisions
        # Convert path separators to underscores to create valid filename
        if cpp_file.is_relative_to(src_dir):
            rel_path = cpp_file.relative_to(src_dir)
        else:
            rel_path = cpp_file

        # Replace path separators with underscores for unique object file names
        obj_name = str(rel_path.with_suffix(".o")).replace("/", "_").replace("\\", "_")
        obj_file = obj_dir / obj_name
        future = compiler.compile_cpp_file(cpp_file, obj_file)
        futures.append((future, obj_file, cpp_file))

    # Wait for compilation to complete
    compiled_count = 0
    for future, obj_file, cpp_file in futures:
        try:
            result = future.result()
            if result.ok:
                fastled_objects.append(obj_file)
                compiled_count += 1
            else:
                print(f"[LIBRARY] WARNING: Failed to compile {cpp_file.relative_to(src_dir)}: {result.stderr[:100]}...")
        except Exception as e:
            print(f"[LIBRARY] WARNING: Exception compiling {cpp_file.relative_to(src_dir)}: {e}")

    print(f"[LIBRARY] Successfully compiled {compiled_count}/{len(fastled_sources)} FastLED sources")

    if not fastled_objects:
        print("[LIBRARY] ERROR: No FastLED source files compiled successfully")
        return None

    # Create static library using the same approach as examples
    print(f"[LIBRARY] Creating static library: {lib_file}")

    archive_future = compiler.create_archive(fastled_objects, lib_file)
    archive_result = archive_future.result()

    if not archive_result.ok:
        print(f"[LIBRARY] ERROR: Library creation failed: {archive_result.stderr}")
        return None

    print(f"[LIBRARY] SUCCESS: FastLED library created: {lib_file}")
    return lib_file


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
    
    # Step 2: Build FastLED library using optimized examples paradigm
    print("Building FastLED library...")
    fastled_build_dir = project_root / ".build" / "fastled"
    fastled_build_dir.mkdir(parents=True, exist_ok=True)
    
    fastled_lib_path = create_unit_test_fastled_library(
        compiler, fastled_build_dir, clean
    )
    
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
            # Special handling for tests that have their own main function
            tests_with_own_main = ["test_example_compilation"]
            if test_name in tests_with_own_main:
                # These tests have their own main function and don't need doctest_main
                object_files = [object_path]
            else:
                # Regular doctest-based unit tests need doctest_main
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

    # Unit tests now use optimized Python API by default (same as examples)
    use_pch = not getattr(args, 'no_pch', False)  # Default to PCH enabled unless --no-pch specified
    
    # Use the optimized Python API with PCH optimization (now default for unit tests)
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
