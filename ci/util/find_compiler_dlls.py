#!/usr/bin/env python3
"""Detect compiler DLL directory for runtime linking."""

import os
import subprocess
import sys
from pathlib import Path


def find_compiler_dll_dir(
    compiler: str = "clang-tool-chain-sccache-cpp",
) -> Path | None:
    """
    Find the DLL directory for the given compiler.

    For clang-tool-chain on Windows, this is typically:
    ~/.clang-tool-chain/clang/win/x86_64/x86_64-w64-mingw32/bin/

    Returns None if not found or not applicable.
    """
    # Only relevant on Windows
    if sys.platform != "win32":
        return None

    # Try to find compiler executable
    try:
        result = subprocess.run(
            ["where", compiler],
            capture_output=True,
            text=True,
            check=True,
            timeout=5,
        )
        compiler_path = Path(result.stdout.strip().split("\n")[0])
    except (
        subprocess.CalledProcessError,
        subprocess.TimeoutExpired,
        FileNotFoundError,
    ):
        # Fallback: Try common installation path
        home = Path.home()
        candidate = home / ".clang-tool-chain/clang/win/x86_64/x86_64-w64-mingw32/bin"
        if candidate.exists():
            return candidate
        return None

    # For clang-tool-chain, the wrapper is in Python scripts directory
    # The actual DLLs are in ~/.clang-tool-chain/clang/win/x86_64/x86_64-w64-mingw32/bin/
    home = Path.home()
    dll_dir = home / ".clang-tool-chain/clang/win/x86_64/x86_64-w64-mingw32/bin"

    if dll_dir.exists():
        return dll_dir

    return None


def get_required_dlls() -> list[str]:
    """
    Return list of DLL names that may be needed by ASAN-instrumented binaries.

    These are transitive dependencies of libclang_rt.asan_dynamic-x86_64.dll.
    """
    return [
        "libc++.dll",  # LLVM C++ standard library
        "libunwind.dll",  # LLVM unwinding library
        "libwinpthread-1.dll",  # POSIX threads for Windows
        "libomp.dll",  # OpenMP runtime (may be needed)
    ]


if __name__ == "__main__":
    dll_dir = find_compiler_dll_dir()
    if dll_dir:
        print(str(dll_dir))
        sys.exit(0)
    else:
        sys.exit(1)
