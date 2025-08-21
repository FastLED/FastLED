#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportReturnType=false, reportMissingParameterType=false
import argparse
import hashlib
import json
import logging
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import Future
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple, Union

from ci.util.paths import PROJECT_ROOT
from ci.util.running_process import RunningProcess
from ci.util.test_args import parse_args as parse_global_test_args

from .clang_compiler import (
    BuildFlags,
    Compiler,
    CompilerOptions,
    LinkOptions,
    test_clang_accessibility,
)
from .test_example_compilation import create_fastled_compiler


# Configure logging
logger = logging.getLogger(__name__)

BUILD_DIR = PROJECT_ROOT / "tests" / ".build"
BUILD_DIR.mkdir(parents=True, exist_ok=True)
CACHE_DIR = PROJECT_ROOT / ".cache"
CACHE_DIR.mkdir(parents=True, exist_ok=True)
TEST_FILES_LIST = CACHE_DIR / "test_files_list.txt"


# ============================================================================
# HASH-BASED LINKING CACHE (same optimization as examples)
# ============================================================================


def calculate_file_hash(file_path: Path) -> str:
    """Calculate SHA256 hash of a file (same as FastLEDTestCompiler)"""
    if not file_path.exists():
        return "no_file"

    hash_sha256 = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_sha256.update(chunk)
    return hash_sha256.hexdigest()


def calculate_linker_args_hash(linker_args: list[str]) -> str:
    """Calculate SHA256 hash of linker arguments (same as FastLEDTestCompiler)"""
    args_str = "|".join(sorted(linker_args))
    hash_sha256 = hashlib.sha256()
    hash_sha256.update(args_str.encode("utf-8"))
    return hash_sha256.hexdigest()


def calculate_link_cache_key(
    object_files: list[str | Path], fastled_lib_path: Path, linker_args: list[str]
) -> str:
    """Calculate comprehensive cache key for linking (same as examples)"""
    # Calculate hash for each object file
    obj_hashes: list[str] = []
    for obj_file in object_files:
        obj_hashes.append(calculate_file_hash(Path(obj_file)))

    # Combine object file hashes in a stable way (sorted by path for consistency)
    sorted_obj_paths = sorted(str(obj) for obj in object_files)
    sorted_obj_hashes: list[str] = []
    for obj_path in sorted_obj_paths:
        obj_file = Path(obj_path)
        sorted_obj_hashes.append(calculate_file_hash(obj_file))

    combined_obj_hash = hashlib.sha256(
        "|".join(sorted_obj_hashes).encode("utf-8")
    ).hexdigest()

    # Calculate other hashes
    fastled_hash = calculate_file_hash(fastled_lib_path)
    linker_hash = calculate_linker_args_hash(linker_args)

    # Combine all components (same format as FastLEDTestCompiler)
    combined = f"fastled:{fastled_hash}|objects:{combined_obj_hash}|flags:{linker_hash}"
    final_hash = hashlib.sha256(combined.encode("utf-8")).hexdigest()

    return final_hash[:16]  # Use first 16 chars for readability


def get_link_cache_dir() -> Path:
    """Get the link cache directory (same as examples)"""
    cache_dir = Path(".build/link_cache")
    cache_dir.mkdir(parents=True, exist_ok=True)
    return cache_dir


def get_cached_executable(test_name: str, cache_key: str) -> Optional[Path]:
    """Check if cached executable exists (same as examples)"""
    cache_dir = get_link_cache_dir()
    cached_exe = cache_dir / f"{test_name}_{cache_key}.exe"
    return cached_exe if cached_exe.exists() else None


def cache_executable(test_name: str, cache_key: str, exe_path: Path) -> None:
    """Cache an executable for future use (same as examples)"""
    cache_dir = get_link_cache_dir()
    cached_exe = cache_dir / f"{test_name}_{cache_key}.exe"
    try:
        shutil.copy2(exe_path, cached_exe)
    except Exception as e:
        print(f"Warning: Failed to cache {test_name}: {e}")


# ============================================================================
# END HASH-BASED LINKING CACHE
# ============================================================================


def get_test_files() -> Set[str]:
    """Get a set of all test files"""
    test_files: Set[str] = set()
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
    project_root = Path(
        __file__
    ).parent.parent.parent  # Go up from ci/compiler/ to project root
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
        if any(
            skip in rel_path_str for skip in ["wasm", "esp", "avr", "arm", "teensy"]
        ):
            continue

        if cpp_file.exists() and cpp_file not in seen_files:
            all_sources.append(cpp_file)
            seen_files.add(cpp_file)

    return all_sources


def create_unit_test_fastled_library(
    clean: bool = False, use_pch: bool = True
) -> Path | None:
    """Create libfastled.a static library specifically for unit tests with FASTLED_FORCE_NAMESPACE=1.

    CRITICAL: Unit tests need their own separate library from examples because they use
    different compilation flags. Unit tests require FASTLED_FORCE_NAMESPACE=1 to put
    all symbols in the fl:: namespace that the tests expect.
    """

    # Unit tests get their own separate library directory under .build/fastled/unit/
    fastled_build_dir = BUILD_DIR.parent / ".build" / "fastled" / "unit"
    fastled_build_dir.mkdir(parents=True, exist_ok=True)
    lib_file = fastled_build_dir / "libfastled.a"

    if lib_file.exists() and not clean:
        print(f"[LIBRARY] Using existing FastLED library: {lib_file}")
        return lib_file

    print("[LIBRARY] Creating FastLED static library with proper compiler flags...")

    # Create a proper FastLED compiler (NOT unit test compiler) for the library
    # This ensures the library is compiled without FASTLED_FORCE_NAMESPACE=1
    print("[LIBRARY] Creating FastLED library compiler (without unit test flags)...")

    # Save current directory and change to project root for create_fastled_compiler
    import os

    project_root = Path(__file__).parent.parent.parent
    original_cwd = os.getcwd()
    os.chdir(str(project_root))

    try:
        library_compiler = create_fastled_compiler(
            use_pch=use_pch,
            parallel=True,
        )

        # CRITICAL: Add required defines for unit test library compilation
        if library_compiler.settings.defines is None:
            library_compiler.settings.defines = []

        # Add FASTLED_FORCE_NAMESPACE=1 to export symbols in fl:: namespace
        library_compiler.settings.defines.append("FASTLED_FORCE_NAMESPACE=1")
        print("[LIBRARY] Added FASTLED_FORCE_NAMESPACE=1 to library compiler")

        # Add FASTLED_TESTING=1 to include MockTimeProvider and test utility functions
        library_compiler.settings.defines.append("FASTLED_TESTING=1")
        print("[LIBRARY] Added FASTLED_TESTING=1 to library compiler")

    finally:
        # Restore original working directory
        os.chdir(original_cwd)

    # Get FastLED sources using optimized selection
    fastled_sources = get_unit_test_fastled_sources()
    fastled_objects: list[Path] = []

    obj_dir = fastled_build_dir / "obj"
    obj_dir.mkdir(exist_ok=True)

    print(f"[LIBRARY] Compiling {len(fastled_sources)} FastLED source files...")

    # Compile each source file with optimized naming (same as examples)
    futures: List[Tuple[Any, ...]] = []
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
        future = library_compiler.compile_cpp_file(cpp_file, obj_file)
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
                print(
                    f"[LIBRARY] WARNING: Failed to compile {cpp_file.relative_to(src_dir)}: {result.stderr[:100]}..."
                )
        except Exception as e:
            print(
                f"[LIBRARY] WARNING: Exception compiling {cpp_file.relative_to(src_dir)}: {e}"
            )

    print(
        f"[LIBRARY] Successfully compiled {compiled_count}/{len(fastled_sources)} FastLED sources"
    )

    if not fastled_objects:
        print("[LIBRARY] ERROR: No FastLED source files compiled successfully")
        return None

    # Create static library using the same approach as examples
    print(f"[LIBRARY] Creating static library: {lib_file}")

    archive_future = library_compiler.create_archive(fastled_objects, lib_file)
    archive_result = archive_future.result()

    if not archive_result.ok:
        print(f"[LIBRARY] ERROR: Library creation failed: {archive_result.stderr}")
        return None

    print(f"[LIBRARY] SUCCESS: FastLED library created: {lib_file}")
    return lib_file


def create_unit_test_compiler(
    use_pch: bool = True, enable_static_analysis: bool = False, debug: bool = False
) -> Compiler:
    """Create compiler optimized for unit test compilation with PCH support."""

    # Always work from the project root, not from ci/compiler
    project_root = Path(
        __file__
    ).parent.parent.parent  # Go up from ci/compiler/ to project root
    current_dir = project_root
    src_path = current_dir / "src"

    # Load build flags configuration
    build_flags_path = current_dir / "ci" / "build_unit.toml"
    build_flags = BuildFlags.parse(
        build_flags_path, quick_build=True, strict_mode=False
    )

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
        "ENABLE_CRASH_HANDLER",
        "RELEASE=1",  # Disable FASTLED_FORCE_DBG to avoid fl::println dependency
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
        # Optimization/debug controls set below based on debug flag
        "-fno-omit-frame-pointer",
        "-fno-inline-functions",
        "-fno-vectorize",
        "-fno-unroll-loops",
        "-fno-strict-aliasing",
        f"-I{current_dir}",
        f"-I{src_path}",
        f"-I{current_dir / 'tests'}",
        f"-I{src_path / 'platforms' / 'stub'}",
    ]

    # Apply quick vs debug modes
    if debug:
        unit_test_args.extend(
            [
                "-O0",
                "-g3",
                "-fstandalone-debug",
            ]
        )
        if os.name == "nt":
            unit_test_args.extend(
                ["-gdwarf-4"]
            )  # GNU debug info for Windows GNU toolchain
    else:
        unit_test_args.extend(
            [
                "-O0",
                "-g0",
            ]
        )

    # Note: DWARF flags are added above conditionally when debug=True

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
        print(
            f"[PCH] PCH includes: test.h, FastLED.h, lib8tion.h, colorutils.h, and more"
        )
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
    clean: bool = False,
    debug: bool = False,
) -> bool:
    """Compile unit tests using the fast Python API instead of CMake.

    Returns:
        bool: True if all tests compiled and linked successfully, False otherwise
    """
    from .clang_compiler import Compiler, LinkOptions

    print("=" * 60)
    print("COMPILING UNIT TESTS WITH PYTHON API")
    print("=" * 60)

    if clean:
        print("Cleaning build directory...")
        shutil.rmtree(BUILD_DIR, ignore_errors=True)
        BUILD_DIR.mkdir(parents=True, exist_ok=True)

    # Create optimized compiler for unit tests
    compiler = create_unit_test_compiler(
        use_pch=use_pch, enable_static_analysis=enable_static_analysis, debug=debug
    )

    # Find all test files - work from project root
    project_root = Path(__file__).parent.parent.parent
    tests_dir = project_root / "tests"
    test_files = []

    if specific_test:
        # Handle specific test with case-insensitive matching
        test_name = (
            specific_test
            if specific_test.startswith("test_")
            else f"test_{specific_test}"
        )
        test_file = tests_dir / f"{test_name}.cpp"

        # First try exact case match
        if test_file.exists():
            test_files = [test_file]
            print(f"Compiling specific test: {test_file.name}")
        else:
            # Try case-insensitive matching for all test files
            found_match = False
            for existing_file in tests_dir.glob("test_*.cpp"):
                # Check if the file matches case-insensitively
                existing_stem = existing_file.stem
                existing_name = existing_stem.replace("test_", "")

                if (
                    existing_stem.lower() == test_name.lower()
                    or existing_name.lower() == specific_test.lower()
                ):
                    test_files = [existing_file]
                    print(
                        f"Compiling specific test (case-insensitive match): {existing_file.name}"
                    )
                    found_match = True
                    break

            if not found_match:
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

    # Create clean execution directory as specified in requirements
    clean_bin_dir = PROJECT_ROOT / "tests" / "bin"
    clean_bin_dir.mkdir(parents=True, exist_ok=True)

    # Step 1: Compile doctest main once
    print("Compiling doctest main...")
    doctest_main_path = tests_dir / "doctest_main.cpp"
    doctest_main_obj = bin_dir / "doctest_main.o"

    if not doctest_main_obj.exists() or clean:
        doctest_compile_future = compiler.compile_cpp_file(
            cpp_path=str(doctest_main_path), output_path=str(doctest_main_obj)
        )
        doctest_result = doctest_compile_future.result()

        if doctest_result.return_code != 0:
            raise RuntimeError(
                f"Failed to compile doctest main: {doctest_result.stderr}"
            )

    # Step 2: Build FastLED library using optimized examples paradigm
    print("Building FastLED library...")

    fastled_lib_path = create_unit_test_fastled_library(clean, use_pch=use_pch)

    # Step 3: Compile and link each test (PARALLEL OPTIMIZATION)
    print(f"Compiling {len(test_files)} tests...")
    start_time = time.time()

    # Phase 1: Start all compilations in parallel (NON-BLOCKING)
    print("🚀 Starting parallel compilation...")
    compile_start = time.time()
    compile_futures: Dict[str, Future[Any]] = {}
    test_info: Dict[str, Any] = {}

    for test_file in test_files:
        test_name = test_file.stem
        executable_path = bin_dir / test_name
        object_path = bin_dir / f"{test_name}.o"

        test_info[test_name] = {
            "test_file": test_file,
            "executable_path": executable_path,
            "object_path": object_path,
        }

        print(f"  Compiling {test_name}...")

        # Start compilation (NON-BLOCKING)
        compile_future = compiler.compile_cpp_file(
            cpp_path=str(test_file), output_path=str(object_path)
        )
        compile_futures[test_name] = compile_future

    # Phase 2: Wait for all compilations to complete and check cache before linking
    compile_dispatch_time = time.time() - compile_start
    print(
        f"⏳ Waiting for compilations to complete... (dispatch took {compile_dispatch_time:.2f}s)"
    )
    compile_wait_start = time.time()
    link_futures: Dict[str, Dict[str, Any]] = {}
    cache_hits = 0
    success_count = 0

    for test_name, compile_future in compile_futures.items():
        try:
            compile_result = (
                compile_future.result()
            )  # BLOCKING WAIT for this specific test

            if compile_result.return_code != 0:
                print(f"❌ Compilation failed for {test_name}: {compile_result.stderr}")
                continue

            # Prepare linking info (same logic as before)
            info = test_info[test_name]
            executable_path = info["executable_path"]
            object_path = info["object_path"]

            tests_with_own_main = ["test_example_compilation"]
            if test_name in tests_with_own_main:
                object_files: list[str | Path] = [object_path]
            else:
                object_files: list[str | Path] = [object_path, doctest_main_obj]

            # Platform-specific linker arguments for crash handler support
            if os.name == "nt":  # Windows
                linker_args = ["-ldbghelp", "-lpsapi"]
            else:  # Linux/macOS
                linker_args = ["-pthread"]

            # HASH-BASED CACHE CHECK (same as examples)
            if not fastled_lib_path:
                print(f"⚠️  No FastLED library found, skipping cache for {test_name}")
                cache_key = "no_fastled_lib"
                cached_exe = None
            else:
                cache_key = calculate_link_cache_key(
                    object_files, fastled_lib_path, linker_args
                )
                cached_exe = get_cached_executable(test_name, cache_key)

            if cached_exe:
                # Cache hit! Copy cached executable to target location
                try:
                    shutil.copy2(cached_exe, executable_path)
                    # Also copy to clean execution directory
                    clean_executable_path = clean_bin_dir / f"{test_name}.exe"
                    shutil.copy2(cached_exe, clean_executable_path)
                    cache_hits += 1
                    success_count += 1
                    print(f"  ⚡ {test_name}: Using cached executable (cache hit)")
                    continue  # Skip linking entirely
                except Exception as e:
                    print(f"  ⚠️  Failed to copy cached {test_name}, will relink: {e}")
                    # Fall through to actual linking

            # Cache miss - proceed with actual linking
            static_libraries: List[Union[str, Path]] = []
            if fastled_lib_path and fastled_lib_path.exists():
                static_libraries.append(fastled_lib_path)

            link_options = LinkOptions(
                output_executable=str(executable_path),
                object_files=object_files,
                static_libraries=static_libraries,
                linker_args=linker_args,
            )

            # Start linking (NON-BLOCKING) and store cache info for later
            link_future = compiler.link_program(link_options)
            link_futures[test_name] = {
                "future": link_future,
                "cache_key": cache_key,
                "executable_path": executable_path,
            }

        except Exception as e:
            print(f"❌ ERROR compiling {test_name}: {e}")
            continue

    # Phase 3: Wait for all linking to complete and cache successful results
    compile_wait_time = time.time() - compile_wait_start
    print(
        f"🔗 Waiting for linking to complete... (compilation took {compile_wait_time:.2f}s)"
    )
    link_start = time.time()
    cache_misses = 0

    for test_name, link_info in link_futures.items():
        try:
            link_future = link_info["future"]
            cache_key = link_info["cache_key"]
            executable_path = link_info["executable_path"]

            link_result = link_future.result()  # BLOCKING WAIT for this specific test

            if link_result.return_code != 0:
                print(f"⚠️  Linking failed for {test_name}: {link_result.stderr}")
                cache_misses += 1
                continue
            else:
                success_count += 1
                cache_misses += 1  # This was a fresh link, not from cache

                # Cache the successful executable for future use (same as examples)
                cache_executable(test_name, cache_key, executable_path)

                # Also copy to clean execution directory
                clean_executable_path = clean_bin_dir / f"{test_name}.exe"
                shutil.copy2(executable_path, clean_executable_path)

        except Exception as e:
            print(f"❌ ERROR linking {test_name}: {e}")
            cache_misses += 1
            continue

    link_time = time.time() - link_start
    compilation_time = time.time() - start_time

    print(f"✅ Unit test compilation completed in {compilation_time:.2f}s")
    print(f"   📊 Time breakdown:")
    print(f"      • Dispatch: {compile_dispatch_time:.2f}s")
    print(f"      • Compilation: {compile_wait_time:.2f}s")
    print(f"      • Linking: {link_time:.2f}s")
    print(f"   🎯 Cache statistics (archive + hash linking optimization):")
    print(f"      • Cache hits: {cache_hits} (skipped linking)")
    print(f"      • Cache misses: {cache_misses} (fresh linking)")
    print(
        f"      • Cache hit ratio: {cache_hits / max(1, cache_hits + cache_misses) * 100:.1f}%"
    )
    print(f"   Successfully compiled: {success_count}/{len(test_files)} tests")
    print(f"   Average: {compilation_time / len(test_files):.2f}s per test")
    print(f"   Output directory: {bin_dir}")
    print(f"   Cache directory: {get_link_cache_dir()}")

    # Return success only if ALL tests compiled and linked successfully
    all_tests_successful = success_count == len(test_files)
    if not all_tests_successful:
        failed_count = len(test_files) - success_count
        print(f"❌ {failed_count} test(s) failed to compile or link")

    return all_tests_successful


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
    parser.add_argument("--no-unity", action="store_true", help="Disable unity build")
    parser.add_argument(
        "--no-pch", action="store_true", help="Disable precompiled headers (PCH)"
    )
    parser.add_argument("--debug", action="store_true", help="Enable debug symbols")

    parser.add_argument("--verbose", action="store_true", help="Enable verbose output")
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

    try:
        with open(build_info_file, "r") as f:
            old_build_info = json.load(f)
    except (json.JSONDecodeError, ValueError) as e:
        # If JSON is corrupted or empty, remove it and force clean build
        logger.warning(
            f"Corrupted build_info.json detected ({e}), removing and forcing clean build"
        )
        try:
            build_info_file.unlink()
        except OSError:
            pass  # Ignore if we can't remove it
        return True

    # If build parameters have changed, we need to rebuild
    if old_build_info != build_info:
        # Check if this is just a change in specific test target
        old_args_raw = old_build_info.get("ARGS", {})
        new_args_raw = build_info.get("ARGS", {})

        # Ensure ARGS is a dictionary
        if not isinstance(old_args_raw, dict) or not isinstance(new_args_raw, dict):
            return True

        old_args: Dict[str, Any] = old_args_raw  # type: ignore
        new_args: Dict[str, Any] = new_args_raw  # type: ignore

        # If only the specific test changed and everything else is the same,
        # we don't need to clean the build directory
        old_test = old_args.get("specific_test", "all")
        new_test = new_args.get("specific_test", "all")

        # Create copies without the specific_test field for comparison
        old_args_no_test: Dict[str, Any] = {
            k: v for k, v in old_args.items() if k != "specific_test"
        }
        new_args_no_test: Dict[str, Any] = {
            k: v for k, v in new_args.items() if k != "specific_test"
        }

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
        if not test_clang_accessibility():
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
    use_pch = not getattr(
        args, "no_pch", False
    )  # Default to PCH enabled unless --no-pch specified

    # Use the optimized Python API with PCH optimization (now default for unit tests)
    compilation_successful = compile_unit_tests_python_api(
        specific_test=args.test,
        enable_static_analysis=args.check,
        use_pch=use_pch,
        clean=need_clean,
        debug=args.debug,
    )

    if compilation_successful:
        update_build_info(build_info)
        print("FastLED library compiled successfully.")
    else:
        print("❌ FastLED unit test compilation failed!")
        import sys

        sys.exit(1)


if __name__ == "__main__":
    main()
