#!/usr/bin/env python3
"""
FastLED Example Compilation Testing Script
Ultra-fast compilation testing of Arduino .ino examples

ENHANCED with Simple Build System Integration:
- Uses proven Compiler class infrastructure for actual compilation
- Preserves all existing functionality (system info, timing, error handling)
- Adds informative stubs for complex features
- Maintains compatibility with existing command-line interface
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import tomllib
from concurrent.futures import Future, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Union

import psutil
import toml  # type: ignore

# Add the parent directory to Python path for imports
# Import the proven Compiler infrastructure
from ci.compiler.clang_compiler import (
    Compiler,
    CompilerOptions,
    LinkOptions,
    Result,
)


# Color output functions using ANSI escape codes
def red_text(text: str) -> str:
    """Return text in red color."""
    return f"\033[31m{text}\033[0m"


def green_text(text: str) -> str:
    """Return text in green color."""
    return f"\033[32m{text}\033[0m"


def orange_text(text: str) -> str:
    """Return text in orange color."""
    return f"\033[33m{text}\033[0m"


@dataclass
class CompilationResult:
    """Results from compiling a set of examples."""

    successful_count: int
    failed_count: int
    compile_time: float
    failed_examples: List[Dict[str, Any]]
    object_file_map: Optional[Dict[Path, List[Path]]] = (
        None  # For full compilation: ino_file -> [obj_files]
    )


@dataclass
class LinkingResult:
    """Results from linking examples into executable programs."""

    linked_count: int
    failed_count: int


def load_build_flags_toml(toml_path: str) -> Dict[str, Any]:
    """Load and parse build_flags.toml file."""
    try:
        with open(toml_path, "rb") as f:
            return tomllib.load(f)
    except FileNotFoundError:
        print(f"Warning: build_flags.toml not found at {toml_path}")
        return {}
    except Exception as e:
        print(f"Warning: Failed to parse build_flags.toml: {e}")
        return {}


def create_simplified_build_flags(
    original_config: Dict[str, Any], output_path: str
) -> Dict[str, Any]:
    """Create a simplified copy of build_flags.toml with only quick mode flags for stub compilation."""

    # Use the original quick flags from the TOML file
    quick_flags = []
    if "build_modes" in original_config and isinstance(
        original_config["build_modes"], dict
    ):
        build_modes = original_config["build_modes"]
        if "quick" in build_modes and isinstance(build_modes["quick"], dict):
            quick_section = build_modes["quick"]
            if "flags" in quick_section and isinstance(quick_section["flags"], list):
                quick_flags = quick_section["flags"]

    # Create simplified config with only quick mode flags
    simplified = {
        "build_modes": {
            "quick": {
                "flags": quick_flags,
            },
        },
    }

    # Write simplified version to file
    try:
        with open(output_path, "w") as f:
            toml.dump(simplified, f)
        print(f"Created simplified build_flags.toml at {output_path}")
    except ImportError:
        # Fallback to manual TOML writing if toml package not available
        with open(output_path, "w") as f:
            f.write(
                "# Simplified build_flags.toml for stub compilation (quick mode only)\\n\\n"
            )
            f.write("[build_modes.quick]\\n")
            build_modes_section = simplified["build_modes"]
            quick_section = build_modes_section["quick"]
            f.write(f"flags = {quick_section['flags']}\\n")
        print(f"Created simplified build_flags.toml at {output_path} (manual format)")

    return simplified


def extract_compiler_flags_from_toml(config: Dict[str, Any]) -> List[str]:
    """Extract compiler flags from TOML config - only uses [build_modes.quick] flags."""
    flags: List[str] = []

    # Only use flags from [build_modes.quick] section for sketch compilation
    if "build_modes" in config and isinstance(config["build_modes"], dict):
        build_modes = config["build_modes"]
        if "quick" in build_modes and isinstance(build_modes["quick"], dict):
            quick_section = build_modes["quick"]

            if "flags" in quick_section and isinstance(quick_section["flags"], list):
                flags.extend(quick_section["flags"])

    return flags


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
            # Use streaming to prevent buffer overflow
            process = subprocess.Popen(
                ["python", "-m", "ziglang", "c++", "--version"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
                encoding="utf-8",
                errors="replace",
            )

            stdout_lines: list[str] = []
            while True:
                stdout_line = process.stdout.readline() if process.stdout else ""
                if stdout_line:
                    stdout_lines.append(stdout_line.rstrip())
                if process.poll() is not None:
                    remaining_stdout = process.stdout.read() if process.stdout else ""
                    if remaining_stdout:
                        for line in remaining_stdout.splitlines():
                            stdout_lines.append(line.rstrip())
                    break

            if process.returncode == 0 and stdout_lines:
                first_line = stdout_lines[0]
                # Extract version from line like "clang version 15.0.0"
                if "clang version" in first_line:
                    version = first_line.split("clang version")[1].strip().split()[0]
                    compiler_info = f"Clang {version}"
        except:
            try:
                # Use streaming to prevent buffer overflow
                process = subprocess.Popen(
                    ["gcc", "--version"],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    bufsize=1,
                    encoding="utf-8",
                    errors="replace",
                )

                stdout_lines: list[str] = []
                while True:
                    stdout_line = process.stdout.readline() if process.stdout else ""
                    if stdout_line:
                        stdout_lines.append(stdout_line.rstrip())
                    if process.poll() is not None:
                        remaining_stdout = (
                            process.stdout.read() if process.stdout else ""
                        )
                        if remaining_stdout:
                            for line in remaining_stdout.splitlines():
                                stdout_lines.append(line.rstrip())
                        break
                if process.returncode == 0:
                    first_line = stdout_lines[0]
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


def get_sccache_path() -> Optional[str]:
    """Get the full path to sccache executable if available."""
    # First check system PATH
    sccache_path = shutil.which("sccache")
    if sccache_path:
        return sccache_path

    # Check for sccache in the uv virtual environment
    venv_sccache = Path(".venv/Scripts/sccache.exe")
    if venv_sccache.exists():
        return str(venv_sccache.absolute())

    return None


def get_ccache_path() -> Optional[str]:
    """Get the full path to ccache executable if available."""
    return shutil.which("ccache")


def get_build_configuration() -> Dict[str, Union[bool, str]]:
    """Get build configuration information."""
    config: Dict[str, Union[bool, str]] = {}

    # Check unified compilation (note: simple build system uses direct compilation)
    config["unified_compilation"] = False  # Simple build system uses direct compilation

    # Check compiler cache availability (ccache or sccache)
    sccache_path = get_sccache_path()
    ccache_path = get_ccache_path()

    config["cache_type"] = "none"
    if sccache_path:
        config["cache_type"] = "sccache"
        config["cache_path"] = sccache_path
    elif ccache_path:
        config["cache_type"] = "ccache"
        config["cache_path"] = ccache_path

    # Build mode for simple build system
    config["build_mode"] = "simple_direct"

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


# Stub functions for complex features (with informative messages)
def generate_pch_header(source_filename: str) -> None:
    """Stub: PCH generation not needed in simple build system."""
    print(
        f"[INFO] PCH header generation for {source_filename} not implemented - using direct compilation"
    )


def detect_build_cache() -> bool:
    """Stub: Build cache detection not implemented."""
    print("[INFO] Build cache detection not implemented - performing fresh compilation")
    return False


def check_incremental_build() -> bool:
    """Stub: Incremental builds not implemented."""
    print(
        "[INFO] Incremental build detection not implemented - performing full compilation"
    )
    return False


def optimize_parallel_jobs() -> int:
    """Stub: Manual parallel job optimization not needed."""
    print(
        "[INFO] Parallel job optimization handled automatically by ThreadPoolExecutor"
    )
    cpu_count = os.cpu_count() or 1
    return cpu_count * 2  # Return sensible default


def check_pch_status(build_dir: Path) -> Dict[str, Union[bool, Path, int, str]]:
    """Check PCH file status - stub for simple build system."""
    # Simple build system doesn't use PCH, but we maintain the interface
    print("[INFO] PCH status check - simple build system uses direct compilation")
    return {"exists": False, "path": None, "size": 0, "size_formatted": "0B"}  # type: ignore


def create_fastled_compiler(use_pch: bool = True, use_sccache: bool = True) -> Compiler:
    """Create compiler with standard FastLED settings for simple build system."""
    import os
    import tempfile

    # Get absolute paths to ensure they work from any working directory
    current_dir = os.getcwd()
    src_path = os.path.join(current_dir, "src")
    arduino_stub_path = os.path.join(current_dir, "src", "platforms", "stub")

    # Try to load build_flags.toml configuration from stashed copy in ci/ directory
    toml_path = os.path.join(os.path.dirname(__file__), "build_flags.toml")
    build_config = load_build_flags_toml(toml_path)

    # Create simplified copy for stub compilation
    if build_config:
        simplified_toml_path = os.path.join(current_dir, "build_flags_simple.toml")
        simplified_config = create_simplified_build_flags(
            build_config, simplified_toml_path
        )

        # Extract additional compiler flags from TOML
        toml_flags = extract_compiler_flags_from_toml(simplified_config)
        print(f"Loaded {len(toml_flags)} compiler flags from build_flags.toml")
    else:
        toml_flags = []
        print("Using default compiler flags (build_flags.toml not available)")

    # Base compiler settings
    base_args = [
        f"-I{arduino_stub_path}",  # Arduino.h stub for examples (absolute path)
    ]

    # Combine base args with TOML flags
    all_args = base_args + toml_flags

    # PCH output path in temp directory
    pch_output_path = None
    if use_pch:
        pch_output_path = os.path.join(tempfile.gettempdir(), "fastled_pch.hpp.pch")

    # Determine compiler command (with or without cache)
    compiler_cmd = "python -m ziglang c++"
    cache_args = []
    print("Using direct compilation with ziglang c++")

    # Combine cache args with other args (cache args go first)
    final_args = cache_args + all_args

    settings = CompilerOptions(
        include_path=src_path,
        defines=[
            "STUB_PLATFORM",
            "ARDUINO=10808",
            "FASTLED_USE_STUB_ARDUINO",
            "SKETCH_HAS_LOTS_OF_MEMORY=1",  # Enable memory-intensive features for STUB platform
            "FASTLED_HAS_ENGINE_EVENTS=1",  # Enable EngineEvents for UI components
        ],  # Define ARDUINO to enable Arduino.h include
        std_version="c++14",
        compiler=compiler_cmd,
        compiler_args=final_args,
        use_pch=use_pch,
        pch_output_path=pch_output_path,
    )
    return Compiler(settings)


def compile_examples_simple(
    compiler: Compiler,
    ino_files: List[Path],
    pch_compatible_files: set[Path],
    log_timing: Callable[[str], None],
    full_compilation: bool = False,
) -> CompilationResult:
    """
    Compile examples using the simple build system (Compiler class).

    Args:
        compiler: The Compiler instance to use
        ino_files: List of .ino files to compile
        pch_compatible_files: Set of files that are PCH compatible
        log_timing: Logging function
        full_compilation: If True, preserve object files for linking; if False, use temp files

    Returns:
        CompilationResult: Results from compilation including counts and failed examples
    """
    compile_start = time.time()
    results: List[Dict[str, Any]] = []

    # Create build directory structure if full compilation is enabled
    build_dir: Optional[Path] = None
    if full_compilation:
        build_dir = Path(".build/examples")
        build_dir.mkdir(parents=True, exist_ok=True)
        log_timing(f"[BUILD] Created build directory: {build_dir}")

    # Submit all compilation jobs with file tracking
    future_to_file: Dict[Future[Result], Path] = {}
    # Track object files for linking (if full_compilation enabled)
    object_file_map: Dict[Path, List[Path]] = {}  # ino_file -> [obj_files]
    pch_files_count = 0
    direct_files_count = 0
    total_cpp_files = 0

    for ino_file in ino_files:
        # Find additional .cpp files in the same directory
        cpp_files = compiler.find_cpp_files_for_example(ino_file)
        if cpp_files:
            total_cpp_files += len(cpp_files)
            log_timing(
                f"[SIMPLE] Found {len(cpp_files)} .cpp file(s) for {ino_file.name}: {[f.name for f in cpp_files]}"
            )

        # Find include directories for this example
        include_dirs = compiler.find_include_dirs_for_example(ino_file)
        if len(include_dirs) > 1:  # More than just the example root directory
            log_timing(
                f"[SIMPLE] Found {len(include_dirs)} include dir(s) for {ino_file.name}: {[Path(d).name for d in include_dirs]}"
            )

        # Build additional flags with include directories
        additional_flags: list[str] = []
        for include_dir in include_dirs:
            additional_flags.append(f"-I{include_dir}")

        # Determine if this file should use PCH
        use_pch_for_file = ino_file in pch_compatible_files

        # Set up object file paths for full compilation
        ino_output_path: Optional[Path] = None
        example_obj_files: List[Path] = []

        if full_compilation and build_dir:
            # Create example-specific build directory
            example_name = ino_file.parent.name
            example_build_dir = build_dir / example_name
            example_build_dir.mkdir(exist_ok=True)

            # Set specific output path for .ino file
            ino_output_path = example_build_dir / f"{ino_file.stem}.o"
            example_obj_files.append(ino_output_path)

        # Compile the .ino file
        future = compiler.compile_ino_file(
            ino_file,
            output_path=ino_output_path,
            use_pch_for_this_file=use_pch_for_file,
            additional_flags=additional_flags,
        )
        future_to_file[future] = ino_file

        if use_pch_for_file:
            pch_files_count += 1
        else:
            direct_files_count += 1

        # Compile additional .cpp files in the same directory
        for cpp_file in cpp_files:
            cpp_output_path: Optional[Path] = None

            if full_compilation and build_dir:
                example_name = ino_file.parent.name
                example_build_dir = build_dir / example_name
                cpp_output_path = example_build_dir / f"{cpp_file.stem}.o"
                example_obj_files.append(cpp_output_path)

            cpp_future = compiler.compile_cpp_file(
                cpp_file,
                output_path=cpp_output_path,
                use_pch_for_this_file=use_pch_for_file,
                additional_flags=additional_flags,
            )
            future_to_file[cpp_future] = cpp_file

            if use_pch_for_file:
                pch_files_count += 1
            else:
                direct_files_count += 1

        # Track object files for this example (for linking)
        if full_compilation and example_obj_files:
            object_file_map[ino_file] = example_obj_files

    total_files = len(ino_files) + total_cpp_files
    log_timing(
        f"[SIMPLE] Submitted {total_files} compilation jobs to ThreadPoolExecutor ({len(ino_files)} .ino + {total_cpp_files} .cpp)"
    )
    log_timing(
        f"[SIMPLE] Using PCH for {pch_files_count} files, direct compilation for {direct_files_count} files"
    )

    # Collect results as they complete with timeout to prevent hanging
    completed_count = 0
    for future in as_completed(
        future_to_file.keys(), timeout=120
    ):  # 2 minute total timeout
        try:
            result: Result = future.result(timeout=30)  # 30 second timeout per file
            source_file: Path = future_to_file[future]
            completed_count += 1
        except Exception as e:
            source_file: Path = future_to_file[future]
            completed_count += 1
            print(f"ERROR: Compilation timed out or failed for {source_file}: {e}")
            result = Result(
                ok=False, stdout="", stderr=f"Compilation timeout: {e}", return_code=-1
            )

        # Only log final completion
        if completed_count == total_files:
            log_timing(
                f"[SIMPLE] Completed {completed_count}/{total_files} compilations"
            )

        file_result: Dict[str, Any] = {
            "file": str(source_file.name),
            "path": str(source_file.relative_to(Path("examples"))),
            "success": bool(result.ok),
            "stderr": str(result.stderr),
        }
        results.append(file_result)

    compile_time = time.time() - compile_start

    # Analyze results
    successful = [r for r in results if r["success"]]
    failed = [r for r in results if not r["success"]]

    log_timing(
        f"[SIMPLE] Compilation completed: {len(successful)} succeeded, {len(failed)} failed"
    )

    # Report failures if any
    if (
        failed and len(failed) <= 10
    ):  # Show full details for reasonable number of failures
        log_timing("[SIMPLE] Failed examples with full error details:")
        for failure in failed[:10]:
            log_timing(f"[SIMPLE] === FAILED: {failure['path']} ===")
            if failure["stderr"]:
                # Show full error message, not just preview
                error_lines = failure["stderr"].strip().split("\n")
                for line in error_lines:
                    log_timing(f"[SIMPLE]   {line}")
            else:
                log_timing(f"[SIMPLE]   No error details available")
            log_timing(f"[SIMPLE] === END: {failure['path']} ===")
    elif failed:
        log_timing(
            f"[SIMPLE] {len(failed)} examples failed (too many to list full details)"
        )
        log_timing("[SIMPLE] First 10 failure summaries:")
        for failure in failed[:10]:
            err = "\n" + failure["stderr"]
            error_preview = err if err.strip() else "No error details"
            log_timing(f"[SIMPLE]   {failure['path']}: {error_preview}...")

    return CompilationResult(
        successful_count=len(successful),
        failed_count=len(failed),
        compile_time=compile_time,
        failed_examples=failed,
        object_file_map=object_file_map if full_compilation else None,
    )


def compile_examples_unity(
    compiler: Compiler,
    ino_files: list[Path],
    log_timing: Callable[[str], None],
    unity_custom_output: Optional[str] = None,
    unity_additional_flags: Optional[List[str]] = None,
) -> CompilationResult:
    """
    Compile FastLED examples using UNITY build approach.

    Creates a single unity.cpp file containing all .ino examples and their
    associated .cpp files, then compiles everything as one unit.
    """
    log_timing("[UNITY] Starting UNITY build compilation...")

    compile_start = time.time()

    # Collect all .cpp files from examples
    all_cpp_files: list[Path] = []

    for ino_file in ino_files:
        # Convert .ino to .cpp (we'll create a temporary .cpp file)
        ino_cpp = ino_file.with_suffix(".cpp")

        # Create temporary .cpp file from .ino
        try:
            ino_content = ino_file.read_text(encoding="utf-8", errors="ignore")
            cpp_content = f"""#include "FastLED.h"
{ino_content}
"""
            # Create temporary .cpp file
            temp_cpp = Path(tempfile.gettempdir()) / f"unity_{ino_file.stem}.cpp"
            temp_cpp.write_text(cpp_content, encoding="utf-8")
            all_cpp_files.append(temp_cpp)

            # Also find any additional .cpp files in the example directory
            example_dir = ino_file.parent
            additional_cpp_files = compiler.find_cpp_files_for_example(ino_file)
            all_cpp_files.extend(additional_cpp_files)

        except Exception as e:
            log_timing(f"[UNITY] ERROR: Failed to process {ino_file.name}: {e}")
            return CompilationResult(
                successful_count=0,
                failed_count=len(ino_files),
                compile_time=0.0,
                failed_examples=[
                    {
                        "file": ino_file.name,
                        "path": str(ino_file.relative_to(Path("examples"))),
                        "success": False,
                        "stderr": f"Failed to prepare for UNITY build: {e}",
                    }
                ],
            )

    if not all_cpp_files:
        log_timing("[UNITY] ERROR: No .cpp files to compile")
        return CompilationResult(
            successful_count=0,
            failed_count=len(ino_files),
            compile_time=0.0,
            failed_examples=[
                {
                    "file": "unity_build",
                    "path": "unity_build",
                    "success": False,
                    "stderr": "No .cpp files found for UNITY build",
                }
            ],
        )

    log_timing(f"[UNITY] Collected {len(all_cpp_files)} .cpp files for UNITY build")

    # Log advanced unity options if used
    if unity_custom_output:
        log_timing(f"[UNITY] Using custom output path: {unity_custom_output}")
    if unity_additional_flags:
        log_timing(
            f"[UNITY] Using additional flags: {' '.join(unity_additional_flags)}"
        )

    # Parse additional flags properly - handle both "flag1 flag2" and ["flag1", "flag2"] formats
    additional_flags = ["-c"]  # Compile only, don't link
    if unity_additional_flags:
        for flag_group in unity_additional_flags:
            # Split space-separated flags into individual flags
            additional_flags.extend(flag_group.split())

    # Create CompilerOptions for unity compilation (reuse the same settings as the main compiler)
    unity_options = CompilerOptions(
        include_path=compiler.settings.include_path,
        compiler=compiler.settings.compiler,
        defines=compiler.settings.defines,
        std_version=compiler.settings.std_version,
        compiler_args=compiler.settings.compiler_args,
        use_pch=False,  # Unity builds don't typically need PCH
        additional_flags=additional_flags,
    )

    try:
        # Perform UNITY compilation - pass unity_output_path as parameter
        unity_future = compiler.compile_unity(
            unity_options, all_cpp_files, unity_custom_output
        )
        unity_result = unity_future.result()

        compile_time = time.time() - compile_start

        # Clean up temporary .cpp files
        for cpp_file in all_cpp_files:
            if cpp_file.name.startswith("unity_") and cpp_file.parent == Path(
                tempfile.gettempdir()
            ):
                try:
                    cpp_file.unlink()
                except:
                    pass  # Ignore cleanup errors

        if unity_result.ok:
            log_timing(
                f"[UNITY] UNITY build completed successfully in {compile_time:.2f}s"
            )
            log_timing(f"[UNITY] All {len(ino_files)} examples compiled as single unit")

            return CompilationResult(
                successful_count=len(ino_files),
                failed_count=0,
                compile_time=compile_time,
                failed_examples=[],
            )
        else:
            log_timing(f"[UNITY] UNITY build failed: {unity_result.stderr}")

            return CompilationResult(
                successful_count=0,
                failed_count=len(ino_files),
                compile_time=compile_time,
                failed_examples=[
                    {
                        "file": "unity_build",
                        "path": "unity_build",
                        "success": False,
                        "stderr": unity_result.stderr,
                    }
                ],
            )

    except Exception as e:
        compile_time = time.time() - compile_start

        # Clean up temporary .cpp files on error
        for cpp_file in all_cpp_files:
            if cpp_file.name.startswith("unity_") and cpp_file.parent == Path(
                tempfile.gettempdir()
            ):
                try:
                    cpp_file.unlink()
                except:
                    pass  # Ignore cleanup errors

        log_timing(f"[UNITY] UNITY build failed with exception: {e}")

        return CompilationResult(
            successful_count=0,
            failed_count=len(ino_files),
            compile_time=compile_time,
            failed_examples=[
                {
                    "file": "unity_build",
                    "path": "unity_build",
                    "success": False,
                    "stderr": f"UNITY build exception: {e}",
                }
            ],
        )


def get_fastled_core_sources() -> List[Path]:
    """Get essential FastLED .cpp files for library creation."""
    src_dir = Path("src")

    # Core FastLED files that must be included
    core_files: List[Path] = [
        src_dir / "FastLED.cpp",
        src_dir / "colorutils.cpp",
        src_dir / "hsv2rgb.cpp",
    ]

    # Find all .cpp files in key directories
    additional_sources: List[Path] = []
    for pattern in ["*.cpp", "lib8tion/*.cpp", "platforms/stub/*.cpp"]:
        additional_sources.extend(list(src_dir.glob(pattern)))

    # Include essential .cpp files from nested directories
    additional_sources.extend(list(src_dir.rglob("*.cpp")))

    # Filter out duplicates and ensure files exist
    all_sources: List[Path] = []
    seen_files: set[Path] = set()

    for cpp_file in core_files + additional_sources:
        # Skip stub_main.cpp since we create individual main.cpp files for each example
        if cpp_file.name == "stub_main.cpp":
            continue

        if cpp_file.exists() and cpp_file not in seen_files:
            all_sources.append(cpp_file)
            seen_files.add(cpp_file)

    return all_sources


def create_fastled_library(
    compiler: Compiler, fastled_build_dir: Path, log_timing: Callable[[str], None]
) -> Path:
    """Create libfastled.a static library."""

    log_timing("[LIBRARY] Creating FastLED static library...")

    # Compile all FastLED sources to object files
    fastled_sources = get_fastled_core_sources()
    fastled_objects: List[Path] = []

    obj_dir = fastled_build_dir / "obj"
    obj_dir.mkdir(exist_ok=True)

    log_timing(f"[LIBRARY] Compiling {len(fastled_sources)} FastLED source files...")

    # Compile each source file
    futures: List[tuple[Future[Result], Path, Path]] = []
    for cpp_file in fastled_sources:
        # Create unique object file name by including relative path to prevent collisions
        # Convert path separators to underscores to create valid filename
        src_dir = Path("src")
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
            result: Result = future.result()
            if result.ok:
                fastled_objects.append(obj_file)
                compiled_count += 1
            else:
                log_timing(
                    f"[LIBRARY] WARNING: Failed to compile {cpp_file.relative_to(Path('src'))}: {result.stderr[:100]}..."
                )
        except Exception as e:
            log_timing(
                f"[LIBRARY] WARNING: Exception compiling {cpp_file.relative_to(Path('src'))}: {e}"
            )

    log_timing(
        f"[LIBRARY] Successfully compiled {compiled_count}/{len(fastled_sources)} FastLED sources"
    )

    if not fastled_objects:
        raise Exception("No FastLED source files compiled successfully")

    # Create static library using ar
    lib_file = fastled_build_dir / "libfastled.a"
    log_timing(f"[LIBRARY] Creating static library: {lib_file}")

    archive_future = compiler.create_archive(fastled_objects, lib_file)
    archive_result = archive_future.result()

    if not archive_result.ok:
        raise Exception(f"Library creation failed: {archive_result.stderr}")

    log_timing(f"[LIBRARY] SUCCESS: FastLED library created: {lib_file}")
    return lib_file


def get_platform_linker_args() -> List[str]:
    """Get platform-specific linker arguments for FastLED executables."""
    import platform

    system = platform.system()
    if system == "Windows":
        # Windows with lld-link - use MSVC-style linking
        return [
            "/SUBSYSTEM:CONSOLE",  # Console application
            "/DEFAULTLIB:libcmt",  # Static C Runtime Library
            "/DEFAULTLIB:libvcruntime",  # VC++ Runtime
            "/DEFAULTLIB:libucrt",  # Universal CRT
            "/NODEFAULTLIB:msvcrt",  # Avoid mixing runtimes
            # Note: Windows FastLED executables use Windows APIs
            # lld-link will automatically link to needed system libraries
        ]
    elif system == "Linux":
        return [
            "-pthread",  # Threading support
            "-lm",  # Math library
            "-ldl",  # Dynamic loading
            "-lrt",  # Real-time extensions
        ]
    elif system == "Darwin":  # macOS
        return [
            "-pthread",  # Threading support
            "-lm",  # Math library
            "-framework",
            "CoreFoundation",
            "-framework",
            "IOKit",
        ]
    else:
        return [
            "-pthread",  # Threading support
            "-lm",  # Math library
        ]


def get_executable_name(example_name: str) -> str:
    """Get platform-appropriate executable name."""
    import platform

    if platform.system() == "Windows":
        return f"{example_name}.exe"
    else:
        return example_name


def create_main_cpp_for_example(example_build_dir: Path) -> Path:
    """Create a minimal main.cpp file that includes the stub_main.hpp."""
    main_cpp_content = """// Auto-generated main.cpp for Arduino sketch
// This file includes the FastLED stub main function

#include "platforms/stub_main.hpp"
"""

    main_cpp_path = example_build_dir / "main.cpp"
    main_cpp_path.write_text(main_cpp_content)
    return main_cpp_path


def link_examples(
    object_file_map: Dict[Path, List[Path]],
    fastled_lib: Path,
    build_dir: Path,
    compiler: Compiler,
    log_timing: Callable[[str], None],
) -> LinkingResult:
    """Link all examples into executable programs."""

    linked_count = 0
    failed_count = 0

    for ino_file, obj_files in object_file_map.items():
        example_name = ino_file.parent.name
        example_build_dir = build_dir / example_name

        # Create executable name
        executable_name = get_executable_name(example_name)
        executable_path = example_build_dir / executable_name

        # Validate that all object files exist
        missing_objects = [obj for obj in obj_files if not obj.exists()]
        if missing_objects:
            log_timing(
                f"[LINKING] FAILED: {executable_name}: Missing object files: {[str(obj) for obj in missing_objects]}"
            )
            failed_count += 1
            continue

        try:
            # Create and compile main.cpp for this example
            main_cpp_path = create_main_cpp_for_example(example_build_dir)
            main_obj_path = example_build_dir / "main.o"

            main_future = compiler.compile_cpp_file(main_cpp_path, main_obj_path)
            main_result: Result = main_future.result()

            if not main_result.ok:
                log_timing(
                    f"[LINKING] FAILED: {executable_name}: Failed to compile main.cpp: {main_result.stderr[:100]}..."
                )
                failed_count += 1
                continue

            # Add main.o to object files for linking
            all_obj_files = obj_files + [main_obj_path]

            # Set up linking options
            link_options = LinkOptions(
                output_executable=str(executable_path),
                object_files=[str(obj) for obj in all_obj_files],
                static_libraries=[str(fastled_lib)],
                linker_args=get_platform_linker_args(),
            )

            # Perform linking
            link_future = compiler.link_program(link_options)
            link_result: Result = link_future.result()

            if link_result.ok:
                linked_count += 1
                log_timing(f"[LINKING] SUCCESS: {executable_name}")
            else:
                failed_count += 1
                log_timing(
                    f"[LINKING] FAILED: {executable_name}: {link_result.stderr[:200]}..."
                )

        except Exception as e:
            failed_count += 1
            log_timing(f"[LINKING] FAILED: {executable_name}: Exception: {e}")

    return LinkingResult(linked_count=linked_count, failed_count=failed_count)


@dataclass
class CompilationTestConfig:
    """Configuration for the compilation test."""
    specific_examples: Optional[List[str]]
    clean_build: bool
    disable_pch: bool
    disable_sccache: bool
    unity_build: bool
    unity_custom_output: Optional[str]
    unity_additional_flags: Optional[List[str]]
    full_compilation: bool


@dataclass 
class CompilationTestResults:
    """Results from compilation test."""
    successful_count: int
    failed_count: int
    failed_examples: List[Dict[str, str]]
    compile_time: float
    linking_time: float
    linked_count: int
    linking_failed_count: int
    object_file_map: Optional[Dict[Path, Path]]


class CompilationTestRunner:
    """Handles the orchestration of FastLED example compilation tests."""
    
    def __init__(self, config: CompilationTestConfig):
        self.config = config
        self.global_start_time = time.time()
        
    def log_timing(self, message: str) -> None:
        """Log a message with timestamp relative to start."""
        elapsed = time.time() - self.global_start_time
        print(f"[{elapsed:6.2f}s] {message}")
    
    def initialize_system(self) -> tuple[Compiler, Dict[str, Union[str, int, float]], Dict[str, Union[bool, str]]]:
        """Initialize the compiler and get system information."""
        self.log_timing("==> FastLED Example Compilation Test (SIMPLE BUILD SYSTEM)")
        self.log_timing("=" * 70)
        
        # Get system information
        self.log_timing("Getting system information...")
        system_info = get_system_info()
        build_config = get_build_configuration()
        
        self.log_timing(
            f"[SYSTEM] OS: {system_info['os']}, Compiler: {system_info['compiler']}, CPU: {system_info['cpu_cores']} cores"
        )
        self.log_timing(f"[SYSTEM] Memory: {system_info['memory_gb']:.1f}GB available")
        
        # Initialize compiler
        self.log_timing("Initializing simple build system...")
        try:
            compiler = create_fastled_compiler(
                use_pch=not self.config.disable_pch,
                use_sccache=not self.config.disable_sccache
            )
            
            # Verify compiler accessibility
            version_result = compiler.check_clang_version()
            if not version_result.success:
                raise RuntimeError(f"Compiler accessibility check failed: {version_result.error}")
                
            self.log_timing(f"[COMPILER] Using {version_result.version}")
            return compiler, system_info, build_config
            
        except Exception as e:
            raise RuntimeError(f"Failed to initialize simple build system: {e}")
    
    def discover_examples(self, compiler: Compiler) -> List[Path]:
        """Discover and validate .ino examples to compile."""
        self.log_timing("Discovering .ino examples...")
        
        try:
            filter_names = self.config.specific_examples if self.config.specific_examples else None
            ino_files = compiler.find_ino_files("examples", filter_names=filter_names)
            
            if not ino_files:
                if self.config.specific_examples:
                    # Show detailed error with suggestions
                    all_ino_files = list(Path("examples").rglob("*.ino"))
                    if all_ino_files:
                        available_names = sorted([f.stem for f in all_ino_files])
                        suggestion = f"\nAvailable examples include: {', '.join(available_names[:10])}..."
                        if len(available_names) > 10:
                            suggestion += f"\n  ... and {len(available_names) - 10} more examples"
                    else:
                        suggestion = ""
                    
                    raise ValueError(f"No .ino files found matching: {self.config.specific_examples}{suggestion}")
                else:
                    raise ValueError("No .ino files found in examples directory")
            
            # Report discovery results
            if self.config.specific_examples:
                self.log_timing(
                    f"[DISCOVER] Found {len(ino_files)} specific examples: {', '.join([f.stem for f in ino_files])}"
                )
            else:
                self.log_timing(
                    f"[DISCOVER] Found {len(ino_files)} total .ino examples in examples/"
                )
                
            return ino_files
            
        except Exception as e:
            raise RuntimeError(f"Failed to discover examples: {e}")
    
    def analyze_pch_compatibility(self, compiler: Compiler, ino_files: List[Path]) -> tuple[set[Path], List[str], List[str]]:
        """Analyze files for PCH compatibility and return configuration."""
        config_parts = ["Simple Build System: enabled"]
        
        if self.config.unity_build:
            config_parts.append("UNITY build: enabled")
        else:
            config_parts.append("Direct .ino compilation: enabled")
        
        # Handle caching configuration  
        if self.config.disable_sccache:
            config_parts.append("cache: disabled (default)")
        else:
            # Add cache info based on build config
            config_parts.append("cache: enabled")
        
        pch_compatible_files: set[Path] = set()
        pch_incompatible_files: List[str] = []
        
        if self.config.unity_build:
            self.log_timing("[UNITY] PCH disabled for UNITY builds (not needed)")
            config_parts.append("PCH: disabled (UNITY mode)")
        elif compiler.settings.use_pch:
            self.log_timing("[PCH] Analyzing examples for PCH compatibility...")
            
            for ino_file in ino_files:
                if compiler.analyze_ino_for_pch_compatibility(ino_file):
                    pch_compatible_files.add(ino_file)
                else:
                    pch_incompatible_files.append(ino_file.name)
            
            if pch_incompatible_files:
                self.log_timing(
                    f"[PCH] Found {len(pch_incompatible_files)} incompatible files (will use direct compilation):"
                )
                for filename in pch_incompatible_files[:5]:  # Show first 5
                    self.log_timing(f"[PCH]   - {filename} (has code before FastLED.h)")
                if len(pch_incompatible_files) > 5:
                    self.log_timing(f"[PCH]   - ... and {len(pch_incompatible_files) - 5} more")
                
                config_parts.append(f"PCH: selective ({len(pch_compatible_files)} files)")
            else:
                self.log_timing(f"[PCH] All {len(ino_files)} files are PCH compatible")
                config_parts.append("PCH: enabled (all files)")
        else:
            self.log_timing("[PCH] PCH disabled globally")
            config_parts.append("PCH: disabled")
        
        return pch_compatible_files, pch_incompatible_files, config_parts
    
    def setup_pch(self, compiler: Compiler, pch_compatible_files: set[Path]) -> bool:
        """Setup precompiled headers if applicable."""
        if (not self.config.unity_build and 
            compiler.settings.use_pch and 
            len(pch_compatible_files) > 0):
            
            pch_start = time.time()
            pch_success = compiler.create_pch_file()
            pch_time = time.time() - pch_start
            
            if pch_success:
                self.log_timing(f"[PCH] Precompiled header created in {pch_time:.2f}s")
                return True
            else:
                self.log_timing(
                    "[PCH] Precompiled header creation failed, using direct compilation for all files"
                )
                pch_compatible_files.clear()
                return False
        elif compiler.settings.use_pch and len(pch_compatible_files) == 0:
            self.log_timing("[PCH] No PCH-compatible files found, skipping PCH creation")
        
        return True
    
    def compile_examples(self, compiler: Compiler, ino_files: List[Path], 
                        pch_compatible_files: set[Path]) -> CompilationTestResults:
        """Execute the compilation process."""
        import multiprocessing
        
        cpu_count = multiprocessing.cpu_count()
        parallel_jobs = min(cpu_count * 2, 16)  # Cap at 16 to avoid overwhelming system
        
        self.log_timing(f"[PERF] Using ThreadPoolExecutor with {parallel_jobs} max workers ({cpu_count} CPU cores)")
        self.log_timing("[PERF] Direct compilation enabled (no CMake overhead)")
        
        self.log_timing(f"\n[BUILD] Starting example compilation...")
        self.log_timing(f"[BUILD] Target examples: {len(ino_files)}")
        self.log_timing(f"[BUILD] Parallel workers: {parallel_jobs}")
        
        start_time = time.time()
        
        try:
            if self.config.unity_build:
                result = compile_examples_unity(
                    compiler,
                    ino_files,
                    self.log_timing,
                    unity_custom_output=self.config.unity_custom_output,
                    unity_additional_flags=self.config.unity_additional_flags,
                )
            else:
                result = compile_examples_simple(
                    compiler, ino_files, pch_compatible_files, self.log_timing, self.config.full_compilation
                )
            
            compile_time = time.time() - start_time
            
            return CompilationTestResults(
                successful_count=result.successful_count,
                failed_count=result.failed_count,
                failed_examples=result.failed_examples,
                compile_time=compile_time,
                linking_time=0.0,
                linked_count=0,
                linking_failed_count=0,
                object_file_map=getattr(result, 'object_file_map', None)
            )
            
        except Exception as e:
            raise RuntimeError(f"Compilation failed: {e}")
    
    def handle_linking(self, compiler: Compiler, results: CompilationTestResults) -> CompilationTestResults:
        """Handle linking phase if full compilation is requested."""
        if not self.config.full_compilation or results.failed_count > 0 or not results.object_file_map:
            if self.config.full_compilation and results.failed_count > 0:
                self.log_timing(f"[LINKING] Skipping linking due to {results.failed_count} compilation failures")
            return results
        
        self.log_timing("\n[LINKING] Starting real program linking...")
        linking_start = time.time()
        
        try:
            # Create FastLED static library
            self.log_timing("[LINKING] Creating FastLED static library...")
            fastled_build_dir = Path(".build/fastled")
            fastled_build_dir.mkdir(parents=True, exist_ok=True)
            
            fastled_lib = create_fastled_library(compiler, fastled_build_dir, self.log_timing)
            
            # Link examples
            self.log_timing("[LINKING] Linking example programs...")
            build_dir = Path(".build/examples")
            linking_result = link_examples(
                results.object_file_map, fastled_lib, build_dir, compiler, self.log_timing
            )
            
            results.linked_count = linking_result.linked_count
            results.linking_failed_count = linking_result.failed_count
            results.linking_time = time.time() - linking_start
            
            if results.linking_failed_count == 0:
                self.log_timing(f"[LINKING] SUCCESS: Successfully linked {results.linked_count} executable programs")
            else:
                self.log_timing(f"[LINKING] WARNING: Linked {results.linked_count} programs, {results.linking_failed_count} failed")
            
            self.log_timing(f"[LINKING] Real linking completed in {results.linking_time:.2f}s")
            
        except Exception as e:
            results.linking_time = time.time() - linking_start
            self.log_timing(f"[LINKING] ERROR: Linking failed: {e}")
            results.linking_failed_count = results.successful_count  # Mark all as failed
            results.linked_count = 0
        
        return results
    
    def report_results(self, ino_files: List[Path], results: CompilationTestResults, 
                      config_parts: List[str]) -> int:
        """Generate the final report and return exit code."""
        import multiprocessing
        
        cpu_count = multiprocessing.cpu_count()
        parallel_jobs = min(cpu_count * 2, 16)
        
        # Calculate efficiency
        if results.compile_time > 0:
            efficiency = min(100, (len(ino_files) / results.compile_time / parallel_jobs) * 100)
        else:
            efficiency = 100
        
        total_time = results.compile_time + results.linking_time
        
        self.log_timing(f"[CONFIG] Mode: {', '.join(config_parts)}")
        self.log_timing(f"\n[BUILD] Using {parallel_jobs} parallel workers (efficiency: {efficiency:.0f}%)")
        
        # Enhanced timing breakdown
        self.log_timing(f"\n[TIMING] Compilation: {results.compile_time:.2f}s")
        linking_mode = "(with program generation)" if self.config.full_compilation and results.failed_count == 0 else "(compile-only mode)"
        self.log_timing(f"[TIMING] Linking: {results.linking_time:.2f}s {linking_mode}")
        self.log_timing(f"[TIMING] Total: {total_time:.2f}s")
        
        # Performance summary
        self.log_timing(f"\n[SUMMARY] FastLED Example Compilation Performance:")
        self.log_timing(f"[SUMMARY]   Examples processed: {len(ino_files)}")
        self.log_timing(f"[SUMMARY]   Successful: {results.successful_count}")
        self.log_timing(f"[SUMMARY]   Failed: {results.failed_count}")
        self.log_timing(f"[SUMMARY]   Parallel workers: {parallel_jobs}")
        self.log_timing(f"[SUMMARY]   Build time: {results.compile_time:.2f}s")
        if results.compile_time > 0:
            self.log_timing(f"[SUMMARY]   Speed: {len(ino_files) / results.compile_time:.1f} examples/second")
        
        # Determine success
        overall_success = results.failed_count == 0
        if self.config.full_compilation:
            overall_success = overall_success and results.linking_failed_count == 0
        
        if overall_success:
            if self.config.full_compilation:
                self.log_timing("\n[SUCCESS] EXAMPLE COMPILATION + LINKING TEST: SUCCESS")
                self.log_timing(f"[SUCCESS] {results.successful_count}/{len(ino_files)} examples compiled and {results.linked_count} linked successfully")
            else:
                self.log_timing("\n[SUCCESS] EXAMPLE COMPILATION TEST: SUCCESS")
                self.log_timing(f"[SUCCESS] {results.successful_count}/{len(ino_files)} examples compiled successfully")
            
            print(green_text("### SUCCESS ###"))
            return 0
        else:
            # Show failed examples
            print(f"\n{red_text('### ERROR ###')}")
            
            if results.failed_count > 0:
                self.log_timing(f"[ERROR] {results.failed_count} compilation failures:")
                for failure in results.failed_examples:
                    path = failure["path"].replace("\\", "/")
                    print(f"  {orange_text(f'examples/{path}')}")
            
            if self.config.full_compilation and results.linking_failed_count > 0:
                self.log_timing(f"[ERROR] {results.linking_failed_count} linking failures detected")
                self.log_timing("[ERROR] Test failed due to linker errors - see linking output above")
            
            return 1


def run_example_compilation_test(
    specific_examples: Optional[List[str]],
    clean_build: bool,
    disable_pch: bool,
    disable_sccache: bool,
    unity_build: bool,
    unity_custom_output: Optional[str],
    unity_additional_flags: Optional[List[str]],
    full_compilation: bool,
) -> int:
    """Run the example compilation test using enhanced simple build system."""
    try:
        # Create configuration
        config = CompilationTestConfig(
            specific_examples=specific_examples,
            clean_build=clean_build,
            disable_pch=disable_pch,
            disable_sccache=disable_sccache,
            unity_build=unity_build,
            unity_custom_output=unity_custom_output,
            unity_additional_flags=unity_additional_flags,
            full_compilation=full_compilation
        )
        
        # Create test runner
        runner = CompilationTestRunner(config)
        
        # Initialize system and compiler
        compiler, system_info, build_config = runner.initialize_system()
        
        # Discover examples to compile
        ino_files = runner.discover_examples(compiler)
        
        # Analyze PCH compatibility and configuration
        pch_compatible_files, pch_incompatible_files, config_parts = runner.analyze_pch_compatibility(compiler, ino_files)
        
        # Setup PCH if needed
        runner.setup_pch(compiler, pch_compatible_files)
        
        # Dump compiler information
        runner.log_timing("\n[COMPILER] Dumping compiler flags...")
        compiler_args = compiler.get_compiler_args()
        runner.log_timing(f"[COMPILER] Full command: {' '.join(compiler_args)} <input_file> -o <output_file>")
        
        # Compile examples
        results = runner.compile_examples(compiler, ino_files, pch_compatible_files)
        
        # Handle linking if requested
        results = runner.handle_linking(compiler, results)
        
        # Generate report and return exit code
        return runner.report_results(ino_files, results, config_parts)
        
    except Exception as e:
        print(f"\n{red_text('### ERROR ###')}")
        print(f"Unexpected error: {e}")
        return 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="FastLED Example Compilation Testing Script (Enhanced with Simple Build System)"
    )
    parser.add_argument(
        "examples",
        nargs="*",
        help="Specific examples to compile (if none specified, compile all examples)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Force a clean build (note: simple build system always performs clean builds).",
    )
    parser.add_argument(
        "--no-pch",
        action="store_true",
        help="Disable precompiled headers (PCH) for compilation - useful for debugging or compatibility issues.",
    )
    parser.add_argument(
        "--cache",
        "--sccache",
        action="store_true",
        help="Enable sccache/ccache for compilation (disabled by default for faster clean builds).",
    )
    parser.add_argument(
        "--unity",
        action="store_true",
        help="Enable UNITY build mode - compile all source files as a single unit for improved performance and optimization.",
    )
    parser.add_argument(
        "--custom-output",
        type=str,
        help="Custom output path for unity.cpp file (only used with --unity)",
    )
    parser.add_argument(
        "--additional-flags",
        nargs="+",
        help="Additional compiler flags to pass to unity build (only used with --unity)",
    )
    parser.add_argument(
        "--full",
        action="store_true",
        help="Enable full compilation mode: compile AND link examples into executable programs",
    )

    args = parser.parse_args()

    # Pass specific examples to the test function
    specific_examples = args.examples if args.examples else None
    sys.exit(
        run_example_compilation_test(
            specific_examples,
            args.clean,
            disable_pch=args.no_pch,
            disable_sccache=not args.cache,
            unity_build=args.unity,
            unity_custom_output=args.custom_output,
            unity_additional_flags=args.additional_flags,
            full_compilation=args.full,
        )
    )
