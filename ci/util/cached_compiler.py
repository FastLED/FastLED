#!/usr/bin/env python3
"""
Cached compiler utilities for FastLED build optimization.

This module provides functions for finding compilers, creating cached compiler scripts,
and managing platform packages for the build optimization system.
"""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Optional

from ci.util.boards import Board


def get_platform_packages_paths() -> List[str]:
    """Get platform packages paths for toolchain resolution."""
    # This is an expensive operation that searches for PlatformIO packages
    platform_packages: List[str] = []
    
    # Check common PlatformIO package locations
    home_dir = Path.home()
    possible_paths = [
        home_dir / ".platformio" / "packages",
        home_dir / ".fastled" / "packages",
        Path("/opt/platformio/packages"),
        Path("/usr/local/platformio/packages"),
    ]
    
    # Also check environment variables
    pio_home = os.environ.get("PLATFORMIO_HOME_DIR")
    if pio_home:
        possible_paths.append(Path(pio_home) / "packages")
    
    pio_packages = os.environ.get("PLATFORMIO_PACKAGES_DIR")  
    if pio_packages:
        possible_paths.append(Path(pio_packages))
    
    # Find all existing package directories
    for path in possible_paths:
        if path.exists() and path.is_dir():
            try:
                # Add all subdirectories that look like toolchain packages
                for subdir in path.iterdir():
                    if subdir.is_dir() and any(keyword in subdir.name.lower() for keyword in 
                                             ["toolchain", "avr", "arm", "gcc", "clang", "compiler"]):
                        platform_packages.append(str(subdir))
            except (OSError, PermissionError):
                continue
    
    return platform_packages


def find_toolchain_compiler(compiler_name: str, platform_packages: List[str]) -> Optional[str]:
    """Find the real compiler executable in the platform packages."""
    
    # First try to find in PATH
    direct_path = shutil.which(compiler_name)
    if direct_path:
        return direct_path
    
    # Search through platform packages
    for package_dir in platform_packages:
        package_path = Path(package_dir)
        if not package_path.exists():
            continue
        
        # Common locations within packages
        search_paths = [
            package_path / "bin",
            package_path / "toolchain" / "bin", 
            package_path / "compiler" / "bin",
        ]
        
        for search_path in search_paths:
            if not search_path.exists():
                continue
                
            # Look for exact match
            exact_match = search_path / compiler_name
            if exact_match.exists() and exact_match.is_file():
                # Check if executable
                if os.access(exact_match, os.X_OK):
                    return str(exact_match)
            
            # Look for executable with common extensions
            for ext in ["", ".exe", ".cmd", ".bat"]:
                candidate = search_path / f"{compiler_name}{ext}"
                if candidate.exists() and candidate.is_file():
                    if os.access(candidate, os.X_OK):
                        return str(candidate)
    
    return None


def create_cached_compiler_script(
    compiler_name: str,
    cache_executable: str,
    real_compiler_path: str,
    output_dir: Path,
    debug: bool = False,
) -> Path:
    """Create a cached compiler script that wraps the real compiler with caching."""
    
    script_name = f"cached_{compiler_name.replace('-', '_')}.py"
    script_path = output_dir / script_name
    
    script_content = f'''#!/usr/bin/env python3
"""
Cached compiler wrapper for {compiler_name}
Generated automatically for FastLED build caching.
"""

import os
import subprocess
import sys

# Configuration
CACHE_EXECUTABLE = r"{cache_executable}"
REAL_COMPILER = r"{real_compiler_path}"
DEBUG = {debug}

def main() -> int:
    """Execute compilation with caching."""
    if not CACHE_EXECUTABLE:
        # No cache available, use real compiler directly
        command = [REAL_COMPILER] + sys.argv[1:]
    else:
        # Use cache wrapper
        command = [CACHE_EXECUTABLE, REAL_COMPILER] + sys.argv[1:]
    
    if DEBUG:
        print(f"Cached compiler executing: {{' '.join(command)}}", file=sys.stderr)
    
    try:
        result = subprocess.run(command, cwd=os.getcwd())
        return result.returncode
    except FileNotFoundError as e:
        print(f"Error: Could not execute {{e}}", file=sys.stderr)
        return 127
    except Exception as e:
        print(f"Error in cached compiler: {{e}}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
'''
    
    # Write the script
    script_path.write_text(script_content, encoding="utf-8")
    
    # Make executable on Unix systems
    if os.name != "nt":
        script_path.chmod(script_path.stat().st_mode | 0o755)
    
    return script_path


def create_cached_toolchain(
    toolchain_info: Dict[str, str],
    cache_executable: str,
    platform_packages: List[str],
    output_dir: Path,
    debug: bool = False,
) -> Optional[Dict[str, str]]:
    """Create a complete cached toolchain from toolchain info."""
    
    output_dir.mkdir(parents=True, exist_ok=True)
    cached_tools: Dict[str, str] = {}
    
    # Standard compiler tools that should be cached
    cacheable_tools = {"CC", "CXX", "gcc", "g++", "clang", "clang++"}
    
    for tool_name, tool_path_or_name in toolchain_info.items():
        # Only cache compiler tools
        tool_base = Path(tool_path_or_name).name if tool_path_or_name else tool_name
        if not any(cacheable in tool_base for cacheable in cacheable_tools):
            if debug:
                print(f"Skipping non-cacheable tool: {tool_name} = {tool_path_or_name}")
            continue
        
        # Find the real compiler
        real_compiler_path = find_toolchain_compiler(tool_path_or_name, platform_packages)
        if not real_compiler_path:
            print(f"Warning: Could not find real compiler for {tool_name}: {tool_path_or_name}")
            continue
        
        # Create cached compiler script
        cached_script = create_cached_compiler_script(
            compiler_name=tool_name,
            cache_executable=cache_executable,
            real_compiler_path=real_compiler_path,
            output_dir=output_dir,
            debug=debug,
        )
        
        # Use Python to execute the script
        cached_tools[tool_name] = f"python {cached_script}"
        
        if debug:
            print(f"Created cached {tool_name}: {cached_tools[tool_name]}")
    
    return cached_tools if cached_tools else None