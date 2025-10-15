#!/usr/bin/env python3
"""Zig archiver wrapper for Meson build system"""

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

    # Filter out thin archive flag 'T' from ar arguments
    # Zig's linker doesn't support thin archives, so we convert them to regular archives
    args = []
    for arg in sys.argv[1:]:
        # If this is an ar flag argument (starts with 'cr' or similar), remove 'T'
        if arg.startswith('cr') or arg.startswith('rc'):
            # Remove 'T' flag which creates thin archives
            arg = arg.replace('T', '')
        args.append(arg)

    result = subprocess.run(
        [python_exe, "-m", "ziglang", "ar"] + args
    )
    sys.exit(result.returncode)
