#!/usr/bin/env python3
"""
Python-based cached compiler system for sccache integration.

This module provides Python scripts that act as cached compilers, wrapping the real
toolchain with cache support. Unlike batch scripts, Python wrappers can:
- Properly resolve toolchain paths from PlatformIO platform packages
- Handle response files and complex argument parsing
- Provide better error handling and debugging
- Work consistently across platforms (Windows/Unix)
"""

import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Dict, List, Optional


def find_toolchain_compiler(
    compiler_name: str, platform_packages_paths: List[str]
) -> Optional[str]:
    """
    Find the real compiler binary in PlatformIO platform packages.

    Args:
        compiler_name: Name of compiler (e.g., 'arm-none-eabi-gcc', 'gcc', 'clang')
        platform_packages_paths: List of paths to search for toolchain

    Returns:
        Absolute path to compiler binary if found, None otherwise
    """
    # First check if compiler is already an absolute path
    if Path(compiler_name).is_absolute() and Path(compiler_name).exists():
        return str(Path(compiler_name).resolve())

    # Check if compiler is in PATH
    path_compiler = shutil.which(compiler_name)
    if path_compiler:
        return str(Path(path_compiler).resolve())

    # Search in platform packages directories
    for package_path in platform_packages_paths:
        package_dir = Path(package_path)
        if not package_dir.exists():
            continue

        # Search for compiler in common toolchain subdirectories
        search_patterns = [
            f"bin/{compiler_name}",
            f"bin/{compiler_name}.exe",
            f"**/bin/{compiler_name}",
            f"**/bin/{compiler_name}.exe",
        ]

        for pattern in search_patterns:
            compiler_candidates = list(package_dir.glob(pattern))
            for candidate in compiler_candidates:
                if candidate.is_file():
                    return str(candidate.resolve())

    return None


def create_cached_compiler_script(
    compiler_name: str,
    cache_executable: str,
    real_compiler_path: str,
    output_dir: Path,
    debug: bool = False,
) -> Path:
    """
    Create a Python script that acts as a cached compiler.

    Args:
        compiler_name: Name for the cached compiler (e.g., 'gcc', 'g++')
        cache_executable: Path to cache tool (sccache, ccache, etc.)
        real_compiler_path: Path to the real compiler to wrap
        output_dir: Directory to create the cached compiler script
        debug: Enable debug output

    Returns:
        Path to the created cached compiler script
    """
    script_base = f"cached_{compiler_name.replace('-', '_')}"
    script_py = output_dir / f"{script_base}.py"

    script_content = f'''#!/usr/bin/env python3
"""
Cached compiler wrapper for {compiler_name}
Generated automatically for sccache integration.

This script acts as a cached compiler that wraps the real {compiler_name}
with cache support. It resolves the real compiler path and forwards
all arguments to the cache tool.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

# Configuration (set at generation time)
CACHE_EXECUTABLE = r"{cache_executable}"
REAL_COMPILER_PATH = r"{real_compiler_path}"
DEBUG = {debug}

def debug_print(msg: str) -> None:
    """Print debug message if debug mode is enabled."""
    if DEBUG:
        print(f"CACHED_COMPILER[{compiler_name}]: {{msg}}", file=sys.stderr)

def main() -> int:
    """Main entry point for cached compiler."""
    try:
        # Verify real compiler exists
        if not Path(REAL_COMPILER_PATH).exists():
            print(f"ERROR: Real compiler not found: {{REAL_COMPILER_PATH}}", file=sys.stderr)
            return 1
        
        # Verify cache executable exists
        cache_path = CACHE_EXECUTABLE
        if not Path(cache_path).exists() and not shutil.which(cache_path.split()[0]):
            print(f"ERROR: Cache executable not found: {{cache_path}}", file=sys.stderr)
            return 1
        
        # Build command: cache_tool real_compiler args...
        if " " in cache_path:
            # Handle "python xcache.py" style commands
            cache_parts = cache_path.split()
            command = cache_parts + [REAL_COMPILER_PATH] + sys.argv[1:]
        else:
            # Handle simple "sccache" style commands
            command = [cache_path, REAL_COMPILER_PATH] + sys.argv[1:]
        
        debug_print(f"Executing: {{' '.join(command)}}")
        
        # Execute the cache tool with real compiler and arguments
        result = subprocess.run(command, cwd=os.getcwd())
        return result.returncode
    except KeyboardInterrupt:
        print(f"Keyboard interrupt in cached compiler {{compiler_name}}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"ERROR in cached compiler {{compiler_name}}: {{e}}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
'''

    # Write the script
    script_py.write_text(script_content, encoding="utf-8")

    # Detect Windows reliably, including MSYS/Cygwin Git-Bash environments where
    # os.name may report 'posix'. In those cases we still need a .cmd shim so
    # that response files ("@file.tmp") are not misinterpreted by 'python'.
    is_windows = (
        os.name == "nt"
        or sys.platform.startswith("win")
        or sys.platform.startswith("cygwin")
        or sys.platform.startswith("msys")
        or platform.system().lower().startswith(("windows", "msys", "cygwin"))
    )

    # Make executable script on Unix-like systems
    if not is_windows:
        script_py.chmod(script_py.stat().st_mode | 0o755)
        return script_py

    # On Windows, create a .cmd shim so our wrapper is the program token
    script_cmd = output_dir / f"{script_base}.cmd"
    # Use system python; PlatformIO environment provides the right interpreter
    cmd_content = f'@echo off\r\npython "{script_py}" %*\r\nexit /b %ERRORLEVEL%\r\n'
    script_cmd.write_text(cmd_content, encoding="utf-8")
    return script_cmd


def create_cached_toolchain(
    toolchain_info: Dict[str, str],
    cache_config: Dict[str, str],
    platform_packages_paths: List[str],
    output_dir: Path,
    debug: bool = False,
) -> Dict[str, str]:
    """
    Create a complete set of cached compiler scripts for a toolchain.

    Args:
        toolchain_info: Mapping of tool names to real paths/names
        cache_config: Cache configuration (CACHE_EXECUTABLE, etc.)
        platform_packages_paths: Paths to search for real compilers
        output_dir: Directory to create cached scripts
        debug: Enable debug output

    Returns:
        Mapping of tool names to cached script paths
    """
    output_dir.mkdir(parents=True, exist_ok=True)

    cache_executable = cache_config.get("CACHE_EXECUTABLE", "sccache")
    cached_tools: Dict[str, str] = {}

    # Standard compiler tools that should be wrapped with cache
    cacheable_tools = {"CC", "CXX", "gcc", "g++", "clang", "clang++"}

    for tool_name, tool_path_or_name in toolchain_info.items():
        # Only wrap cacheable tools
        tool_base = Path(tool_path_or_name).name if tool_path_or_name else tool_name
        if not any(cacheable in tool_base for cacheable in cacheable_tools):
            if debug:
                print(f"Skipping non-cacheable tool: {tool_name} = {tool_path_or_name}")
            continue

        # Find the real compiler
        real_compiler_path = find_toolchain_compiler(
            tool_path_or_name, platform_packages_paths
        )
        if not real_compiler_path:
            if debug:
                print(
                    f"WARNING: Could not find real compiler for {tool_name} = {tool_path_or_name}"
                )
            continue

        # Create cached compiler script
        cached_script = create_cached_compiler_script(
            compiler_name=tool_name,
            cache_executable=cache_executable,
            real_compiler_path=real_compiler_path,
            output_dir=output_dir,
            debug=debug,
        )

        # Use returned script path directly (Windows: .cmd shim; Unix: executable .py)
        cached_tools[tool_name] = str(cached_script)

        if debug:
            print(
                f"Created cached {tool_name}: {cached_tools[tool_name]} -> {real_compiler_path}"
            )

    return cached_tools


def get_platform_packages_paths() -> List[str]:
    """
    Get list of platform package paths from PlatformIO.

    Returns:
        List of paths where platform packages are installed
    """
    paths: List[str] = []

    # Try to get PlatformIO home directory
    try:
        result = subprocess.run(
            ["pio", "system", "info", "--json"],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode == 0:
            import json

            info = json.loads(result.stdout)
            platformio_home = info.get("platformio_home_dir")
            if platformio_home:
                packages_dir = Path(platformio_home) / "packages"
                if packages_dir.exists():
                    # Add all package directories
                    for package_dir in packages_dir.iterdir():
                        if package_dir.is_dir():
                            paths.append(str(package_dir))
    except KeyboardInterrupt:
        print(f"Keyboard interrupt in get_platform_packages_paths")
        import sys

        sys.exit(1)
    except Exception:
        pass

    # Fallback: common PlatformIO package locations
    common_locations = [
        Path.home() / ".platformio" / "packages",
        Path.home() / ".fastled" / "packages",
        Path("C:/Users") / os.environ.get("USERNAME", "") / ".platformio" / "packages",
        Path("C:/Users") / os.environ.get("USERNAME", "") / ".fastled" / "packages",
    ]

    for location in common_locations:
        if location.exists():
            for package_dir in location.iterdir():
                if package_dir.is_dir():
                    paths.append(str(package_dir))

    return paths


if __name__ == "__main__":
    # Test the cached compiler system
    print("Testing cached compiler system...")

    # Get platform packages
    packages = get_platform_packages_paths()
    print(f"Found {len(packages)} platform packages")

    # Test finding a common compiler
    test_compiler = find_toolchain_compiler("gcc", packages)
    print(f"Found gcc at: {test_compiler}")

    # Create test cached compiler
    test_dir = Path(tempfile.mkdtemp())
    try:
        cached_tools = create_cached_toolchain(
            toolchain_info={"CC": "gcc", "CXX": "g++"},
            cache_config={"CACHE_EXECUTABLE": "sccache"},
            platform_packages_paths=packages,
            output_dir=test_dir,
            debug=True,
        )
        print(f"Created cached tools: {cached_tools}")
    finally:
        shutil.rmtree(test_dir, ignore_errors=True)
