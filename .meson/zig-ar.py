#!/usr/bin/env python3
"""Zig archiver wrapper for Meson build system - filters thin archive flags for Zig compatibility"""

import sys
import os
import subprocess

if __name__ == "__main__":
    # When VIRTUAL_ENV is set, we're running under 'uv run' or an activated venv
    # In this case, sys.executable points to the venv Python with ziglang available
    if os.environ.get("VIRTUAL_ENV"):
        python_exe = sys.executable
    else:
        # Fallback to system python when not in a virtual environment
        python_exe = "C:/tools/python13/python.exe"

    # Filter out thin archive flags from arguments
    # Zig's linker doesn't support thin archives, so we must remove the T flag
    args = []
    for arg in sys.argv[1:]:
        # Skip --thin flag entirely
        if arg == "--thin" or arg == "-T":
            continue

        # Remove T from operation flags like csrDT, crT, rcT, Tcr, etc.
        # Operation flags are single args containing only ar flag letters
        # Common ar flags: c r s q t p a d i b D T U V u
        ar_flag_chars = set("crsqtpadibDTUVu")
        if len(arg) <= 10 and arg and set(arg).issubset(ar_flag_chars) and "T" in arg:
            # This looks like an operation flag containing T - remove T
            arg = arg.replace("T", "")
            # Skip if now empty
            if not arg:
                continue

        args.append(arg)

    result = subprocess.run(
        [python_exe, "-m", "ziglang", "ar"] + args
    )
    sys.exit(result.returncode)
