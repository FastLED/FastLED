#!/usr/bin/env python3
"""
Pre-build setup script for FastLED.
Ensures all build dependencies (like sccache wrappers) are in place.
"""

import sys
import subprocess
from pathlib import Path

def ensure_sccache_wrappers():
    """Ensure sccache wrapper trampolines are built."""
    script_dir = Path(__file__).parent / ".meson"
    build_script = script_dir / "build_wrappers.py"
    
    if not build_script.exists():
        print("Warning: build_wrappers.py not found, skipping wrapper build")
        return True
    
    print("Ensuring sccache wrappers are built...")
    result = subprocess.run([sys.executable, str(build_script)])
    
    return result.returncode == 0

def main():
    """Run all pre-build setup tasks."""
    tasks = [
        ("Build sccache wrappers", ensure_sccache_wrappers),
    ]
    
    for task_name, task_func in tasks:
        if not task_func():
            print(f"✗ Failed: {task_name}")
            return 1
    
    print("\n✓ Build setup complete")
    return 0

if __name__ == "__main__":
    sys.exit(main())
