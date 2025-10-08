"""System information utilities."""

import os
import platform
from typing import Dict, Union

import psutil


def get_system_info() -> Dict[str, Union[str, int, float]]:
    """Get basic system configuration information.

    OPTIMIZED: Removed expensive subprocess calls for compiler detection
    since we know the build system uses Zig/Clang after migration.
    """
    try:
        # Get CPU information
        cpu_count = os.cpu_count() or 1

        # Get memory information
        memory = psutil.virtual_memory()
        memory_gb = memory.total / (1024**3)

        # Get OS information
        os_name = platform.system()
        os_version = platform.release()

        # Use known compiler info since we're post-migration to Zig/Clang
        # No expensive subprocess calls needed
        compiler_info = "Zig/Clang (known)"

        return {
            "os": f"{os_name} {os_version}",
            "compiler": compiler_info,
            "cpu_cores": cpu_count,
            "memory_gb": memory_gb,
        }
    except Exception as e:
        return {
            "os": f"{platform.system()} {platform.release()}",
            "compiler": "Zig/Clang (fallback)",
            "cpu_cores": os.cpu_count() or 1,
            "memory_gb": 8.0,  # Fallback estimate
        }


def get_build_configuration() -> Dict[str, Union[bool, str]]:
    """Get build configuration information."""
    config: Dict[str, Union[bool, str]] = {}

    # Check unified compilation (note: simple build system uses direct compilation)
    config["unified_compilation"] = False  # Simple build system uses direct compilation

    # Compiler cache disabled
    config["cache_type"] = "none"

    # Build mode for simple build system
    config["build_mode"] = "simple_direct"

    return config


def format_file_size(size_bytes: int) -> str:
    """Format file size in human readable format."""
    if size_bytes == 0:
        return "0B"

    units = ["B", "KB", "MB", "GB"]
    size = float(size_bytes)
    unit_index = 0

    while size >= 1024 and unit_index < len(units) - 1:
        size /= 1024
        unit_index += 1

    if unit_index == 0:
        return f"{int(size)}{units[unit_index]}"
    else:
        return f"{size:.1f}{units[unit_index]}"
