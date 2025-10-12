#!/usr/bin/env python3
"""Zig C compiler wrapper for Meson build system"""

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

    result = subprocess.run(
        [python_exe, "-m", "ziglang", "cc"] + sys.argv[1:]
    )
    sys.exit(result.returncode)
