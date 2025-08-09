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
import concurrent.futures
import hashlib
import os
import platform
import shutil
import stat
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


_IS_GITHUB_ACTIONS = os.getenv("GITHUB_ACTIONS") == "true"

_TIMEOUT = 600 if _IS_GITHUB_ACTIONS else 120

# Add the parent directory to Python path for imports
# Import the proven Compiler infrastructure
from ci.compiler.clang_compiler import (
    BuildFlags,
    Compiler,
    CompilerOptions,
    LinkOptions,
    Result,
)
from ci.util.running_process import EndOfStream, RunningProcess


# Abort threshold to prevent flooding logs with repeated failures
MAX_FAILURES_BEFORE_ABORT = 3


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
    object_file_map: Optional[Dict[Path, List[Path]]]


@dataclass
class LinkingResult:
    """Results from linking examples into executable programs."""

    linked_count: int
    failed_count: int
    cache_hits: int = 0
    cache_misses: int = 0

    @property
    def total_count(self) -> int:
        """Total number of examples processed (linked + failed)"""
        return self.linked_count + self.failed_count

    @property
    def cache_hit_rate(self) -> float:
        """Cache hit rate as percentage"""
        total_processed = self.cache_hits + self.cache_misses
        return (self.cache_hits / total_processed * 100) if total_processed > 0 else 0.0


@dataclass
class ExecutionFailure:
    """Represents a failed example execution with identifying info and reason."""

    name: str
    reason: str
    stdout: str


def load_build_flags_toml(toml_path: str) -> Dict[str, Any]:
    """Load and parse build_example.toml file."""
    try:
        with open(toml_path, "rb") as f:
            config = tomllib.load(f)
            if not config:
                raise RuntimeError(
                    f"build_example.toml at {toml_path} is empty or invalid"
                )
            return config
    except FileNotFoundError:
        raise RuntimeError(
            f"CRITICAL: build_example.toml not found at {toml_path}. This file is required for proper compilation flags."
        )
    except Exception as e:
        raise RuntimeError(
            f"CRITICAL: Failed to parse build_example.toml at {toml_path}: {e}"
        )


def extract_compiler_flags_from_toml(config: Dict[str, Any]) -> List[str]:
    """Extract compiler flags from TOML config - includes universal [all] flags and [build_modes.quick] flags."""
    flags: List[str] = []

    # First, extract universal compiler flags from [all] section
    if "all" in config and isinstance(config["all"], dict):
        all_section: Dict[str, Any] = config["all"]  # type: ignore
        if "compiler_flags" in all_section and isinstance(
            all_section["compiler_flags"], list
        ):
            compiler_flags: List[str] = all_section["compiler_flags"]  # type: ignore
            flags.extend(compiler_flags)
            print(
                f"[CONFIG] Loaded {len(compiler_flags)} universal compiler flags from [all] section"
            )

    # Then, extract flags from [build_modes.quick] section for sketch compilation
    if "build_modes" in config and isinstance(config["build_modes"], dict):
        build_modes: Dict[str, Any] = config["build_modes"]  # type: ignore
        if "quick" in build_modes and isinstance(build_modes["quick"], dict):
            quick_section: Dict[str, Any] = build_modes["quick"]  # type: ignore

            if "flags" in quick_section and isinstance(quick_section["flags"], list):
                quick_flags: List[str] = quick_section["flags"]  # type: ignore
                flags.extend(quick_flags)
                print(
                    f"[CONFIG] Loaded {len(quick_flags)} quick mode flags from [build_modes.quick] section"
                )

    return flags


def extract_stub_platform_defines_from_toml(config: Dict[str, Any]) -> List[str]:
    """Extract stub platform defines from TOML config - uses [stub_platform] section."""
    defines: List[str] = []

    # Extract defines from [stub_platform] section
    if "stub_platform" in config and isinstance(config["stub_platform"], dict):
        stub_section: Dict[str, Any] = config["stub_platform"]  # type: ignore
        if "defines" in stub_section and isinstance(stub_section["defines"], list):
            stub_defines: List[str] = stub_section["defines"]  # type: ignore
            defines.extend(stub_defines)
            print(
                f"[CONFIG] Loaded {len(stub_defines)} stub platform defines from [stub_platform] section"
            )
        else:
            raise RuntimeError(
                "CRITICAL: No 'defines' list found in [stub_platform] section. "
                "This is required for stub platform compilation."
            )
    else:
        raise RuntimeError(
            "CRITICAL: No [stub_platform] section found in build_example.toml. "
            "This section is MANDATORY for stub platform compilation."
        )

    # Validate that all required defines are present
    required_defines = [
        "STUB_PLATFORM",
        "ARDUINO=",  # Partial match for version number
        "FASTLED_USE_STUB_ARDUINO",
        "FASTLED_STUB_IMPL",
    ]

    for required in required_defines:
        if not any(define.startswith(required) for define in defines):
            raise RuntimeError(
                f"CRITICAL: Required stub platform define '{required}' not found in configuration. "
                f"Please ensure [stub_platform] section contains all required defines."
            )

    return defines


def extract_stub_platform_include_paths_from_toml(config: Dict[str, Any]) -> List[str]:
    """Extract stub platform include paths from TOML config - uses [stub_platform] section."""
    include_paths: List[str] = []

    # Extract include paths from [stub_platform] section
    if "stub_platform" in config and isinstance(config["stub_platform"], dict):
        stub_section: Dict[str, Any] = config["stub_platform"]  # type: ignore
        if "include_paths" in stub_section and isinstance(
            stub_section["include_paths"], list
        ):
            stub_include_paths: List[str] = stub_section["include_paths"]  # type: ignore
            include_paths.extend(stub_include_paths)
            print(
                f"[CONFIG] Loaded {len(stub_include_paths)} stub platform include paths from [stub_platform] section"
            )
        else:
            raise RuntimeError(
                "CRITICAL: No 'include_paths' list found in [stub_platform] section. "
                "This is required for stub platform compilation."
            )
    else:
        raise RuntimeError(
            "CRITICAL: No [stub_platform] section found in build_example.toml. "
            "This section is MANDATORY for stub platform compilation."
        )

    return include_paths


def get_system_info() -> Dict[str, Union[str, int, float]]:
    """Get basic system configuration information.

    OPTIMIZED: Removed expensive subprocess calls for compiler detection
    since we know the build system uses Zig/Clang after migration.
    """
    try:
        # Get CPU information
        cpu_count = os.cpu_count() or 1

        # Get memory information
        memory = psutil.virtual_memory()
        memory_gb = memory.total / (1024**3)

        # Get OS information
        os_name = platform.system()
        os_version = platform.release()

        # Use known compiler info since we're post-migration to Zig/Clang
        # No expensive subprocess calls needed
        compiler_info = "Zig/Clang (known)"

        return {
            "os": f"{os_name} {os_version}",
            "compiler": compiler_info,
            "cpu_cores": cpu_count,
            "memory_gb": memory_gb,
        }
    except Exception as e:
        return {
            "os": f"{platform.system()} {platform.release()}",
            "compiler": "Zig/Clang (fallback)",
            "cpu_cores": os.cpu_count() or 1,
            "memory_gb": 8.0,  # Fallback estimate
        }


def get_build_configuration() -> Dict[str, Union[bool, str]]:
    """Get build configuration information."""
    config: Dict[str, Union[bool, str]] = {}

    # Check unified compilation (note: simple build system uses direct compilation)
    config["unified_compilation"] = False  # Simple build system uses direct compilation

    # Compiler cache disabled
    config["cache_type"] = "none"

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


def create_fastled_compiler(use_pch: bool, parallel: bool) -> Compiler:
    """Create compiler with standard FastLED settings for simple build system."""
    import os
    import tempfile

    # Get absolute paths to ensure they work from any working directory
    current_dir = os.getcwd()
    src_path = os.path.join(current_dir, "src")
    arduino_stub_path = os.path.join(current_dir, "src", "platforms", "stub")

    # Load build_example.toml configuration directly from ci/ directory
    toml_path = os.path.join(
        os.path.dirname(os.path.dirname(__file__)), "build_example.toml"
    )
    build_config = load_build_flags_toml(
        toml_path
    )  # Will raise RuntimeError if not found

    # Extract additional compiler flags from TOML (using ci/build_example.toml directly)
    toml_flags = extract_compiler_flags_from_toml(
        build_config
    )  # Will raise RuntimeError if critical flags missing
    print(f"Loaded {len(toml_flags)} total compiler flags from build_example.toml")

    # Extract stub platform defines from TOML configuration
    stub_defines = extract_stub_platform_defines_from_toml(
        build_config
    )  # Will raise RuntimeError if critical defines missing
    print(f"Loaded {len(stub_defines)} stub platform defines from build_example.toml")

    # Extract stub platform include paths from TOML configuration
    stub_include_paths = extract_stub_platform_include_paths_from_toml(
        build_config
    )  # Will raise RuntimeError if critical paths missing
    print(
        f"Loaded {len(stub_include_paths)} stub platform include paths from build_example.toml"
    )

    # Base compiler settings - convert relative paths to absolute
    base_args: list[str] = []
    for include_path in stub_include_paths:
        if os.path.isabs(include_path):
            base_args.append(f"-I{include_path}")
        else:
            # Convert relative path to absolute from project root
            abs_path = os.path.join(current_dir, include_path)
            base_args.append(f"-I{abs_path}")
    print(f"[CONFIG] Added {len(base_args)} include paths from configuration")

    # Combine base args with TOML flags
    all_args: List[str] = base_args + toml_flags

    # PCH output path in build cache directory (persistent)
    pch_output_path = None
    if use_pch:
        cache_dir = Path(".build") / "cache"
        cache_dir.mkdir(parents=True, exist_ok=True)
        pch_output_path = str(cache_dir / "fastled_pch.hpp.pch")

    # Determine compiler command (with or without cache)
    compiler_cmd = "python -m ziglang c++"
    cache_args: List[str] = []
    print("Using direct compilation with ziglang c++")

    # Combine cache args with other args (cache args go first)
    final_args: List[str] = cache_args + all_args

    settings = CompilerOptions(
        include_path=src_path,
        defines=stub_defines,  # Use configuration-based defines instead of hardcoded ones
        std_version="c++14",
        compiler=compiler_cmd,
        compiler_args=final_args,
        use_pch=use_pch,
        pch_output_path=pch_output_path,
        parallel=parallel,
    )
    # Load build flags from TOML
    current_dir_path = Path(current_dir)
    build_flags_path = current_dir_path / "ci" / "build_example.toml"
    build_flags = BuildFlags.parse(
        build_flags_path, quick_build=False, strict_mode=False
    )

    return Compiler(settings, build_flags)


def compile_examples_simple(
    compiler: Compiler,
    ino_files: List[Path],
    pch_compatible_files: set[Path],
    log_timing: Callable[[str], None],
    full_compilation: bool,
    verbose: bool = False,
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
        if verbose:
            pch_status = "with PCH" if use_pch_for_file else "direct compilation"
            print(
                f"[VERBOSE] Compiling {ino_file.relative_to(Path('examples'))} ({pch_status})"
            )
            log_timing(
                f"[VERBOSE] Compiling {ino_file.relative_to(Path('examples'))} ({pch_status})"
            )

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

            if verbose:
                pch_status = "with PCH" if use_pch_for_file else "direct compilation"
                print(
                    f"[VERBOSE] Compiling {cpp_file.relative_to(Path('examples'))} ({pch_status})"
                )
                log_timing(
                    f"[VERBOSE] Compiling {cpp_file.relative_to(Path('examples'))} ({pch_status})"
                )

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
    failure_count = 0
    for future in as_completed(
        future_to_file.keys(), timeout=_TIMEOUT
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

        # Show verbose completion status or only final completion
        if verbose:
            status = "SUCCESS" if result.ok else "FAILED"
            log_timing(
                f"[VERBOSE] {status}: {source_file.relative_to(Path('examples'))} ({completed_count}/{total_files})"
            )
        elif completed_count == total_files:
            log_timing(
                f"[SIMPLE] Completed {completed_count}/{total_files} compilations"
            )

        # Show compilation errors immediately for better debugging
        if not result.ok and result.stderr.strip():
            failure_count += 1
            log_timing(
                f"[ERROR] Compilation failed for {source_file.relative_to(Path('examples'))}:"
            )
            error_lines = result.stderr.strip().split("\n")
            for line in error_lines[:20]:  # Limit to first 20 lines per file
                log_timing(f"[ERROR]   {line}")
            if len(error_lines) > 20:
                log_timing(f"[ERROR]   ... ({len(error_lines) - 20} more lines)")

            if failure_count >= MAX_FAILURES_BEFORE_ABORT:
                log_timing(
                    f"[ERROR] Reached failure threshold ({MAX_FAILURES_BEFORE_ABORT}). Aborting remaining compilation jobs to avoid log spam."
                )
                break

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

    # Report failures summary if any (detailed errors already shown above)
    if failed:
        if verbose:
            log_timing(
                f"[SIMPLE] Failed examples summary: {[f['path'] for f in failed]}"
            )
        else:
            # Show full details only in non-verbose mode since we didn't show them above
            if len(failed) <= 10:
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
                object_file_map=None,
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
            object_file_map=None,
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
                object_file_map=None,
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
                object_file_map=None,
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
            object_file_map=None,
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
    compiler: Compiler,
    fastled_build_dir: Path,
    log_timing: Callable[[str], None],
    verbose: bool,
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
        if verbose:
            log_timing(f"[LIBRARY] Compiling src/{rel_path} -> {obj_file.name}")
        future = compiler.compile_cpp_file(cpp_file, obj_file)
        futures.append((future, obj_file, cpp_file))

    # Wait for compilation to complete
    compiled_count = 0
    failed_count = 0
    for future, obj_file, cpp_file in futures:
        try:
            result: Result = future.result()
            if result.ok:
                fastled_objects.append(obj_file)
                compiled_count += 1
                if verbose:
                    log_timing(
                        f"[LIBRARY] SUCCESS: {cpp_file.relative_to(Path('src'))}"
                    )
            else:
                failed_count += 1
                log_timing(
                    f"[LIBRARY] ERROR: Failed to compile {cpp_file.relative_to(Path('src'))}: {result.stderr[:300]}..."
                )
        except Exception as e:
            failed_count += 1
            log_timing(
                f"[LIBRARY] ERROR: Exception compiling {cpp_file.relative_to(Path('src'))}: {e}"
            )

    log_timing(
        f"[LIBRARY] Successfully compiled {compiled_count}/{len(fastled_sources)} FastLED sources"
    )

    # FAIL FAST: If any source files failed to compile, abort immediately
    if failed_count > 0:
        raise Exception(
            f"CRITICAL: {failed_count} FastLED source files failed to compile. "
            f"This indicates a serious build environment issue (likely PCH invalidation). "
            f"All source files must compile successfully."
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
        # In this project, we use Zig toolchain which has GNU target
        # The compiler is hardcoded to "python -m ziglang c++" in create_fastled_compiler()
        # Zig toolchain uses GNU-style linking even on Windows
        return [
            "-static-libgcc",  # Static link GCC runtime
            "-static-libstdc++",  # Static link C++ runtime
            "-lpthread",  # Threading support
            "-lm",  # Math library
            # Windows system libraries linked automatically
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
    """Create a main.cpp file with sketch runner support and stub main function."""
    main_cpp_content = """// Auto-generated main.cpp for Arduino sketch with sketch runner support
// This file provides both the main() function and sketch runner exports

#include <stdio.h>

#ifdef FASTLED_STUB_IMPL
#include "platforms/stub/time_stub.h"
#include "fl/function.h"
#endif

// Arduino sketch functions (provided by compiled .ino file)
extern void setup();
extern void loop();

// Sketch runner exports for external calling
extern "C" {
    void sketch_setup() {
        setup();
    }
    
    void sketch_loop() {
        loop();
    }
}

// Main function for direct execution
int main() {
    printf("RUNNER: Starting sketch execution\\n");
    
#ifdef FASTLED_STUB_IMPL
    // Override delay function to return immediately for fast testing
    setDelayFunction([](uint32_t ms) {
        // Fast delay override - do nothing for speed
        (void)ms; // Suppress unused parameter warning
    });
#endif
    
    // Initialize sketch
    printf("RUNNER: Calling setup()\\n");
    setup();
    printf("RUNNER: Setup complete\\n");
    
    // Run sketch loop limited times for testing (not infinite)
    printf("RUNNER: Running loop() 5 times for testing\\n");
    for (int i = 1; i <= 5; i++) {
        printf("RUNNER: Loop iteration %d\\n", i);
        loop();
    }
    
    printf("RUNNER: Sketch execution complete\\n");
    return 0;
}
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
                    f"[LINKING] FAILED: {executable_name}: Failed to compile main.cpp: {main_result.stderr[:300]}..."
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
                log_timing(f"[LINKING] SUCCESS: {executable_name}")

                # Verify the executable was actually created
                if executable_path.exists():
                    try:
                        file_stat = executable_path.stat()
                        is_executable = bool(file_stat.st_mode & stat.S_IXUSR)
                        log_timing(
                            f"[LINKING] VERIFIED: {executable_name} created, size: {file_stat.st_size} bytes, executable: {is_executable}"
                        )
                        # Fix permissions if needed on Unix systems
                        if not is_executable and not platform.system() == "Windows":
                            try:
                                executable_path.chmod(
                                    executable_path.stat().st_mode
                                    | stat.S_IXUSR
                                    | stat.S_IXGRP
                                    | stat.S_IXOTH
                                )
                                log_timing(
                                    f"[LINKING] FIXED: Added execute permissions to {executable_name}"
                                )
                            except Exception as e:
                                log_timing(
                                    f"[LINKING] WARNING: Could not fix permissions for {executable_name}: {e}"
                                )

                        # Only count as success if executable actually exists and is valid
                        linked_count += 1

                    except Exception as e:
                        log_timing(
                            f"[LINKING] WARNING: Error checking created executable: {e}"
                        )
                        # Count as success anyway since file exists, just can't check it
                        linked_count += 1
                else:
                    # CRITICAL BUG FIX: If executable doesn't exist, count as failure!
                    failed_count += 1
                    log_timing(
                        f"[LINKING] FAILED: {executable_name} reported success but file not found at {executable_path}"
                    )
            else:
                failed_count += 1
                log_timing(
                    f"[LINKING] FAILED: {executable_name}: {link_result.stderr[:200]}..."
                )

        except Exception as e:
            failed_count += 1
            log_timing(f"[LINKING] FAILED: {executable_name}: Exception: {e}")

        if failed_count >= MAX_FAILURES_BEFORE_ABORT:
            log_timing(
                f"[LINKING] Reached failure threshold ({MAX_FAILURES_BEFORE_ABORT}). Aborting further linking to avoid repeated errors."
            )
            break

    return LinkingResult(linked_count=linked_count, failed_count=failed_count)


def link_examples_with_cache(
    object_file_map: Dict[Path, List[Path]],
    fastled_lib: Path,
    build_dir: Path,
    compiler: Compiler,
    log_timing: Callable[[str], None],
) -> LinkingResult:
    """
    Link all examples into executable programs with intelligent caching.

    Leverages the existing FastLEDTestCompiler cache infrastructure to skip
    linking when all input artifacts (object files, library, linker args) are unchanged.

    Uses parallel cache detection for much faster cache checking (~10x speedup).
    """
    from ci.compiler.clang_compiler import link_program_sync

    linked_count = 0
    failed_count = 0
    cache_hits = 0
    cache_misses = 0

    # Create cache directory (same as FastLEDTestCompiler)
    cache_dir = Path(".build/link_cache")
    cache_dir.mkdir(parents=True, exist_ok=True)

    @dataclass
    class ExampleCacheInfo:
        """Information about an example's cache status and linking requirements"""

        ino_file: Path
        obj_files: List[Path]
        example_name: str
        executable_name: str
        executable_path: Path
        main_obj_path: Path
        all_obj_files: List[Path]
        cache_key: str
        cached_exe: Optional[Path]
        is_cache_hit: bool
        main_compile_future: Optional[Any] = None

    # Check if parallel processing is disabled
    no_parallel = False
    try:
        # Access the config through the module's current instance if available
        import inspect

        from ci.compiler.test_example_compilation import CompilationTestRunner

        frame = inspect.currentframe()
        while frame:
            if "self" in frame.f_locals and hasattr(frame.f_locals["self"], "config"):
                config = frame.f_locals["self"].config
                no_parallel = getattr(config, "no_parallel", False)
                break
            frame = frame.f_back
    except Exception:
        # Fallback: check environment variable
        import os

        no_parallel = os.getenv("NO_PARALLEL") == "1"

    def parallel_cache_detection(
        object_file_map: Dict[Path, List[Path]],
        fastled_lib: Path,
        build_dir: Path,
        compiler: Compiler,
        log_timing: Callable[[str], None],
    ) -> tuple[int, int, int, int]:
        """
        Parallel cache detection and linking for much faster processing.

        Returns: (linked_count, failed_count, cache_hits, cache_misses)
        """
        linked_count = 0
        failed_count = 0
        cache_hits = 0
        cache_misses = 0

        # Phase 1: Prepare all examples and submit main.cpp compilations in parallel
        example_infos: List[ExampleCacheInfo] = []

        log_timing(
            f"[LINKING] Starting parallel preparation for {len(object_file_map)} examples..."
        )

        for ino_file, obj_files in object_file_map.items():
            example_name = ino_file.parent.name
            example_build_dir = build_dir / example_name
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

            # Create main.cpp and submit compilation (non-blocking)
            main_cpp_path = create_main_cpp_for_example(example_build_dir)
            main_obj_path = example_build_dir / "main.o"
            main_future = compiler.compile_cpp_file(main_cpp_path, main_obj_path)

            # Create example info (cache key will be calculated after main.cpp compiles)
            example_info = ExampleCacheInfo(
                ino_file=ino_file,
                obj_files=obj_files,
                example_name=example_name,
                executable_name=executable_name,
                executable_path=executable_path,
                main_obj_path=main_obj_path,
                all_obj_files=[],  # Will be filled after main.cpp compiles
                cache_key="",  # Will be calculated
                cached_exe=None,  # Will be checked
                is_cache_hit=False,  # Will be determined
                main_compile_future=main_future,
            )
            example_infos.append(example_info)

        # Phase 2: Wait for main.cpp compilations and calculate cache keys in parallel
        log_timing(
            f"[LINKING] Parallel cache key calculation for {len(example_infos)} examples..."
        )

        for example_info in example_infos[
            :
        ]:  # Use slice to allow removal during iteration
            try:
                main_result: Result = example_info.main_compile_future.result()

                if not main_result.ok:
                    log_timing(
                        f"[LINKING] FAILED: {example_info.executable_name}: Failed to compile main.cpp: {main_result.stderr[:100]}..."
                    )
                    failed_count += 1
                    example_infos.remove(example_info)
                    continue

                # Update example info with complete information
                example_info.all_obj_files = example_info.obj_files + [
                    example_info.main_obj_path
                ]
                linker_args = get_platform_linker_args()
                example_info.cache_key = calculate_multiple_objects_cache_key(
                    example_info.all_obj_files, fastled_lib, linker_args
                )
                example_info.cached_exe = get_cached_executable(
                    example_info.example_name, example_info.cache_key
                )
                example_info.is_cache_hit = example_info.cached_exe is not None

            except Exception as e:
                log_timing(
                    f"[LINKING] FAILED: {example_info.executable_name}: Exception during preparation: {e}"
                )
                failed_count += 1
                example_infos.remove(example_info)
                if failed_count >= MAX_FAILURES_BEFORE_ABORT:
                    log_timing(
                        f"[LINKING] Reached failure threshold ({MAX_FAILURES_BEFORE_ABORT}) during preparation. Aborting."
                    )
                    return linked_count, failed_count, cache_hits, cache_misses

        # Phase 3: Separate cache hits from cache misses
        cache_hit_examples = [info for info in example_infos if info.is_cache_hit]
        cache_miss_examples = [info for info in example_infos if not info.is_cache_hit]

        log_timing(
            f"[LINKING] Cache analysis: {len(cache_hit_examples)} hits, {len(cache_miss_examples)} misses"
        )

        # Phase 4: Process cache hits in parallel (copy cached executables)
        if cache_hit_examples:

            def copy_cached_executable(example_info: ExampleCacheInfo) -> bool:
                try:
                    if example_info.cached_exe is None:
                        return False
                    shutil.copy2(example_info.cached_exe, example_info.executable_path)
                    return True
                except Exception as e:
                    log_timing(
                        f"[LINKING] Warning: Failed to copy cached {example_info.executable_name}, will relink: {e}"
                    )
                    return False

            with concurrent.futures.ThreadPoolExecutor(
                max_workers=min(len(cache_hit_examples), 8)
            ) as executor:
                copy_futures = {
                    executor.submit(copy_cached_executable, info): info
                    for info in cache_hit_examples
                }

                for future in concurrent.futures.as_completed(copy_futures):
                    example_info = copy_futures[future]
                    try:
                        success = future.result()
                        if success:
                            linked_count += 1
                            cache_hits += 1
                            log_timing(
                                f"[LINKING] {green_text('SUCCESS (CACHED)')}: {example_info.executable_name}"
                            )
                        else:
                            # Failed to copy, add to cache misses for actual linking
                            cache_miss_examples.append(example_info)
                    except Exception as e:
                        log_timing(
                            f"[LINKING] Exception copying cached {example_info.executable_name}: {e}"
                        )
                        cache_miss_examples.append(example_info)

        # Phase 5: Process cache misses in parallel (actual linking)
        if cache_miss_examples:

            def link_example(example_info: ExampleCacheInfo) -> bool:
                try:
                    linker_args = get_platform_linker_args()
                    link_options = LinkOptions(
                        output_executable=str(example_info.executable_path),
                        object_files=[str(obj) for obj in example_info.all_obj_files],
                        static_libraries=[str(fastled_lib)],
                        linker_args=linker_args,
                    )

                    link_result: Result = link_program_sync(
                        link_options, compiler.build_flags
                    )

                    if link_result.ok:
                        # Cache the successful executable
                        cache_executable(
                            example_info.example_name,
                            example_info.cache_key,
                            example_info.executable_path,
                        )
                        return True
                    else:
                        log_timing(
                            f"[LINKING] FAILED: {example_info.executable_name}: {link_result.stderr[:200]}..."
                        )
                        return False

                except Exception as e:
                    log_timing(
                        f"[LINKING] FAILED: {example_info.executable_name}: Exception: {e}"
                    )
                    return False

            with concurrent.futures.ThreadPoolExecutor(
                max_workers=min(len(cache_miss_examples), 4)
            ) as executor:
                link_futures = {
                    executor.submit(link_example, info): info
                    for info in cache_miss_examples
                }

                for future in concurrent.futures.as_completed(link_futures):
                    example_info = link_futures[future]
                    try:
                        success = future.result()
                        if success:
                            linked_count += 1
                            cache_misses += 1
                            log_timing(
                                f"[LINKING] {green_text('SUCCESS (REBUILT)')}: {example_info.executable_name}"
                            )
                        else:
                            failed_count += 1
                            if failed_count >= MAX_FAILURES_BEFORE_ABORT:
                                log_timing(
                                    f"[LINKING] Reached failure threshold ({MAX_FAILURES_BEFORE_ABORT}). Aborting remaining linking jobs."
                                )
                                break
                    except Exception as e:
                        log_timing(
                            f"[LINKING] Exception linking {example_info.executable_name}: {e}"
                        )
                        failed_count += 1
                        if failed_count >= MAX_FAILURES_BEFORE_ABORT:
                            log_timing(
                                f"[LINKING] Reached failure threshold ({MAX_FAILURES_BEFORE_ABORT}). Aborting remaining linking jobs."
                            )
                            break

        return linked_count, failed_count, cache_hits, cache_misses

    # Helper functions using the same algorithms as FastLEDTestCompiler
    def calculate_file_hash(file_path: Path) -> str:
        """Calculate SHA256 hash of a file (same as FastLEDTestCompiler._calculate_file_hash)"""
        if not file_path.exists():
            return "no_file"

        hash_sha256 = hashlib.sha256()
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_sha256.update(chunk)
        return hash_sha256.hexdigest()

    def calculate_linker_args_hash(linker_args: List[str]) -> str:
        """Calculate SHA256 hash of linker arguments (same as FastLEDTestCompiler._calculate_linker_args_hash)"""
        args_str = "|".join(sorted(linker_args))
        hash_sha256 = hashlib.sha256()
        hash_sha256.update(args_str.encode("utf-8"))
        return hash_sha256.hexdigest()

    def calculate_multiple_objects_cache_key(
        obj_files: List[Path], fastled_lib: Path, linker_args: List[str]
    ) -> str:
        """
        Calculate cache key for multiple object files linking.
        Extends FastLEDTestCompiler logic to handle multiple object files.
        """
        # Calculate hash for each object file
        obj_hashes: List[str] = []
        for obj_file in obj_files:
            obj_hashes.append(calculate_file_hash(obj_file))

        # Combine object file hashes in a stable way (sorted by path for consistency)
        sorted_obj_paths = sorted(str(obj) for obj in obj_files)
        sorted_obj_hashes: List[str] = []
        for obj_path in sorted_obj_paths:
            obj_file = Path(obj_path)
            sorted_obj_hashes.append(calculate_file_hash(obj_file))

        combined_obj_hash = hashlib.sha256(
            "|".join(sorted_obj_hashes).encode("utf-8")
        ).hexdigest()

        # Calculate other hashes
        fastled_hash = calculate_file_hash(fastled_lib)
        linker_hash = calculate_linker_args_hash(linker_args)
        # Include header dependency fingerprint to invalidate link cache when headers change
        try:
            header_hash = ""
            if hasattr(compiler, "_get_pch_dependencies"):
                dep_paths = compiler._get_pch_dependencies()  # type: ignore[attr-defined]
                dep_hashes: List[str] = []
                for p in dep_paths:
                    dep_hashes.append(calculate_file_hash(p))
                header_hash = hashlib.sha256(
                    "|".join(dep_hashes).encode("utf-8")
                ).hexdigest()
            else:
                header_hash = "no_header_info"
        except KeyboardInterrupt:
            import _thread

            _thread.interrupt_main()
            raise
        except Exception:
            header_hash = "header_scan_error"

        # Combine all components (same format as FastLEDTestCompiler)
        combined = f"fastled:{fastled_hash}|objects:{combined_obj_hash}|flags:{linker_hash}|hdr:{header_hash}"
        final_hash = hashlib.sha256(combined.encode("utf-8")).hexdigest()

        return final_hash[
            :16
        ]  # Use first 16 chars for readability (same as FastLEDTestCompiler)

    def get_cached_executable(example_name: str, cache_key: str) -> Optional[Path]:
        """Check if cached executable exists (same as FastLEDTestCompiler._get_cached_executable)"""
        cached_exe = cache_dir / f"{example_name}_{cache_key}.exe"
        return cached_exe if cached_exe.exists() else None

    def cache_executable(example_name: str, cache_key: str, exe_path: Path) -> None:
        """Cache an executable (same as FastLEDTestCompiler._cache_executable)"""
        if not exe_path.exists():
            return

        cached_exe = cache_dir / f"{example_name}_{cache_key}.exe"
        try:
            shutil.copy2(exe_path, cached_exe)
        except Exception as e:
            log_timing(f"[LINKING] Warning: Failed to cache {example_name}: {e}")

    # Choose between parallel and serial processing based on --no-parallel flag
    if no_parallel:
        log_timing("[LINKING] Serial cache detection (--no-parallel specified)")

        # Original serial processing
        for ino_file, obj_files in object_file_map.items():
            example_name = ino_file.parent.name
            example_build_dir = build_dir / example_name

            # Create executable name and path
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

                # Combine all object files for linking
                all_obj_files = obj_files + [main_obj_path]

                # Get platform-specific linker arguments
                linker_args = get_platform_linker_args()

                # Calculate cache key using the same algorithm as FastLEDTestCompiler
                cache_key = calculate_multiple_objects_cache_key(
                    all_obj_files, fastled_lib, linker_args
                )

                # Check for cached executable
                cached_exe = get_cached_executable(example_name, cache_key)
                if cached_exe:
                    try:
                        # Copy cached executable to target location
                        shutil.copy2(cached_exe, executable_path)
                        linked_count += 1
                        cache_hits += 1
                        log_timing(
                            f"[LINKING] {green_text('SUCCESS (CACHED)')}: {executable_name}"
                        )
                        continue  # Skip actual linking
                    except Exception as e:
                        log_timing(
                            f"[LINKING] Warning: Failed to copy cached {executable_name}, will relink: {e}"
                        )
                        # Fall through to actual linking

                # No cache hit - perform actual linking using existing link_program_sync
                link_options = LinkOptions(
                    output_executable=str(executable_path),
                    object_files=[str(obj) for obj in all_obj_files],
                    static_libraries=[str(fastled_lib)],
                    linker_args=linker_args,
                )

                # Use link_program_sync for synchronous linking
                link_result: Result = link_program_sync(
                    link_options, compiler.build_flags
                )

                if link_result.ok:
                    linked_count += 1
                    cache_misses += 1
                    log_timing(
                        f"[LINKING] {green_text('SUCCESS (REBUILT)')}: {executable_name}"
                    )

                    # Cache the newly linked executable for future use
                    cache_executable(example_name, cache_key, executable_path)
                else:
                    failed_count += 1
                    log_timing(
                        f"[LINKING] FAILED: {executable_name}: {link_result.stderr[:200]}..."
                    )

            except Exception as e:
                failed_count += 1
                log_timing(f"[LINKING] FAILED: {executable_name}: Exception: {e}")

    else:
        # Use parallel cache detection for much faster processing
        log_timing("[LINKING] Parallel cache detection enabled")
        linked_count, failed_count, cache_hits, cache_misses = parallel_cache_detection(
            object_file_map, fastled_lib, build_dir, compiler, log_timing
        )

    return LinkingResult(
        linked_count=linked_count,
        failed_count=failed_count,
        cache_hits=cache_hits,
        cache_misses=cache_misses,
    )


@dataclass
class CompilationTestConfig:
    """Configuration for the compilation test."""

    specific_examples: Optional[List[str]]
    clean_build: bool
    disable_pch: bool
    unity_build: bool
    unity_custom_output: Optional[str]
    unity_additional_flags: Optional[List[str]]
    full_compilation: bool
    no_parallel: bool
    verbose: bool


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
    object_file_map: Optional[Dict[Path, List[Path]]]
    execution_failures: List[ExecutionFailure]
    # Sketch runner execution results
    executed_count: int = 0
    execution_failed_count: int = 0
    execution_time: float = 0.0


class CompilationTestRunner:
    """Handles the orchestration of FastLED example compilation tests."""

    def __init__(self, config: CompilationTestConfig):
        self.config = config
        self.global_start_time = time.time()

    def log_timing(self, message: str) -> None:
        """Log a message with timestamp relative to start."""
        elapsed = time.time() - self.global_start_time
        print(f"[{elapsed:6.2f}s] {message}")

    def initialize_system(
        self,
    ) -> tuple[
        Compiler, Dict[str, Union[str, int, float]], Dict[str, Union[bool, str]]
    ]:
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
                parallel=not self.config.no_parallel,
            )

            # Verify compiler accessibility

            version_result = compiler.check_clang_version()

            if not version_result.success:
                raise RuntimeError(
                    f"Compiler accessibility check failed: {version_result.error}"
                )

            return compiler, system_info, build_config

        except Exception as e:
            raise RuntimeError(f"Failed to initialize simple build system: {e}")

    def discover_examples(self, compiler: Compiler) -> List[Path]:
        """Discover and validate .ino examples to compile."""
        self.log_timing("Discovering .ino examples...")
        if self.config.verbose:
            print(
                f"[VERBOSE] Discovering examples with filter: {self.config.specific_examples}"
            )

        try:
            filter_names = (
                self.config.specific_examples if self.config.specific_examples else None
            )
            ino_files = compiler.find_ino_files("examples", filter_names=filter_names)

            if not ino_files:
                if self.config.specific_examples:
                    # Show detailed error with suggestions
                    all_ino_files = list(Path("examples").rglob("*.ino"))
                    if all_ino_files:
                        available_names = sorted([f.stem for f in all_ino_files])
                        suggestion = f"\nAvailable examples include: {', '.join(available_names[:10])}..."
                        if len(available_names) > 10:
                            suggestion += (
                                f"\n  ... and {len(available_names) - 10} more examples"
                            )
                    else:
                        suggestion = ""

                    raise ValueError(
                        f"No .ino files found matching: {self.config.specific_examples}{suggestion}"
                    )
                else:
                    raise ValueError("No .ino files found in examples directory")

            # Report discovery results
            if self.config.specific_examples:
                self.log_timing(
                    f"[DISCOVER] Found {len(ino_files)} specific examples: {', '.join([f.stem for f in ino_files])}"
                )
                if self.config.verbose:
                    print(f"[VERBOSE] Specific examples found:")
                    for ino_file in ino_files:
                        print(f"[VERBOSE]   - {ino_file}")
            else:
                self.log_timing(
                    f"[DISCOVER] Found {len(ino_files)} total .ino examples in examples/"
                )
                if self.config.verbose:
                    print(f"[VERBOSE] All examples found:")
                    for ino_file in ino_files:
                        print(f"[VERBOSE]   - {ino_file.stem}")

            return ino_files

        except Exception as e:
            raise RuntimeError(f"Failed to discover examples: {e}")

    def analyze_pch_compatibility(
        self, compiler: Compiler, ino_files: List[Path]
    ) -> tuple[set[Path], List[str], List[str]]:
        """Analyze files for PCH compatibility and return configuration."""
        config_parts = ["Simple Build System: enabled"]

        if self.config.unity_build:
            config_parts.append("UNITY build: enabled")
        else:
            config_parts.append("Direct .ino compilation: enabled")

        # Caching is disabled
        config_parts.append("cache: disabled")

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
                    self.log_timing(
                        f"[PCH]   - ... and {len(pch_incompatible_files) - 5} more"
                    )

                config_parts.append(
                    f"PCH: selective ({len(pch_compatible_files)} files)"
                )
            else:
                self.log_timing(f"[PCH] All {len(ino_files)} files are PCH compatible")
                config_parts.append("PCH: enabled (all files)")
        else:
            self.log_timing("[PCH] PCH disabled globally")
            config_parts.append("PCH: disabled")

        return pch_compatible_files, pch_incompatible_files, config_parts

    def setup_pch(self, compiler: Compiler, pch_compatible_files: set[Path]) -> bool:
        """Setup precompiled headers if applicable."""
        if (
            not self.config.unity_build
            and compiler.settings.use_pch
            and len(pch_compatible_files) > 0
        ):
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
            self.log_timing(
                "[PCH] No PCH-compatible files found, skipping PCH creation"
            )

        return True

    def compile_examples(
        self,
        compiler: Compiler,
        ino_files: List[Path],
        pch_compatible_files: set[Path],
        enable_fingerprint_cache: bool = True,
        cache_file: str = ".build/fingerprint_cache.json",
        cache_verbose: bool = False,
    ) -> CompilationTestResults:
        """Execute the compilation process."""
        parallel_status = "disabled" if self.config.no_parallel else "enabled"
        self.log_timing(
            f"[PERF] Parallel compilation: {parallel_status} (managed by compiler)"
        )
        self.log_timing("[PERF] Direct compilation enabled (no CMake overhead)")

        if self.config.verbose:
            print(f"[VERBOSE] Starting compilation with configuration:")
            print(f"[VERBOSE]   - Examples: {len(ino_files)}")
            print(f"[VERBOSE]   - PCH compatible: {len(pch_compatible_files)}")
            print(f"[VERBOSE]   - PCH disabled: {self.config.disable_pch}")
            print(f"[VERBOSE]   - Parallel: {not self.config.no_parallel}")
            print(f"[VERBOSE]   - Full compilation: {self.config.full_compilation}")
            print(f"[VERBOSE]   - Unity build: {self.config.unity_build}")

        self.log_timing(f"\n[BUILD] Starting example compilation...")
        self.log_timing(f"[BUILD] Target examples: {len(ino_files)}")

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
                if enable_fingerprint_cache:
                    # Use cache-aware compilation
                    from ci.compiler.cache_enhanced_compilation import (
                        create_cache_compiler,
                    )

                    cache_compiler = create_cache_compiler(
                        compiler, Path(cache_file), verbose=cache_verbose
                    )

                    cache_result = cache_compiler.compile_with_cache(
                        ino_files,
                        pch_compatible_files,
                        self.log_timing,
                        self.config.full_compilation,
                        self.config.verbose,
                    )

                    result = cache_result["compilation_result"]
                else:
                    # Standard compilation without cache
                    result = compile_examples_simple(
                        compiler,
                        ino_files,
                        pch_compatible_files,
                        self.log_timing,
                        self.config.full_compilation,
                        self.config.verbose,
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
                object_file_map=getattr(result, "object_file_map", None),
                execution_failures=[],
            )

        except Exception as e:
            raise RuntimeError(f"Compilation failed: {e}")

    def handle_linking(
        self, compiler: Compiler, results: CompilationTestResults
    ) -> CompilationTestResults:
        """Handle linking phase if full compilation is requested."""
        if (
            not self.config.full_compilation
            or results.failed_count > 0
            or not results.object_file_map
        ):
            if self.config.full_compilation and results.failed_count > 0:
                self.log_timing(
                    f"[LINKING] Skipping linking due to {results.failed_count} compilation failures"
                )
            return results

        self.log_timing("\n[LINKING] Starting real program linking...")
        linking_start = time.time()

        try:
            # Create FastLED static library
            self.log_timing("[LINKING] Creating FastLED static library...")
            fastled_build_dir = Path(".build/fastled")
            fastled_build_dir.mkdir(parents=True, exist_ok=True)

            fastled_lib = create_fastled_library(
                compiler,
                fastled_build_dir,
                self.log_timing,
                verbose=self.config.verbose,
            )

            # Link examples
            self.log_timing("[LINKING] Linking example programs...")
            build_dir = Path(".build/examples")
            linking_result = link_examples_with_cache(
                results.object_file_map,
                fastled_lib,
                build_dir,
                compiler,
                self.log_timing,
            )

            results.linked_count = linking_result.linked_count
            results.linking_failed_count = linking_result.failed_count
            results.linking_time = time.time() - linking_start

            if results.linking_failed_count == 0:
                self.log_timing(
                    f"[LINKING] SUCCESS: Successfully linked {results.linked_count} executable programs"
                )
            else:
                self.log_timing(
                    f"[LINKING] WARNING: Linked {results.linked_count} programs, {results.linking_failed_count} failed"
                )

            # Report cache statistics
            if hasattr(linking_result, "cache_hits") and hasattr(
                linking_result, "cache_misses"
            ):
                total_processed = (
                    linking_result.cache_hits + linking_result.cache_misses
                )
                if total_processed > 0:
                    hit_rate = linking_result.cache_hit_rate
                    self.log_timing(
                        f"[LINKING] Cache: {linking_result.cache_hits} hits, {linking_result.cache_misses} misses ({hit_rate:.1f}% hit rate)"
                    )

            self.log_timing(
                f"[LINKING] Real linking completed in {results.linking_time:.2f}s"
            )

        except Exception as e:
            results.linking_time = time.time() - linking_start
            self.log_timing(f"[LINKING] ERROR: Linking failed: {e}")
            results.linking_failed_count = (
                results.successful_count
            )  # Mark all as failed
            results.linked_count = 0

        return results

    def handle_execution(
        self, results: CompilationTestResults
    ) -> CompilationTestResults:
        """Execute linked programs when full compilation is requested."""
        if (
            not self.config.full_compilation
            or results.linking_failed_count > 0
            or results.linked_count == 0
        ):
            if self.config.full_compilation and results.linking_failed_count > 0:
                self.log_timing(
                    f"[EXECUTION] Skipping execution due to {results.linking_failed_count} linking failures"
                )
            return results

        self.log_timing("\n[EXECUTION] Starting sketch runner execution...")
        execution_start = time.time()

        executed_count = 0
        execution_failed_count = 0
        execution_failures: List[ExecutionFailure] = []
        build_dir = Path(".build/examples")

        # Only execute examples that were compiled and linked in this run
        if not results.object_file_map:
            self.log_timing(
                "[EXECUTION] No object file map available - no examples to execute"
            )
            return results

        # Execute only the examples from this compilation run
        failure_threshold_reached = False
        for ino_file in results.object_file_map.keys():
            example_name = ino_file.parent.name
            example_dir = build_dir / example_name

            if not example_dir.is_dir():
                continue

            executable_name = get_executable_name(example_name)
            executable_path = example_dir / executable_name

            if not executable_path.exists():
                # Enhanced logging to debug why executable is missing
                self.log_timing(
                    f"[EXECUTION] SKIPPED: {executable_name}: Executable not found at {executable_path}"
                )
                # Show what files actually exist in the directory
                try:
                    existing_files = list(example_dir.iterdir())
                    self.log_timing(
                        f"[EXECUTION] DEBUG: Directory {example_dir} contains: {[f.name for f in existing_files]}"
                    )
                except Exception as e:
                    self.log_timing(f"[EXECUTION] DEBUG: Error listing directory: {e}")
                continue

            # Check executable permissions
            import stat

            try:
                file_stat = executable_path.stat()
                is_executable = bool(file_stat.st_mode & stat.S_IXUSR)
                self.log_timing(
                    f"[EXECUTION] DEBUG: {executable_name} exists, size: {file_stat.st_size} bytes, executable: {is_executable}"
                )
            except Exception as e:
                self.log_timing(
                    f"[EXECUTION] DEBUG: Error checking file permissions: {e}"
                )

            rp = RunningProcess(
                command=[str(executable_path.absolute())],
                cwd=example_dir.absolute(),
                check=False,
                auto_run=True,
                enable_stack_trace=True,
                on_complete=None,
                output_formatter=None,
            )

            try:
                self.log_timing(f"[EXECUTION] Running: {executable_name}")

                # Use RunningProcess to execute and stream output

                with rp.line_iter(timeout=60) as it:
                    for line in it:
                        if self.config.verbose:
                            self.log_timing(f"[EXECUTION]   {line}")

                rc = rp.wait()

                if rc == 0:
                    executed_count += 1
                    self.log_timing(f"[EXECUTION] SUCCESS: {executable_name}")
                else:
                    execution_failed_count += 1
                    self.log_timing(
                        f"[EXECUTION] FAILED: {executable_name}: Exit code {rc}"
                    )
                    execution_failures.append(
                        ExecutionFailure(
                            name=example_name,
                            reason=f"exit code {rc}",
                            stdout=rp.stdout,
                        )
                    )
                    if self.config.verbose:
                        self.log_timing(f"[EXECUTION] Failed output:")
                        # If nothing meaningful captured, note it
                        stdout = rp.stdout
                        if not stdout.strip():
                            self.log_timing(
                                "[EXECUTION] No output captured from failed execution"
                            )

            except TimeoutError:
                execution_failed_count += 1
                self.log_timing(
                    f"[EXECUTION] FAILED: {executable_name}: Execution timeout (30s)"
                )
                timeout_stdout: str = "\n".join(rp.stdout)
                execution_failures.append(
                    ExecutionFailure(
                        name=example_name,
                        reason="timeout (30s)",
                        stdout=timeout_stdout,
                    )
                )
            except Exception as e:
                execution_failed_count += 1
                self.log_timing(
                    f"[EXECUTION] FAILED: {executable_name}: Exception: {e}"
                )

                exc_stdout: str = "\n".join(rp.stdout)
                execution_failures.append(
                    ExecutionFailure(
                        name=example_name,
                        reason=f"exception: {e}",
                        stdout=exc_stdout,
                    )
                )

            if execution_failed_count >= MAX_FAILURES_BEFORE_ABORT:
                self.log_timing(
                    f"[EXECUTION] Reached failure threshold ({MAX_FAILURES_BEFORE_ABORT}). Aborting further execution to avoid repeated errors."
                )
                failure_threshold_reached = True
                break

        execution_time = time.time() - execution_start

        if execution_failed_count == 0:
            self.log_timing(
                f"[EXECUTION] SUCCESS: Successfully executed {executed_count} sketch programs"
            )
        else:
            self.log_timing(
                f"[EXECUTION] WARNING: Executed {executed_count} programs, {execution_failed_count} failed"
            )

        self.log_timing(
            f"[EXECUTION] Sketch execution completed in {execution_time:.2f}s"
        )

        # Store execution results in the results object
        results.executed_count = executed_count
        results.execution_failed_count = execution_failed_count
        results.execution_time = execution_time
        results.execution_failures = execution_failures

        return results

    def report_results(
        self,
        ino_files: List[Path],
        results: CompilationTestResults,
        config_parts: List[str],
    ) -> int:
        """Generate the final report and return exit code."""
        total_time = (
            results.compile_time + results.linking_time + results.execution_time
        )
        parallel_status = "disabled" if self.config.no_parallel else "enabled"

        self.log_timing(f"[CONFIG] Mode: {', '.join(config_parts)}")
        self.log_timing(
            f"\n[BUILD] Parallel compilation: {parallel_status} (managed by compiler)"
        )

        # Enhanced timing breakdown
        self.log_timing(f"\n[TIMING] Compilation: {results.compile_time:.2f}s")
        linking_mode = (
            "(with program generation)"
            if self.config.full_compilation and results.failed_count == 0
            else "(compile-only mode)"
        )
        self.log_timing(f"[TIMING] Linking: {results.linking_time:.2f}s {linking_mode}")
        if self.config.full_compilation and results.execution_time > 0:
            self.log_timing(
                f"[TIMING] Execution: {results.execution_time:.2f}s (sketch runner)"
            )
        self.log_timing(f"[TIMING] Total: {total_time:.2f}s")

        # Performance summary
        self.log_timing(f"\n[SUMMARY] FastLED Example Compilation Performance:")
        self.log_timing(f"[SUMMARY]   Examples processed: {len(ino_files)}")

        if self.config.verbose:
            print(f"\n[VERBOSE] Detailed compilation results:")
            print(f"[VERBOSE]   - Total examples: {len(ino_files)}")
            print(f"[VERBOSE]   - Successful: {results.successful_count}")
            print(f"[VERBOSE]   - Failed: {results.failed_count}")
            print(f"[VERBOSE]   - Compilation time: {results.compile_time:.2f}s")
            print(f"[VERBOSE]   - Linking time: {results.linking_time:.2f}s")
            print(f"[VERBOSE]   - Execution time: {results.execution_time:.2f}s")
            print(f"[VERBOSE]   - Total time: {total_time:.2f}s")
        self.log_timing(f"[SUMMARY]   Successful: {results.successful_count}")
        self.log_timing(f"[SUMMARY]   Failed: {results.failed_count}")
        if self.config.full_compilation:
            self.log_timing(f"[SUMMARY]   Linked: {results.linked_count}")
            self.log_timing(
                f"[SUMMARY]   Linking failed: {results.linking_failed_count}"
            )
            self.log_timing(f"[SUMMARY]   Executed: {results.executed_count}")
            self.log_timing(
                f"[SUMMARY]   Execution failed: {results.execution_failed_count}"
            )
        self.log_timing(f"[SUMMARY]   Parallel compilation: {parallel_status}")
        self.log_timing(f"[SUMMARY]   Build time: {results.compile_time:.2f}s")

        # Show both compilation speed and total throughput for complete picture
        if results.compile_time > 0:
            self.log_timing(
                f"[SUMMARY]   Compilation speed: {len(ino_files) / results.compile_time:.1f} examples/second"
            )
        if total_time > 0:
            self.log_timing(
                f"[SUMMARY]   Total throughput: {len(ino_files) / total_time:.1f} examples/second"
            )

        # Detailed execution failure reporting (rich output without timestamps)
        if self.config.full_compilation and results.execution_failed_count > 0:
            print()
            print("########################################################")
            print("# ERROR: TEST EXECUTION FAILED (see detailed output below) #")
            print("########################################################")
            print()
            for failure in results.execution_failures:
                print(f"{failure.name} failed with:\n")
                preview: str = failure.stdout or failure.reason
                preview = preview.strip()
                if len(preview) > 300:
                    preview = preview[:300]
                if preview:
                    print(preview)
                else:
                    print("(no output captured)")
                print("\n------------\n")
            print("Failing tests (see detailed output above)")
            for failure in results.execution_failures:
                print(f"  * {failure.name}")

        # Determine success
        overall_success = results.failed_count == 0
        if self.config.full_compilation:
            overall_success = (
                overall_success
                and results.linking_failed_count == 0
                and results.execution_failed_count == 0
            )

        if overall_success:
            if self.config.full_compilation:
                self.log_timing(
                    "\n[SUCCESS] EXAMPLE COMPILATION + LINKING + EXECUTION TEST: SUCCESS"
                )
                self.log_timing(
                    f"[SUCCESS] {len(ino_files)} examples compiled ({results.successful_count} compilation jobs), "
                    f"{results.linked_count} linked, and {results.executed_count} executed successfully"
                )
            else:
                self.log_timing("\n[SUCCESS] EXAMPLE COMPILATION TEST: SUCCESS")
                self.log_timing(
                    f"[SUCCESS] {len(ino_files)} examples compiled successfully ({results.successful_count} compilation jobs)"
                )

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
                self.log_timing(
                    f"[ERROR] {results.linking_failed_count} linking failures detected"
                )
                self.log_timing(
                    "[ERROR] Test failed due to linker errors - see linking output above"
                )

            return 1


def run_example_compilation_test(
    specific_examples: Optional[List[str]],
    clean_build: bool,
    disable_pch: bool,
    unity_build: bool,
    unity_custom_output: Optional[str],
    unity_additional_flags: Optional[List[str]],
    full_compilation: bool,
    no_parallel: bool,
    verbose: bool = False,
    enable_fingerprint_cache: bool = True,
    cache_file: str = ".build/fingerprint_cache.json",
    cache_verbose: bool = False,
) -> int:
    """Run the example compilation test using enhanced simple build system."""
    try:
        # Create configuration
        config = CompilationTestConfig(
            specific_examples=specific_examples,
            clean_build=clean_build,
            disable_pch=disable_pch,
            unity_build=unity_build,
            unity_custom_output=unity_custom_output,
            unity_additional_flags=unity_additional_flags,
            full_compilation=full_compilation,
            no_parallel=no_parallel,
            verbose=verbose,
        )

        # Create test runner

        runner = CompilationTestRunner(config)

        # Initialize system and compiler

        compiler, system_info, build_config = runner.initialize_system()

        # Discover examples to compile
        ino_files = runner.discover_examples(compiler)

        # Analyze PCH compatibility and configuration
        pch_compatible_files, pch_incompatible_files, config_parts = (
            runner.analyze_pch_compatibility(compiler, ino_files)
        )

        # Setup PCH if needed
        runner.setup_pch(compiler, pch_compatible_files)

        # Dump compiler information

        compiler_args = compiler.get_compiler_args()

        if config.verbose:
            print(f"\n[VERBOSE] Compiler configuration:")
            print(f"[VERBOSE] Base compiler command: {' '.join(compiler_args[:3])}")
            print(f"[VERBOSE] Compiler flags ({len(compiler_args) - 3} total):")
            for i, arg in enumerate(compiler_args[3:], 1):
                print(f"[VERBOSE]   {i:2d}. {arg}")
            print()

        # Compile examples
        results = runner.compile_examples(
            compiler,
            ino_files,
            pch_compatible_files,
            enable_fingerprint_cache=enable_fingerprint_cache,
            cache_file=cache_file,
            cache_verbose=cache_verbose,
        )

        # Handle linking if requested
        results = runner.handle_linking(compiler, results)

        # Handle execution if requested (after linking)
        results = runner.handle_execution(results)

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
        "--no-cache",
        action="store_true",
        help="Disable cache for compilation (cache disabled by default).",
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
    parser.add_argument(
        "--no-parallel",
        action="store_true",
        help="Disable parallel compilation - useful for debugging or single-threaded environments",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose output showing detailed compilation commands and results",
    )
    parser.add_argument(
        "--no-fingerprint-cache",
        action="store_true",
        help="Disable fingerprint cache (cache is enabled by default for faster incremental builds)",
    )
    parser.add_argument(
        "--cache-file",
        type=str,
        default=".build/fingerprint_cache.json",
        help="Path to fingerprint cache file (default: .build/fingerprint_cache.json)",
    )
    parser.add_argument(
        "--cache-verbose",
        action="store_true",
        help="Enable verbose cache output showing which files are skipped or compiled",
    )

    args = parser.parse_args()

    # Pass specific examples to the test function
    specific_examples = args.examples if args.examples else None

    sys.exit(
        run_example_compilation_test(
            specific_examples,
            args.clean,
            disable_pch=args.no_pch,
            unity_build=args.unity,
            unity_custom_output=args.custom_output,
            unity_additional_flags=args.additional_flags,
            full_compilation=args.full,
            no_parallel=args.no_parallel,
            verbose=args.verbose,
            enable_fingerprint_cache=not args.no_fingerprint_cache,
            cache_file=args.cache_file,
            cache_verbose=args.cache_verbose,
        )
    )
