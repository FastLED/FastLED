#!/usr/bin/env python3
"""Find the LLVM bin directory in .cache/cc installation."""

import sys
from pathlib import Path


def find_llvm_bin() -> int:
    """Find and print the LLVM bin directory path."""
    project_root = Path(__file__).parent.parent
    cache_cc = project_root / ".cache" / "cc"

    if not cache_cc.exists():
        print(f"Error: {cache_cc} does not exist", file=sys.stderr)
        return 1

    # Look for bin directories in extracted LLVM directories
    for item in cache_cc.iterdir():
        if item.is_dir():
            bin_dir = item / "bin"
            if bin_dir.exists():
                # Check if this looks like an LLVM installation
                clang = bin_dir / "clang"
                clang_exe = bin_dir / "clang.exe"
                if clang.exists() or clang_exe.exists():
                    # Print the bin directory path
                    print(bin_dir)
                    return 0

    print(f"Error: No LLVM bin directory found in {cache_cc}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(find_llvm_bin())
