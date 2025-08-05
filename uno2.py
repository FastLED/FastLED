#!/usr/bin/env python3
"""
FastLED UNO Target Build Script

Convenience script to run a full build test on the UNO target using
the new PlatformIO testing system.
"""

import sys
import subprocess
import shutil
from pathlib import Path
from typing import Tuple

def resolve_project_root() -> Path:
    """Resolve the FastLED project root directory."""
    current = Path(__file__).parent.resolve()
    while current != current.parent:
        if (current / "src" / "FastLED.h").exists():
            return current
        current = current.parent
    raise RuntimeError("Could not find FastLED project root")

def run_uno_build(example: str = "Blink", verbose: bool = False) -> Tuple[bool, str]:
    """Run UNO build for specified example using new PlatformIO system."""
    
    try:
        project_root = resolve_project_root()
        
        # Set up build directory
        build_dir = project_root / ".build" / "test_platformio" / "uno"
        build_dir.mkdir(parents=True, exist_ok=True)
        
        # Configure example source
        example_path = project_root / "examples" / example
        if not example_path.exists():
            return False, f"Example not found: {example_path}"
        
        # Copy example source to src directory (PlatformIO requirement)
        src_dir = build_dir / "src"
        if src_dir.exists():
            shutil.rmtree(src_dir)
        src_dir.mkdir(parents=True, exist_ok=True)
        
        # Copy all files from example directory
        for file_path in example_path.iterdir():
            if file_path.is_file():
                shutil.copy2(file_path, src_dir)
        
        # Create lib directory and copy FastLED library
        lib_dir = build_dir / "lib" / "FastLED"
        if lib_dir.exists():
            shutil.rmtree(lib_dir)
        lib_dir.mkdir(parents=True, exist_ok=True)
        
        # Copy FastLED source files
        fastled_src_path = project_root / "src"
        shutil.copytree(fastled_src_path, lib_dir, dirs_exist_ok=True)
        
        # Copy library.json to the FastLED lib directory
        library_json_src = project_root / "library.json"
        library_json_dst = lib_dir / "library.json"
        shutil.copy2(library_json_src, library_json_dst)
        
        # Generate platformio.ini
        platformio_content = f"""[env:uno]
platform = atmelavr
board = uno
framework = arduino

# LDF Configuration
lib_ldf_mode = deep+
lib_compat_mode = off
"""
        
        platformio_ini = build_dir / "platformio.ini"
        platformio_ini.write_text(platformio_content)
        
        # Run pio build
        cmd = ["pio", "run", "--project-dir", str(build_dir)]
        if verbose:
            cmd.append("--verbose")
        
        print(f"Building {example} for UNO target...")
        print(f"Build directory: {build_dir}")
        print(f"Command: {' '.join(cmd)}")
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"✅ Build successful for {example}")
            if verbose:
                print("STDOUT:", result.stdout)
            return True, result.stdout
        else:
            print(f"❌ Build failed for {example}")
            print("STDERR:", result.stderr)
            if result.stdout:
                print("STDOUT:", result.stdout)
            return False, result.stderr
            
    except Exception as e:
        return False, f"Build error: {e}"

def main() -> int:
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(description="FastLED UNO Target Build Script")
    parser.add_argument("--example", default="Blink", 
                       help="Example to build (default: Blink)")
    parser.add_argument("--verbose", action="store_true",
                       help="Enable verbose output")
    
    args = parser.parse_args()
    
    success, output = run_uno_build(args.example, args.verbose)
    
    if success:
        if args.verbose:
            print(output)
        return 0
    else:
        print(f"Error: {output}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
