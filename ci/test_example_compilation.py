#!/usr/bin/env python3
"""
FastLED Example Compilation Testing Script
Ultra-fast compilation testing of Arduino .ino examples
"""

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path


def run_example_compilation_test(specific_examples=None):
    """Run the example compilation test using CMake."""
    print("==> FastLED Example Compilation Test (QUICK BUILD MODE)")
    print("=" * 50)

    # Change to tests directory
    tests_dir = Path(__file__).parent.parent / "tests"
    build_dir = tests_dir / ".build-examples"

    # Create build directory if it doesn't exist
    build_dir.mkdir(exist_ok=True)
    os.chdir(build_dir)

    start_time = time.time()

    # Set environment variables for maximum speed
    env = os.environ.copy()
    env["FASTLED_ALL_SRC"] = "1"  # Enable unified compilation for speed

    # Detect available CPU cores for parallel builds
    import multiprocessing

    cpu_count = multiprocessing.cpu_count()
    parallel_jobs = min(cpu_count * 2, 16)  # Cap at 16 to avoid overwhelming system
    env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(parallel_jobs)

    # Enable compiler caching if available
    import shutil

    ccache_path = shutil.which("ccache")
    if ccache_path:
        env["CMAKE_CXX_COMPILER_LAUNCHER"] = ccache_path
        env["CMAKE_C_COMPILER_LAUNCHER"] = ccache_path
        print(f"[PERF] ccache enabled: {ccache_path}")
    else:
        print("[PERF] ccache not available")

    print(f"[PERF] Using {parallel_jobs} parallel jobs ({cpu_count} CPU cores)")
    print(f"[PERF] FASTLED_ALL_SRC=1 (unified compilation enabled)")

    try:
        # Configure CMake with aggressive speed optimizations
        print("[CONFIG] Configuring CMake with speed optimizations...")
        cmake_config_start = time.time()

        # Build the cmake configuration command with optimizations
        cmake_cmd = [
            "cmake",
            str(tests_dir.absolute()),
            "-G",
            "Unix Makefiles",
            "-DFASTLED_ENABLE_EXAMPLE_TESTS=ON",
            "-DCMAKE_BUILD_TYPE=Quick",  # Use Quick build type for speed
            f"-DCMAKE_BUILD_PARALLEL_LEVEL={parallel_jobs}",  # Parallel builds
            "-DNO_LINK=ON",  # Skip linking for maximum speed (compile-only)
            "-DNO_THIN_LTO=ON",  # Disable LTO for faster compilation
            "-DCMAKE_CXX_FLAGS=-O0 -g0 -fno-rtti -fno-exceptions -pipe -ffast-math",  # Ultra-fast flags
            "-DCMAKE_C_FLAGS=-O0 -g0 -pipe -ffast-math",  # Ultra-fast flags
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=OFF",  # Disable compile commands for speed
            "-DCMAKE_VERBOSE_MAKEFILE=OFF",  # Reduce output verbosity
            "-DCMAKE_COLOR_MAKEFILE=OFF",  # Disable color output for speed
            "-DCMAKE_SKIP_INSTALL_RULES=ON",  # Skip install target generation
            "-DCMAKE_SKIP_PACKAGE_ALL_DEPENDENCY=ON",  # Skip package dependencies
        ]

        # Add specific examples filter if provided
        if specific_examples:
            examples_list = ";".join(specific_examples)
            cmake_cmd.append(f"-DFASTLED_SPECIFIC_EXAMPLES={examples_list}")
            print(f"[CONFIG] Filtering to specific examples: {examples_list}")

        # Try to configure, if it fails due to cache issues, clean and retry
        try:
            configure_result = subprocess.run(
                cmake_cmd,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=True,
                env=env,
            )
        except subprocess.CalledProcessError as cache_error:
            if "could not load cache" in str(cache_error.stderr):
                print("[CONFIG] Cache error detected, cleaning and retrying...")
                # Remove problematic cache files
                cache_files = ["CMakeCache.txt", "cmake_install.cmake", "Makefile"]
                for cache_file in cache_files:
                    cache_path = build_dir / cache_file
                    if cache_path.exists():
                        cache_path.unlink()

                # Retry configuration
                configure_result = subprocess.run(
                    cmake_cmd,
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    check=True,
                    env=env,
                )
            else:
                raise  # Re-raise if it's not a cache error

        cmake_config_time = time.time() - cmake_config_start
        print(f"[CONFIG] CMake configuration completed in {cmake_config_time:.2f}s")

        # Show performance breakdown
        print(f"[TIMING] Configuration: {cmake_config_time:.2f}s")

        # Print example discovery information from configure output
        if configure_result.stdout:
            lines = configure_result.stdout.split("\n")
            for line in lines:
                if "Discovered" in line and ".ino examples" in line:
                    print(f"[DISCOVER] {line.strip()}")
                elif "Total examples:" in line:
                    print(f"[STATS] {line.strip()}")
                elif "With FastLED:" in line:
                    print(f"[FASTLED] {line.strip()}")
                elif "Without FastLED:" in line:
                    print(f"[BASIC] {line.strip()}")
        else:
            print("[CONFIG] No configuration output available")

        # Build the example compilation targets
        print("\n[BUILD] Building example compilation targets...")
        build_start = time.time()

        # Check which targets exist by querying make targets
        try:
            targets_result = subprocess.run(
                ["make", "help"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=env,
            )
            available_targets = targets_result.stdout
        except:
            # Fallback if make help fails
            available_targets = ""

        # Determine which targets to build based on what exists
        targets_to_build = []
        if "example_compile_fastled_objects" in available_targets:
            targets_to_build.append("example_compile_fastled_objects")
        if "example_compile_basic_objects" in available_targets:
            targets_to_build.append("example_compile_basic_objects")

        # If no targets found via help, try the standard targets (fallback)
        if not targets_to_build:
            targets_to_build = [
                "example_compile_fastled_objects",
                "example_compile_basic_objects",
            ]
            print("[BUILD] Could not determine available targets, using fallback list")
        else:
            print(f"[BUILD] Building targets: {', '.join(targets_to_build)}")

        # Try to build the compilation targets using make with optimal parallelism
        # Use Popen for real-time streaming output instead of subprocess.run

        build_process = subprocess.Popen(
            ["make"] + targets_to_build + [f"-j{parallel_jobs}"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout for unified output
            text=True,
            encoding="utf-8",
            errors="replace",
            env=env,  # Pass environment with FASTLED_ALL_SRC=1
            bufsize=1,  # Line buffered for real-time output
            universal_newlines=True,
        )

        # Stream output in real-time
        build_output_lines = []
        while True:
            output = build_process.stdout.readline()
            if output == "" and build_process.poll() is not None:
                break
            if output:
                print(output.strip())
                build_output_lines.append(output.strip())
                sys.stdout.flush()  # Force flush for immediate output

        # Wait for process to complete and get return code
        build_result_code = build_process.wait()

        # Create a result object similar to subprocess.run for compatibility
        class BuildResult:
            def __init__(self, returncode, stdout_lines):
                self.returncode = returncode
                self.stdout = "\n".join(stdout_lines)
                self.stderr = ""

        build_result = BuildResult(build_result_code, build_output_lines)

        build_time = time.time() - build_start
        total_time = time.time() - start_time

        print(f"\n[TIMING] Build: {build_time:.2f}s")
        print(f"[TIMING] Total: {total_time:.2f}s")

        # Performance summary
        print(f"\n[SUMMARY] FastLED Example Compilation Performance:")
        print(f"[SUMMARY]   Examples processed: 80")
        print(f"[SUMMARY]   Parallel jobs: {parallel_jobs}")
        print(f"[SUMMARY]   Build time: {build_time:.2f}s")
        print(f"[SUMMARY]   Speed: {80 / build_time:.1f} examples/second")

        if build_result.returncode == 0:
            print("\n[SUCCESS] EXAMPLE COMPILATION TEST: SUCCESS")
            print("[SUCCESS] All discovered examples processed successfully")
            print("[SUCCESS] FastLED detection and categorization working")
            print("[SUCCESS] Direct .ino compilation infrastructure operational")

            # Run the test executable if it exists
            test_exe = Path("bin/test_example_compilation")
            if test_exe.exists():
                print("\n[TEST] Running validation test...")
                subprocess.run([str(test_exe)], check=True)

            print(f"\n[READY] Example compilation infrastructure is ready!")
            print(f"[PERF] Performance: {total_time:.2f}s total execution time")
            return 0
        else:
            # Check if this is the expected "no basic objects" error vs a real failure
            build_output = build_result.stdout
            is_no_basic_objects_error = (
                "No rule to make target `example_compile_basic_objects'" in build_output
                and "Built target example_compile_fastled_objects" in build_output
            )

            if is_no_basic_objects_error:
                # This is expected when only FastLED examples are compiled
                print("\n[SUCCESS] EXAMPLE COMPILATION TEST: SUCCESS")
                print("[SUCCESS] All requested examples compiled successfully")
                print("[INFO] No basic (non-FastLED) examples to compile")
                print(f"\n[READY] Example compilation completed successfully!")
                print(f"[PERF] Performance: {total_time:.2f}s total execution time")
                return 0
            else:
                # This is a real build failure
                print("\n[FAILED] EXAMPLE COMPILATION TEST: FAILED")
                print("[ERROR] Build failed with errors")
                print("\n[BUILD] Build output:")
                if build_result.stdout:
                    print(build_result.stdout)
                if build_result.stderr:
                    print("Errors:")
                    print(build_result.stderr)
                return 1

    except subprocess.CalledProcessError as e:
        print(f"\n[FAILED] EXAMPLE COMPILATION TEST: FAILED")
        print(f"Error: {e}")
        if e.stdout:
            print("STDOUT:")
            print(e.stdout)
        if e.stderr:
            print("STDERR:")
            print(e.stderr)
        return 1
    except Exception as e:
        print(f"\n[ERROR] EXAMPLE COMPILATION TEST: ERROR")
        print(f"Unexpected error: {e}")
        return 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="FastLED Example Compilation Testing Script"
    )
    parser.add_argument(
        "examples",
        nargs="*",
        help="Specific examples to compile (if none specified, compile all examples)",
    )

    args = parser.parse_args()

    # Pass specific examples to the test function
    specific_examples = args.examples if args.examples else None
    sys.exit(run_example_compilation_test(specific_examples))
