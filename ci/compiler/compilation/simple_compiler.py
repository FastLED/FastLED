"""Simple compilation mode for individual .ino and .cpp files."""

import os
import time
from concurrent.futures import Future, as_completed
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set

from ci.compiler.clang_compiler import Compiler, Result
from ci.compiler.utils.config import CompilationResult


# Maximum failures before aborting to prevent log flooding
MAX_FAILURES_BEFORE_ABORT = 3

# Timeout settings
_IS_GITHUB_ACTIONS = os.getenv("GITHUB_ACTIONS") == "true"
_TIMEOUT = 600 if _IS_GITHUB_ACTIONS else 120


def compile_examples_simple(
    compiler: Compiler,
    ino_files: List[Path],
    pch_compatible_files: Set[Path],
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

            # Determine PCH compatibility for this specific .cpp file
            use_pch_for_cpp = cpp_file in pch_compatible_files

            if verbose:
                pch_status = "with PCH" if use_pch_for_cpp else "direct compilation"
                print(
                    f"[VERBOSE] Compiling {cpp_file.relative_to(Path('examples'))} ({pch_status})"
                )
                log_timing(
                    f"[VERBOSE] Compiling {cpp_file.relative_to(Path('examples'))} ({pch_status})"
                )

            cpp_future = compiler.compile_cpp_file(
                cpp_file,
                output_path=cpp_output_path,
                use_pch_for_this_file=use_pch_for_cpp,
                additional_flags=additional_flags,
            )
            future_to_file[cpp_future] = cpp_file

            if use_pch_for_cpp:
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
