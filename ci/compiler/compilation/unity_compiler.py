"""Unity build compilation mode for all examples in a single unit."""

import tempfile
import time
from pathlib import Path
from typing import Callable, List, Optional

from ci.compiler.clang_compiler import Compiler, CompilerOptions
from ci.compiler.utils.config import CompilationResult


def compile_examples_unity(
    compiler: Compiler,
    ino_files: List[Path],
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
    all_cpp_files: List[Path] = []

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

        log_timing(f"[UNITY] UNITY build exception: {e}")

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
