#!/usr/bin/env python3
"""
Custom IWYU wrapper that properly forwards compiler arguments.

The clang-tool-chain-iwyu wrapper has broken argument parsing that treats
compiler flags like -I, -D, -std, etc. as file paths instead of forwarding
them to the underlying compiler.

This wrapper correctly handles the argument separation and invokes IWYU
with the proper command structure.

Usage:
    iwyu_wrapper.py [iwyu-args] -- compiler [compiler-args] source.cpp
"""

import shutil
import subprocess
import sys
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


def find_iwyu_binary() -> str | None:
    """Find the actual include-what-you-use binary."""
    # Try system IWYU first (preferred)
    iwyu = shutil.which("include-what-you-use")
    if iwyu:
        return iwyu

    # Try clang-tool-chain cache directory
    home = Path.home()
    cache_dir = home / ".clang-tool-chain" / "iwyu"

    # Search for include-what-you-use.exe in cache
    if cache_dir.exists():
        for path in cache_dir.rglob("include-what-you-use.exe"):
            if path.is_file():
                return str(path)
        for path in cache_dir.rglob("include-what-you-use"):
            if path.is_file():
                return str(path)

    return None


def get_compiler_include_paths(compiler_path: str) -> list[str]:
    """
    Extract C++ stdlib include paths from the compiler.

    This queries the compiler for its default include search paths
    and returns them as a list of -I flags for IWYU.
    """
    try:
        # Query compiler for its include paths
        # Using -E (preprocess only), -x c++ (C++ mode), -v (verbose), - (stdin)
        result = subprocess.run(
            [compiler_path, "-E", "-x", "c++", "-v", "-"],
            input="",
            capture_output=True,
            text=True,
            timeout=10,
        )

        # Parse stderr for include paths
        # Format:
        # #include <...> search starts here:
        #  /path/to/include1
        #  /path/to/include2
        # End of search list.
        paths: list[str] = []
        in_includes = False

        for line in result.stderr.splitlines():
            if "#include <...> search starts here:" in line:
                in_includes = True
                continue
            if "End of search list" in line:
                break
            if in_includes:
                path = line.strip()
                # Verify path exists and is a directory
                if path and Path(path).exists() and Path(path).is_dir():
                    paths.append(f"-I{path}")

        return paths
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise  # MUST re-raise KeyboardInterrupt to allow user to stop execution
    except Exception as e:
        print(
            f"Warning: Failed to extract compiler include paths: {e}", file=sys.stderr
        )
        return []


def main():
    # Find IWYU binary
    iwyu_binary = find_iwyu_binary()
    if not iwyu_binary:
        print("ERROR: include-what-you-use binary not found", file=sys.stderr)
        print("Install via: pip install clang-tool-chain", file=sys.stderr)
        sys.exit(1)

    # Parse arguments
    # Format: iwyu_wrapper.py [iwyu-specific-args] -- compiler [compiler-args] source.cpp
    # IWYU expects: include-what-you-use [iwyu-opts] [compiler-args] source.cpp
    # The compiler itself is not passed to IWYU - IWYU uses its own Clang frontend

    args = sys.argv[1:]
    compiler_executable: str | None = None
    compiler_args: list[str] = []
    iwyu_specific_args: list[str] = []

    # Split at '--' separator if present
    if "--" in args:
        separator_idx = args.index("--")
        iwyu_specific_args = args[:separator_idx]
        # Skip the compiler path after '--' and take the rest
        # Format after '--': compiler_path [compiler_args...]
        if len(args) > separator_idx + 1:
            # Skip compiler executable, take only its arguments
            compiler_and_args = args[separator_idx + 1 :]
            # First element after '--' is the compiler path, rest are compiler args
            if len(compiler_and_args) > 0:
                # Save compiler path for querying include paths
                compiler_executable = compiler_and_args[0]
                # Skip the compiler path itself (IWYU doesn't need it)
                compiler_args = compiler_and_args[1:]
            else:
                compiler_args = []
        else:
            compiler_args = []
    else:
        # No separator - all args go to IWYU
        iwyu_specific_args = []
        compiler_args = args

    # Extract include paths from the actual compiler
    # This ensures IWYU can find stdlib headers like <initializer_list>
    extra_includes: list[str] = []
    if compiler_executable:
        extra_includes = get_compiler_include_paths(compiler_executable)

    # Strip PCH flags from compiler_args since IWYU doesn't support PCH
    # Remove: -include-pch path -Werror=invalid-pch -fpch-validate-input-files-content
    filtered_args: list[str] = []
    skip_next = False
    for i, arg in enumerate(compiler_args):
        if skip_next:
            skip_next = False
            continue
        if arg == "-include-pch":
            # Skip this flag and the next argument (the path to PCH file)
            skip_next = True
            continue
        if arg in ("-Werror=invalid-pch", "-fpch-validate-input-files-content"):
            # Skip these PCH-related flags
            continue
        filtered_args.append(arg)

    # Add FastLED-specific IWYU mapping files
    script_dir = Path(__file__).parent
    fastled_mapping = script_dir / "iwyu" / "fastled.imp"
    stdlib_mapping = script_dir / "iwyu" / "stdlib.imp"

    mapping_args: list[str] = []
    if fastled_mapping.exists():
        mapping_args.extend(["-Xiwyu", f"--mapping_file={fastled_mapping}"])
    if stdlib_mapping.exists():
        mapping_args.extend(["-Xiwyu", f"--mapping_file={stdlib_mapping}"])

    # Build IWYU command
    # IWYU format: include-what-you-use [iwyu-opts] [compiler-args] source.cpp
    # Add --driver-mode=g++ to help IWYU find C++ stdlib headers
    driver_mode = ["--driver-mode=g++"]
    iwyu_cmd = (
        [iwyu_binary]
        + driver_mode
        + mapping_args
        + iwyu_specific_args
        + extra_includes
        + filtered_args
    )

    # Run IWYU
    # IWYU writes suggestions to stderr, actual errors to stderr
    # Exit code 0 = no suggestions, non-zero = suggestions or errors
    iwyu_result = subprocess.run(iwyu_cmd)

    # CRITICAL: Also run the actual compiler to produce .obj files
    # The build system expects .obj files to exist for linking, but IWYU only analyzes code.
    # We must invoke the actual compiler after IWYU analysis to create object files.
    # Note: We ALWAYS run the compiler, even if IWYU had suggestions (non-zero exit).
    # IWYU suggestions are informational, but the build must complete.
    compile_returncode = 0
    if compiler_executable and compiler_args:
        # Run the actual compilation
        # Use the original compiler with original arguments (including PCH if present)
        compile_cmd = [compiler_executable] + compiler_args
        compile_result = subprocess.run(compile_cmd)
        compile_returncode = compile_result.returncode

    # Return compilation error if compilation failed (most critical)
    # Otherwise return IWYU result (suggestions are useful but non-blocking)
    if compile_returncode != 0:
        sys.exit(compile_returncode)

    sys.exit(iwyu_result.returncode)


if __name__ == "__main__":
    main()
