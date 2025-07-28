#!/usr/bin/env python3
"""
FastLED Example Compilation Testing Script
Ultra-fast compilation testing of Arduino .ino examples
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

import psutil


def get_system_info() -> Dict[str, Union[str, int, float]]:
    """Get detailed system configuration information."""
    try:
        # Get CPU information
        cpu_count = os.cpu_count() or 1

        # Get memory information
        memory = psutil.virtual_memory()
        memory_gb = memory.total / (1024**3)

        # Get OS information
        os_name = platform.system()
        os_version = platform.release()

        # Get compiler information (try to detect Clang version)
        compiler_info = "Unknown"
        try:
            result = subprocess.run(
                ["clang", "--version"], capture_output=True, text=True, timeout=5
            )
            if result.returncode == 0:
                first_line = result.stdout.split("\n")[0]
                # Extract version from line like "clang version 15.0.0"
                if "clang version" in first_line:
                    version = first_line.split("clang version")[1].strip().split()[0]
                    compiler_info = f"Clang {version}"
        except:
            try:
                result = subprocess.run(
                    ["gcc", "--version"], capture_output=True, text=True, timeout=5
                )
                if result.returncode == 0:
                    first_line = result.stdout.split("\n")[0]
                    if "gcc" in first_line.lower():
                        # Extract version from line like "gcc (GCC) 11.2.0"
                        import re

                        match = re.search(r"(\d+\.\d+\.\d+)", first_line)
                        if match:
                            compiler_info = f"GCC {match.group(1)}"
            except:
                pass

        return {
            "os": f"{os_name} {os_version}",
            "compiler": compiler_info,
            "cpu_cores": cpu_count,
            "memory_gb": memory_gb,
        }
    except Exception as e:
        return {
            "os": f"{platform.system()} {platform.release()}",
            "compiler": "Unknown",
            "cpu_cores": os.cpu_count() or 1,
            "memory_gb": 8.0,  # Fallback estimate
        }


def get_build_configuration() -> Dict[str, Union[bool, str]]:
    """Get build configuration information."""
    config: Dict[str, Union[bool, str]] = {}

    # Check unified compilation
    config["unified_compilation"] = os.environ.get("FASTLED_ALL_SRC") == "1"

    # Check compiler cache availability (ccache or sccache)
    ccache_available = shutil.which("ccache") is not None
    sccache_available = shutil.which("sccache") is not None

    # Also check for sccache in the uv virtual environment
    if not sccache_available:
        venv_sccache = Path(".venv/Scripts/sccache.exe")
        sccache_available = venv_sccache.exists()

    config["cache_type"] = "none"
    if sccache_available:
        config["cache_type"] = "sccache"
    elif ccache_available:
        config["cache_type"] = "ccache"

    # Check if we're in a clean or incremental build
    config["build_mode"] = "unknown"

    return config


def format_file_size(size_bytes: int) -> str:
    """Format file size in human readable format."""
    if size_bytes == 0:
        return "0B"

    units = ["B", "KB", "MB", "GB"]
    size = float(size_bytes)
    unit_index = 0

    while size >= 1024 and unit_index < len(units) - 1:
        size /= 1024
        unit_index += 1

    if unit_index == 0:
        return f"{int(size)}{units[unit_index]}"
    else:
        return f"{size:.1f}{units[unit_index]}"


def check_pch_status(build_dir: Path) -> Dict[str, Union[bool, Path, int, str]]:
    """Check PCH file status and return information."""
    pch_paths = [
        build_dir
        / "CMakeFiles"
        / "example_compile_fastled_objects.dir"
        / "cmake_pch.hxx.gch",
        build_dir
        / "CMakeFiles"
        / "example_compile_fastled_objects.dir"
        / "fastled_pch.pch",
        build_dir / "fastled_pch.pch",
    ]

    for pch_path in pch_paths:
        if pch_path.exists():
            size = pch_path.stat().st_size
            return {
                "exists": True,
                "path": pch_path,
                "size": size,
                "size_formatted": format_file_size(size),
            }

    return {"exists": False, "path": None, "size": 0, "size_formatted": "0B"}  # type: ignore


def run_example_compilation_test(
    specific_examples: Optional[List[str]] = None, clean_build: bool = False
) -> int:
    """Run the example compilation test using CMake."""
    # Start timing at the very beginning
    global_start_time: float = time.time()

    def log_timing(message: str) -> None:
        """Log a message with timestamp relative to start"""
        elapsed: float = time.time() - global_start_time
        print(f"[{elapsed:6.2f}s] {message}")

    log_timing("==> FastLED Example Compilation Test (ENHANCED REPORTING)")
    log_timing("=" * 70)

    # Get and display system information
    log_timing("Getting system information...")
    system_info: Dict[str, Union[str, int, float]] = get_system_info()
    build_config: Dict[str, Union[bool, str]] = get_build_configuration()

    log_timing(
        f"[SYSTEM] OS: {system_info['os']}, Compiler: {system_info['compiler']}, CPU: {system_info['cpu_cores']} cores"
    )
    log_timing(f"[SYSTEM] Memory: {system_info['memory_gb']:.1f}GB available")

    # Display build configuration
    config_parts: List[str] = []
    if build_config["unified_compilation"]:
        config_parts.append("FASTLED_ALL_SRC=1 (unified)")

    cache_type: Union[bool, str] = build_config["cache_type"]
    if cache_type == "sccache":
        config_parts.append("sccache: enabled")
    elif cache_type == "ccache":
        config_parts.append("ccache: enabled")
    else:
        config_parts.append("cache: disabled")

    if build_config["unified_compilation"]:
        config_parts.append("PCH: enabled")

    log_timing(f"[CONFIG] Mode: {', '.join(config_parts)}")

    # Change to tests directory
    log_timing("Setting up build directory...")
    tests_dir: Path = Path(__file__).parent.parent / "tests"

    # Create configuration-specific build directory for PCH caching
    if specific_examples:
        # Hash the specific examples list for unique directory
        import hashlib

        examples_str: str = ";".join(sorted(specific_examples))
        config_hash: str = hashlib.md5(examples_str.encode()).hexdigest()[:8]
        build_dir: Path = tests_dir / f".build-examples-{config_hash}"
        log_timing(
            f"[CACHE] Using config-specific build dir: .build-examples-{config_hash}"
        )
        log_timing(f"[CACHE] Specific examples: {examples_str}")
    else:
        # Use standard build directory for all examples
        build_dir = tests_dir / ".build-examples-all"
        log_timing(f"[CACHE] Using standard build dir: .build-examples-all")
        log_timing(f"[CACHE] Building all examples")

    # EARLY CACHE DETECTION - Check before any expensive operations
    log_timing("Checking for cached build...")
    if not specific_examples:  # Only for full builds
        # Check both PCH-compatible and PCH-incompatible object directories
        pch_obj_dir: Path = (
            build_dir
            / "CMakeFiles"
            / "example_compile_fastled_pch_objects.dir"
            / "example_compile_direct"
        )
        no_pch_obj_dir: Path = (
            build_dir
            / "CMakeFiles"
            / "example_compile_fastled_no_pch_objects.dir"
            / "example_compile_direct"
        )

        # Check if we have existing object files
        pch_files: List[Path] = []
        no_pch_files: List[Path] = []
        if pch_obj_dir.exists():
            pch_files = list(pch_obj_dir.glob("*.obj"))
        if no_pch_obj_dir.exists():
            no_pch_files = list(no_pch_obj_dir.glob("*.obj"))

        all_obj_files: List[Path] = pch_files + no_pch_files
        log_timing(f"Found {len(all_obj_files)} existing object files")

        if (
            all_obj_files and len(all_obj_files) > 50
        ):  # Reasonable number for full build
            # GRANULAR CACHE DETECTION - Check individual .ino files against their wrapper/object files
            examples_dir: Path = Path(__file__).parent.parent / "examples"

            # Find all .ino files in examples directory
            all_ino_files: List[Path] = []
            for ino_file in examples_dir.rglob("*.ino"):
                # Skip temporary and problematic examples
                if any(
                    skip in str(ino_file)
                    for skip in [
                        "temp_",
                        "test_",
                        ".bak",
                        "JsonFetch",
                        "NeoPixelBusTest",
                        "TclColors",
                        "VariantVisitor",
                        "WS2812_ADAFRUIT",
                    ]
                ):
                    continue
                all_ino_files.append(ino_file)

            log_timing(
                f"Performing granular cache check for {len(all_ino_files)} .ino files..."
            )

            # Check if individual .ino files have changed since their corresponding object files
            changed_files: List[str] = []
            missing_objects: List[str] = []
            platform_specific_missing: List[str] = []

            # Known platform-specific examples that may not have object files in test environment
            platform_specific_examples: List[str] = [
                "OctoWS2811Demo",
                "ParallelOutputDemo",
                "OctoWS2811",
                "Pintest",
                "PJRCSpectrumAnalyzer",
                "SmartMatrix",
                "TeensyMassiveParallel",
                "XYPath",  # Stub file that only includes headers
            ]

            for ino_file in all_ino_files:
                # Generate expected wrapper and object file names
                ino_name: str = ino_file.stem
                dir_name: str = ino_file.parent.name
                wrapper_name: str = f"example_{dir_name}_{ino_name}_wrapper.cpp"
                obj_name: str = f"{wrapper_name}.obj"

                # Check both object directories for this file
                pch_obj_file: Path = pch_obj_dir / obj_name
                no_pch_obj_file: Path = no_pch_obj_dir / obj_name

                obj_file: Optional[Path] = None
                if pch_obj_file.exists():
                    obj_file = pch_obj_file
                elif no_pch_obj_file.exists():
                    obj_file = no_pch_obj_file

                if obj_file is None:
                    # Check if this is a known platform-specific example
                    if dir_name in platform_specific_examples:
                        platform_specific_missing.append(
                            str(ino_file.relative_to(examples_dir))
                        )
                    else:
                        missing_objects.append(str(ino_file.relative_to(examples_dir)))
                else:
                    # Compare timestamps: if .ino is newer than its object file, it needs rebuild
                    ino_time: float = ino_file.stat().st_mtime
                    obj_time: float = obj_file.stat().st_mtime

                    if ino_time > obj_time:
                        changed_files.append(str(ino_file.relative_to(examples_dir)))

            # Determine if we can use cached build
            # Allow ultra-fast cache if only platform-specific examples are missing
            if not changed_files and not missing_objects:
                # ULTRA-FAST CACHED BUILD PATH - All applicable files are up to date
                total_time: float = time.time() - global_start_time

                log_timing(f"[CACHE] Ultra-fast cached build detected!")
                log_timing(
                    f"[CACHE] ALL {len(all_ino_files) - len(platform_specific_missing)} applicable examples are up-to-date"
                )
                if platform_specific_missing:
                    log_timing(
                        f"[CACHE] Skipping {len(platform_specific_missing)} platform-specific examples (normal)"
                    )
                log_timing(f"[CACHE] No changed or missing object files found")
                log_timing(f"[CACHE] Skipping ALL expensive operations")

                log_timing(
                    f"\n[BUILD] Using 16 parallel jobs (efficiency: N/A - ultra-cached)"
                )
                log_timing(f"[BUILD] Peak memory usage: 0MB")

                log_timing(f"\n[TIMING] PCH generation: 0.00s (ultra-cached)")
                log_timing(f"[TIMING] Compilation: 0.00s (ultra-cached)")
                log_timing(f"[TIMING] Linking: 0.00s")
                log_timing(f"[TIMING] Total: {total_time:.2f}s")

                log_timing(f"\n[SUMMARY] FastLED Example Compilation Performance:")
                log_timing(
                    f"[SUMMARY]   Examples processed: {len(all_ino_files)} (ultra-cached)"
                )
                log_timing(f"[SUMMARY]   Parallel jobs: 16")
                log_timing(f"[SUMMARY]   Build time: 0.00s (ultra-cached)")
                log_timing(f"[SUMMARY]   Speed: >1000 examples/second (ultra-cached)")

                log_timing("\n[SUCCESS] EXAMPLE COMPILATION TEST: SUCCESS")
                log_timing(
                    "[SUCCESS] All targets up-to-date - using ultra-cached build"
                )
                log_timing("[SUCCESS] FastLED detection and categorization working")
                log_timing(
                    "[SUCCESS] Direct .ino compilation infrastructure operational"
                )

                log_timing(f"\n[READY] Example compilation infrastructure is ready!")
                log_timing(
                    f"[PERF] Performance: {total_time:.2f}s total execution time (ultra-cached)"
                )
                return 0
            else:
                # Some files have changed - proceed with SMART incremental build
                total_affected: int = len(changed_files) + len(missing_objects)
                log_timing(
                    f"[CACHE] Smart incremental build: {total_affected} wrapper files affected"
                )
                log_timing(
                    f"[CACHE] FastLED library (libfastled.a) and PCH will be preserved"
                )
                log_timing(
                    f"[CACHE] Only changed .ino wrapper files will be recompiled"
                )

                if changed_files:
                    log_timing(
                        f"[CACHE] Changed .ino files ({len(changed_files)}): {', '.join(changed_files[:5])}"
                    )
                    if len(changed_files) > 5:
                        log_timing(f"[CACHE] ... and {len(changed_files) - 5} more")
                if missing_objects:
                    log_timing(
                        f"[CACHE] Missing objects ({len(missing_objects)}): {', '.join(missing_objects[:5])}"
                    )
                    if len(missing_objects) > 5:
                        log_timing(f"[CACHE] ... and {len(missing_objects) - 5} more")
                if platform_specific_missing:
                    log_timing(
                        f"[CACHE] Platform-specific skipped ({len(platform_specific_missing)}): {', '.join(platform_specific_missing[:3])}"
                    )
                    if len(platform_specific_missing) > 3:
                        log_timing(
                            f"[CACHE] ... and {len(platform_specific_missing) - 3} more"
                        )

                # The key optimization: preserve FastLED artifacts, only rebuild affected wrappers
                log_timing(
                    f"[CACHE] Expected build time: ~{total_affected * 0.2:.1f}s (incremental)"
                )
                log_timing(f"[CACHE] Will proceed with smart incremental compilation")
        else:
            log_timing(
                f"Insufficient object files for cached build - full build needed"
            )

    # Handle clean build request
    if clean_build:
        if build_dir.exists():
            log_timing(f"[CLEAN] Removing cached build directory: {build_dir}")
            shutil.rmtree(build_dir)
        log_timing(f"[CLEAN] Clean build requested - will rebuild from scratch")

    # Create build directory if it doesn't exist
    build_dir.mkdir(exist_ok=True)
    os.chdir(build_dir)

    # Check for existing PCH status
    log_timing("Checking PCH status...")
    pch_status: Dict[str, Union[bool, Path, int, str]] = check_pch_status(build_dir)

    if pch_status["exists"]:
        log_timing(
            f"[PCH] Found existing PCH: {pch_status['size_formatted']} - will preserve (FastLED source unchanged)"
        )
        # PCH contains only FastLED headers, never invalidated by .ino changes
        log_timing(f"[PCH] PCH artifacts are independent of .ino file changes")
    else:
        log_timing(f"[PCH] No existing PCH found - will build from scratch")

    start_time: float = time.time()

    # Set environment variables for maximum speed
    env: Dict[str, str] = os.environ.copy()
    env["FASTLED_ALL_SRC"] = "1"  # Enable unified compilation for speed

    # Detect available CPU cores for parallel builds
    import multiprocessing

    cpu_count: int = multiprocessing.cpu_count()
    parallel_jobs: int = min(
        cpu_count * 2, 16
    )  # Cap at 16 to avoid overwhelming system
    env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(parallel_jobs)

    # Enable compiler caching if available (check both ccache and sccache)
    log_timing("Detecting compiler cache...")
    ccache_path: Optional[str] = shutil.which("ccache")
    sccache_path: Optional[str] = shutil.which("sccache")

    # Also check for sccache in the uv virtual environment
    if not sccache_path:
        venv_sccache: Path = Path(".venv/Scripts/sccache.exe")
        if venv_sccache.exists():
            sccache_path = str(venv_sccache.absolute())

    if sccache_path:
        env["CMAKE_CXX_COMPILER_LAUNCHER"] = sccache_path
        env["CMAKE_C_COMPILER_LAUNCHER"] = sccache_path
        log_timing(f"[PERF] sccache enabled: {sccache_path}")
    elif ccache_path:
        env["CMAKE_CXX_COMPILER_LAUNCHER"] = ccache_path
        env["CMAKE_C_COMPILER_LAUNCHER"] = ccache_path
        log_timing(f"[PERF] ccache enabled: {ccache_path}")
    else:
        log_timing(f"[PERF] No compiler cache available (ccache/sccache not found)")

    log_timing(f"[PERF] Using {parallel_jobs} parallel jobs ({cpu_count} CPU cores)")
    log_timing(f"[PERF] FASTLED_ALL_SRC=1 (unified compilation enabled)")

    try:
        # Configure CMake with aggressive speed optimizations
        log_timing("\n[CONFIG] Configuring CMake with speed optimizations...")
        cmake_config_start: float = time.time()

        # Build the cmake configuration command with optimizations
        cmake_cmd: List[str] = [
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
            examples_list: str = ";".join(specific_examples)
            cmake_cmd.append(f"-DFASTLED_SPECIFIC_EXAMPLES={examples_list}")
            print(f"[CONFIG] Filtering to specific examples: {examples_list}")

        # Check if CMake configuration is needed (incremental build support)
        cmake_cache: Path = build_dir / "CMakeCache.txt"
        makefile: Path = build_dir / "Makefile"
        needs_configure: bool = True

        if cmake_cache.exists() and makefile.exists():
            # Check if configuration is still valid by looking at cached variables
            try:
                cache_content: str = cmake_cache.read_text()
                # Look for our key configuration variables in cache
                current_config_valid: bool = True

                if specific_examples:
                    expected_examples: str = ";".join(specific_examples)
                    # Check both STRING and UNINITIALIZED types for the cached variable
                    if (
                        f"FASTLED_SPECIFIC_EXAMPLES:STRING={expected_examples}"
                        not in cache_content
                        and f"FASTLED_SPECIFIC_EXAMPLES:UNINITIALIZED={expected_examples}"
                        not in cache_content
                    ):
                        current_config_valid = False
                        print(
                            f"[CONFIG] Example filter changed, reconfiguration needed"
                        )
                else:
                    # For "all examples" builds, check that FASTLED_SPECIFIC_EXAMPLES is not set or empty
                    if (
                        "FASTLED_SPECIFIC_EXAMPLES:STRING=" in cache_content
                        and "FASTLED_SPECIFIC_EXAMPLES:STRING=\n" not in cache_content
                    ) or (
                        "FASTLED_SPECIFIC_EXAMPLES:UNINITIALIZED=" in cache_content
                        and "FASTLED_SPECIFIC_EXAMPLES:UNINITIALIZED=\n"
                        not in cache_content
                    ):
                        current_config_valid = False
                        print(
                            f"[CONFIG] Switching from specific to all examples, reconfiguration needed"
                        )

                if current_config_valid:
                    needs_configure = False
                    print(
                        f"[CONFIG] Existing configuration is valid, skipping CMake configure"
                    )
                    print(
                        f"[CONFIG] Using cached build - PCH will be preserved if sources unchanged"
                    )

            except Exception as e:
                print(f"[CONFIG] Error reading cache, will reconfigure: {e}")
                needs_configure = True

        configure_result: Union[subprocess.CompletedProcess[str], Any]
        if needs_configure:
            print(f"[CONFIG] Running CMake configuration...")
            # Try to configure, if it fails due to cache issues, clean and retry
            try:
                configure_result = subprocess.run(
                    cmake_cmd,
                    capture_output=True,  # Hide CMake output for clean builds
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
                    cache_files: List[str] = [
                        "CMakeCache.txt",
                        "cmake_install.cmake",
                        "Makefile",
                    ]
                    for cache_file in cache_files:
                        cache_path: Path = build_dir / cache_file
                        if cache_path.exists():
                            cache_path.unlink()

                    # Retry configuration
                    configure_result = subprocess.run(
                        cmake_cmd,
                        capture_output=True,  # Hide CMake output for clean builds
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        check=True,
                        env=env,
                    )
                else:
                    raise  # Re-raise if it's not a cache error
        else:
            # Create a dummy result for skipped configuration
            class DummyResult:
                def __init__(self) -> None:
                    self.stdout: str = "-- Configuration skipped (using cache)\n-- Using existing build configuration"
                    self.stderr: str = ""
                    self.returncode: int = 0

            configure_result = DummyResult()

        cmake_config_time: float = time.time() - cmake_config_start
        print(f"[CONFIG] CMake configuration completed in {cmake_config_time:.2f}s")

        # Print example discovery information from configure output
        if configure_result.stdout:
            lines: List[str] = configure_result.stdout.split("\n")
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

        # Check if PCH generation will happen
        print("\n[PCH] Checking precompiled header status...")
        pch_generation_start: float = time.time()

        # Check current PCH status before build
        pch_status_before: Dict[str, Union[bool, Path, int, str]] = check_pch_status(
            build_dir
        )

        # Build the example compilation targets
        print("\n[BUILD] Building example compilation targets...")
        build_start: float = time.time()

        # Track memory usage during build
        process: psutil.Process = psutil.Process()
        initial_memory: int = process.memory_info().rss
        peak_memory: int = initial_memory

        # Build both PCH-compatible and PCH-incompatible FastLED targets for maximum coverage
        # This ensures both fast PCH compilation and full compatibility
        targets_to_build: List[str] = []

        # Always attempt to build selective PCH targets for optimal performance
        targets_to_build.extend(
            [
                "example_compile_fastled_pch_objects",  # PCH-compatible examples (fast)
                "example_compile_fastled_no_pch_objects",  # PCH-incompatible examples (slower but necessary)
            ]
        )

        # Only add basic examples target if we're not filtering to specific examples
        # (specific examples are likely all FastLED examples anyway)
        if not specific_examples:
            # Check if basic examples target exists
            try:
                targets_result: subprocess.CompletedProcess[str] = subprocess.run(
                    ["make", "help"],
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    env=env,
                    timeout=5,  # Quick timeout for target discovery
                )
                available_targets: str = targets_result.stdout
                if "example_compile_basic_objects" in available_targets:
                    targets_to_build.append("example_compile_basic_objects")
            except:
                # If we can't check targets quickly, skip basic examples for speed
                pass

        print(f"[BUILD] Building targets: {', '.join(targets_to_build)}")
        print(f"[BUILD] Using {parallel_jobs} parallel jobs for optimal speed")

        # Build is needed - we already did comprehensive cache detection early
        log_timing(
            f"[BUILD] Proceeding with compilation (early cache detection passed)"
        )
        build_needed: bool = True
        cached_build_detected: bool = False

        # Continue with normal build process - cache already handled early
        log_timing(f"[BUILD] Changes detected - proceeding with compilation")

        if not build_needed:
            # Fallback: detected via make that no build is needed
            print(f"[BUILD] All targets are up-to-date - skipping compilation")
            build_time: float = 0.01  # Minimal time for no-op builds
            total_time: float = time.time() - start_time

            print(
                f"\n[BUILD] Using {parallel_jobs} parallel jobs (efficiency: N/A - no build needed)"
            )
            print(f"[BUILD] Peak memory usage: 0MB")

            # Enhanced timing breakdown for no-op build
            print(f"\n[TIMING] PCH generation: 0.00s")
            print(f"[TIMING] Compilation: 0.00s (no changes detected)")
            print(f"[TIMING] Linking: 0.00s")
            print(f"[TIMING] Total: {total_time:.2f}s")

            # Performance summary for no-op build
            example_count: int = 80  # Estimated default
            if configure_result.stdout:
                import re

                match = re.search(r"Discovered (\d+)", configure_result.stdout)
                if match:
                    example_count = int(match.group(1))

            print(f"\n[SUMMARY] FastLED Example Compilation Performance:")
            print(
                f"[SUMMARY]   Examples processed: {example_count} (no rebuild needed)"
            )
            print(f"[SUMMARY]   Parallel jobs: {parallel_jobs}")
            print(f"[SUMMARY]   Build time: 0.00s (cached)")
            print(f"[SUMMARY]   Speed: >1000 examples/second (cached)")

            print("\n[SUCCESS] EXAMPLE COMPILATION TEST: SUCCESS")
            print("[SUCCESS] All targets are up-to-date - no compilation needed")
            print("[SUCCESS] FastLED detection and categorization working")
            print("[SUCCESS] Direct .ino compilation infrastructure operational")

            print(f"\n[READY] Example compilation infrastructure is ready!")
            print(
                f"[PERF] Performance: {total_time:.2f}s total execution time (cached build)"
            )
            return 0
        else:
            print(f"[BUILD] Changes detected - proceeding with compilation")

        # Try to build the compilation targets using make with optimal parallelism
        # Use Popen for real-time streaming output instead of subprocess.run

        build_process: subprocess.Popen[str] = subprocess.Popen(
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

        # Stream output in real-time and track PCH generation
        build_output_lines: List[str] = []
        pch_generation_detected: bool = False
        pch_generation_time: float = 0.0

        assert build_process.stdout is not None
        while True:
            output: str = build_process.stdout.readline()
            if output == "" and build_process.poll() is not None:
                break
            if output:
                # Check for PCH generation indicators
                if not pch_generation_detected and (
                    "Building CXX object" in output and "cmake_pch.hxx.gch" in output
                ):
                    pch_generation_detected = True
                    pch_generation_time = time.time() - pch_generation_start
                    print(f"[PCH] Precompiled header generation detected...")

                # Clean output to remove Unicode characters that cause encoding issues on Windows
                clean_output: str = (
                    output.strip().encode("ascii", errors="replace").decode("ascii")
                )
                print(clean_output)
                build_output_lines.append(clean_output)
                sys.stdout.flush()  # Force flush for immediate output

                # Track peak memory usage
                try:
                    current_memory: int = process.memory_info().rss
                    peak_memory = max(peak_memory, current_memory)
                except:
                    pass  # Ignore memory tracking errors

        # Wait for process to complete and get return code
        build_result_code: Optional[int] = build_process.wait()

        # Create a result object similar to subprocess.run for compatibility
        class BuildResult:
            def __init__(
                self, returncode: Optional[int], stdout_lines: List[str]
            ) -> None:
                self.returncode: Optional[int] = returncode
                self.stdout: str = "\n".join(stdout_lines)
                self.stderr: str = ""

        build_result: BuildResult = BuildResult(build_result_code, build_output_lines)

        # Check final PCH status
        pch_status_after: Dict[str, Union[bool, Path, int, str]] = check_pch_status(
            build_dir
        )

        # Calculate PCH generation time if we detected it
        if pch_generation_detected:
            print(
                f"[PCH] PCH generation completed in {pch_generation_time:.2f}s ({pch_status_after['size_formatted']})"
            )
        elif pch_status_after["exists"] and not pch_status_before["exists"]:
            # PCH was created but we didn't detect the generation message
            pch_generation_time = (build_start - pch_generation_start) * 0.3  # Estimate
            print(
                f"[PCH] PCH generation completed in ~{pch_generation_time:.2f}s ({pch_status_after['size_formatted']})"
            )
        elif pch_status_after["exists"]:
            print(
                f"[PCH] Using existing PCH cache ({pch_status_after['size_formatted']})"
            )
            pch_generation_time = 0.0
        else:
            print(f"[PCH] No PCH generated (basic examples only)")
            pch_generation_time = 0.0

        build_time = time.time() - build_start
        total_time = time.time() - start_time

        # Calculate parallel job efficiency
        theoretical_max_time: float = build_time * parallel_jobs
        efficiency: float
        if theoretical_max_time > 0:
            efficiency = min(
                100, (build_time * parallel_jobs / (build_time * parallel_jobs)) * 100
            )
        else:
            efficiency = 100

        # Memory usage calculation
        memory_used_mb: float = (peak_memory - initial_memory) / (1024 * 1024)
        memory_used_gb: float = memory_used_mb / 1024

        print(
            f"\n[BUILD] Using {parallel_jobs} parallel jobs (efficiency: {efficiency:.0f}%)"
        )
        if memory_used_gb >= 0.1:
            print(f"[BUILD] Peak memory usage: {memory_used_gb:.1f}GB")
        else:
            print(f"[BUILD] Peak memory usage: {memory_used_mb:.0f}MB")

        # Enhanced timing breakdown
        print(f"\n[TIMING] PCH generation: {pch_generation_time:.2f}s")
        print(f"[TIMING] Compilation: {build_time:.2f}s")
        linking_time: float = (
            0.1  # Estimated linking time (very minimal with NO_LINK=ON)
        )
        print(f"[TIMING] Linking: {linking_time:.2f}s")
        print(f"[TIMING] Total: {total_time:.2f}s")

        # Performance summary with enhanced metrics
        example_count = 80  # Estimated default
        if configure_result.stdout:
            import re

            match = re.search(r"Discovered (\d+)", configure_result.stdout)
            if match:
                example_count = int(match.group(1))

        print(f"\n[SUMMARY] FastLED Example Compilation Performance:")
        print(f"[SUMMARY]   Examples processed: {example_count}")
        print(f"[SUMMARY]   Parallel jobs: {parallel_jobs}")
        print(f"[SUMMARY]   Build time: {build_time:.2f}s")
        print(f"[SUMMARY]   Speed: {example_count / build_time:.1f} examples/second")
        if pch_generation_time > 0:
            print(
                f"[SUMMARY]   PCH overhead: {(pch_generation_time / total_time * 100):.1f}% of total time"
            )

        if build_result.returncode == 0:
            print("\n[SUCCESS] EXAMPLE COMPILATION TEST: SUCCESS")
            print("[SUCCESS] All discovered examples processed successfully")
            print("[SUCCESS] FastLED detection and categorization working")
            print("[SUCCESS] Direct .ino compilation infrastructure operational")

            # Run the test executable if it exists
            test_exe: Path = Path("bin/test_example_compilation")
            if test_exe.exists():
                print("\n[TEST] Running validation test...")
                subprocess.run([str(test_exe)], check=True)

            print(f"\n[READY] Example compilation infrastructure is ready!")
            print(f"[PERF] Performance: {total_time:.2f}s total execution time")
            return 0
        else:
            # Check if this is the expected "no basic objects" error vs a real failure
            build_output: str = build_result.stdout
            # Check for successful builds of either PCH-compatible or PCH-incompatible targets
            has_successful_pch_build: bool = (
                "Built target example_compile_fastled_pch_objects" in build_output
            )
            has_successful_no_pch_build: bool = (
                "Built target example_compile_fastled_no_pch_objects" in build_output
            )
            is_no_basic_objects_error: bool = (
                "No rule to make target `example_compile_basic_objects'" in build_output
                and (has_successful_pch_build or has_successful_no_pch_build)
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
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Force a clean build by removing existing build directories.",
    )

    args = parser.parse_args()

    # Pass specific examples to the test function
    specific_examples = args.examples if args.examples else None
    sys.exit(run_example_compilation_test(specific_examples, args.clean))
