#!/usr/bin/env python3
"""
Wrapper script that ensures sccache trampolines are built before using them.
This is called by Meson's native file to ensure wrappers exist.
"""

import sys
import subprocess
from pathlib import Path

def main():
    script_dir = Path(__file__).parent
    build_script = script_dir / "build_wrappers.py"
    
    # Build wrappers if needed
    result = subprocess.run([sys.executable, str(build_script)], capture_output=True, text=True)
    
    if result.returncode != 0:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)
    
    # Forward to the actual wrapper
    wrapper_type = sys.argv[1] if len(sys.argv) > 1 else "cxx"
    wrapper_ext = ".exe" if sys.platform == "win32" else ""
    wrapper_path = script_dir / f"zig-{wrapper_type}-wrapper{wrapper_ext}"
    
    if not wrapper_path.exists():
        print(f"Error: Wrapper {wrapper_path} not found after build", file=sys.stderr)
        sys.exit(1)
    
    # Execute the wrapper with remaining arguments
    import os
    os.execv(str(wrapper_path), [str(wrapper_path)] + sys.argv[2:])

if __name__ == "__main__":
    main()
