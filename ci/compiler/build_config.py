"""Build configuration for FastLED PlatformIO builds."""

import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import TYPE_CHECKING, Optional

from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream

from ci.compiler.build_utils import get_utf8_env
from ci.compiler.compiler import CacheType
from ci.util.create_build_dir import insert_tool_aliases
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


if TYPE_CHECKING:
    from ci.boards import Board
    from ci.compiler.path_manager import FastLEDPaths


# Module-level constants
_PROJECT_ROOT: Optional[Path] = None


def _get_project_root() -> Path:
    """Get the cached project root."""
    global _PROJECT_ROOT
    if _PROJECT_ROOT is None:
        from ci.compiler.path_manager import resolve_project_root

        _PROJECT_ROOT = resolve_project_root()
    return _PROJECT_ROOT


def generate_build_info_json_from_existing_build(
    build_dir: Path, board: "Board", example: Optional[str] = None
) -> bool:
    """Generate build_info.json from an existing PlatformIO build.

    Args:
        build_dir: Build directory containing the PlatformIO project
        board: Board configuration
        example: Optional example name for generating example-specific build_info_{example}.json

    Returns:
        True if build_info.json was successfully generated
    """
    try:
        # Use existing project to get metadata with streaming output
        # This prevents the process from appearing to stall during library resolution
        metadata_cmd = ["pio", "project", "metadata", "--json-output"]

        print("Generating build metadata (resolving library dependencies)...")

        metadata_proc = RunningProcess(
            metadata_cmd,
            cwd=build_dir,
            auto_run=True,
            timeout=None,  # No global timeout - rely on per-line timeout instead
            env=get_utf8_env(),
        )

        # Stream output while collecting it for JSON parsing
        # Show progress indicators for long-running operations
        # Use per-line timeout of 15 minutes - some packages take a long time to download
        metadata_lines: list[str] = []
        while line := metadata_proc.get_next_line(timeout=900):
            if isinstance(line, EndOfStream):
                break
            metadata_lines.append(line)
            # Show progress for library resolution operations (not JSON output)
            # JSON output will be a single line starting with '{'
            if not line.strip().startswith("{"):
                # This is a progress message from PlatformIO
                stripped = line.strip()
                if any(
                    keyword in stripped
                    for keyword in [
                        "Resolving",
                        "Installing",
                        "Downloading",
                        "Checking",
                    ]
                ):
                    print(f"  {stripped}")

        metadata_proc.wait()

        if metadata_proc.returncode != 0:
            # Note: RunningProcess merges stderr into stdout
            print(
                f"Warning: Failed to get metadata for build_info.json (exit code {metadata_proc.returncode})"
            )
            return False

        # Parse and save the metadata
        try:
            metadata_output = "".join(metadata_lines)
            data = json.loads(metadata_output)

            # Add tool aliases for symbol analysis and debugging
            insert_tool_aliases(data)

            # Save to build_info.json (example-specific if example provided)
            if example:
                build_info_filename = f"build_info_{example}.json"
            else:
                build_info_filename = "build_info.json"
            build_info_path = build_dir / build_info_filename
            with open(build_info_path, "w") as f:
                json.dump(data, f, indent=4, sort_keys=True)

            print(f"✅ Generated {build_info_filename} at {build_info_path}")
            return True

        except json.JSONDecodeError as e:
            print(f"Warning: Failed to parse metadata JSON for build_info.json: {e}")
            return False

    except TimeoutError:
        print("Warning: Timeout generating build_info.json (no output for 900s)")
        return False
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Warning: Exception generating build_info.json: {e}")
        return False


def apply_board_specific_config(
    board: "Board",
    platformio_ini_path: Path,
    example: str,
    paths: "FastLEDPaths",
    additional_defines: Optional[list[str]] = None,
    additional_include_dirs: Optional[list[str]] = None,
    additional_libs: Optional[list[str]] = None,
    cache_type: CacheType = CacheType.NO_CACHE,
) -> bool:
    """Apply board-specific build configuration from Board class."""
    # Use provided paths object (which may have overrides)
    paths.ensure_directories_exist()

    project_root = _get_project_root()

    # Generate platformio.ini content using the enhanced Board method
    config_content = board.to_platformio_ini(
        additional_defines=additional_defines,
        additional_include_dirs=additional_include_dirs,
        additional_libs=additional_libs,
        include_platformio_section=True,
        core_dir=str(paths.core_dir),
        packages_dir=str(paths.packages_dir),
        project_root=str(project_root),
        build_cache_dir=str(paths.build_cache_dir),
        extra_scripts=(
            ["post:cache_setup.scons"] if cache_type != CacheType.NO_CACHE else None
        ),
    )

    # Apply PlatformIO cache optimization to speed up builds
    try:
        from ci.compiler.platformio_cache import PlatformIOCache
        from ci.compiler.platformio_ini import PlatformIOIni

        # Parse the generated INI content
        pio_ini = PlatformIOIni.parseString(config_content)

        # Set up global PlatformIO cache
        cache = PlatformIOCache(paths.global_platformio_cache_dir)

        # Optimize by downloading and caching packages, replacing URLs with local file:// paths
        pio_ini.optimize(cache)

        # Use the optimized content
        config_content = str(pio_ini)
        print(
            f"Applied PlatformIO cache optimization using cache directory: {paths.global_platformio_cache_dir}"
        )

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        # Graceful fallback to original URLs on cache failures
        print(
            f"Warning: PlatformIO cache optimization failed, using original URLs: {e}"
        )
        # config_content remains unchanged (original URLs)

    platformio_ini_path.write_text(config_content)

    # Log applied configurations for debugging
    if board.build_flags:
        print(f"Applied build_flags: {board.build_flags}")
    if board.defines:
        print(f"Applied defines: {board.defines}")
    if additional_defines:
        print(f"Applied additional defines: {additional_defines}")
    if additional_include_dirs:
        print(f"Applied additional include dirs: {additional_include_dirs}")
    if board.platform_packages:
        print(f"Using platform_packages: {board.platform_packages}")

    return True


def copy_cache_build_script(build_dir: Path, cache_config: dict[str, str]) -> None:
    """Copy the standalone cache setup script and set environment variables for configuration."""
    # Source script location
    project_root = _get_project_root()
    source_script = project_root / "ci" / "compiler" / "cache_setup.scons"
    dest_script = build_dir / "cache_setup.scons"

    # Copy the standalone script
    if not source_script.exists():
        raise RuntimeError(f"Cache setup script not found: {source_script}")

    shutil.copy2(source_script, dest_script)
    print(f"Copied cache setup script: {source_script} -> {dest_script}")

    # Set environment variables for cache configuration
    # These will be read by the cache_setup.scons script
    cache_type = cache_config.get("CACHE_TYPE", "sccache")

    os.environ["FASTLED_CACHE_TYPE"] = cache_type
    os.environ["FASTLED_SCCACHE_DIR"] = cache_config.get("SCCACHE_DIR", "")
    os.environ["FASTLED_SCCACHE_CACHE_SIZE"] = cache_config.get(
        "SCCACHE_CACHE_SIZE", "2G"
    )
    os.environ["FASTLED_CACHE_DEBUG"] = (
        "1" if os.environ.get("XCACHE_DEBUG") == "1" else "0"
    )

    if cache_type == "xcache":
        os.environ["FASTLED_CACHE_EXECUTABLE"] = cache_config.get(
            "CACHE_EXECUTABLE", ""
        )
        os.environ["FASTLED_SCCACHE_PATH"] = cache_config.get("SCCACHE_PATH", "")
        os.environ["FASTLED_XCACHE_PATH"] = cache_config.get("XCACHE_PATH", "")
    elif cache_type == "sccache":
        os.environ["FASTLED_CACHE_EXECUTABLE"] = cache_config.get("SCCACHE_PATH", "")
        os.environ["FASTLED_SCCACHE_PATH"] = cache_config.get("SCCACHE_PATH", "")
    else:
        os.environ["FASTLED_CACHE_EXECUTABLE"] = cache_config.get("CCACHE_PATH", "")

    print(f"Set cache environment variables for {cache_type} configuration")


def get_cache_build_flags(board_name: str, cache_type: CacheType) -> dict[str, str]:
    """Get environment variables for compiler cache configuration."""
    if cache_type == CacheType.NO_CACHE:
        print("No compiler cache configured")
        return {}
    elif cache_type == CacheType.SCCACHE:
        return get_sccache_build_flags(board_name)
    elif cache_type == CacheType.CCACHE:
        return get_ccache_build_flags(board_name)
    else:
        print(f"Unknown cache type: {cache_type}")
        return {}


def get_sccache_build_flags(board_name: str) -> dict[str, str]:
    """Get build flags for SCCACHE configuration with xcache wrapper support."""
    from ci.compiler.path_manager import FastLEDPaths

    # Check if sccache is available
    sccache_path = shutil.which("sccache")
    if not sccache_path:
        print("SCCACHE not found in PATH, compilation will proceed without caching")
        return {}

    print(f"Setting up SCCACHE build flags: {sccache_path}")

    # Set up sccache directory in the global .fastled directory
    # Shared across all boards for maximum cache efficiency
    paths = FastLEDPaths(board_name)
    sccache_dir = paths.fastled_root / "sccache"
    sccache_dir.mkdir(parents=True, exist_ok=True)

    print(f"SCCACHE cache directory: {sccache_dir}")
    print("SCCACHE cache size limit: 2G")

    # Get xcache wrapper path
    project_root = _get_project_root()
    xcache_path = project_root / "ci" / "util" / "xcache.py"

    if xcache_path.exists():
        print(f"Using xcache wrapper for ESP32S3 response file support: {xcache_path}")
        cache_type = "xcache"
        cache_executable_path = f"python {xcache_path}"
    else:
        print(f"xcache not found at {xcache_path}, using direct sccache")
        cache_type = "sccache"
        cache_executable_path = sccache_path

    # Return the cache configuration
    config = {
        "CACHE_TYPE": cache_type,
        "SCCACHE_DIR": str(sccache_dir),
        "SCCACHE_CACHE_SIZE": "2G",
        "SCCACHE_PATH": sccache_path,
        "XCACHE_PATH": str(xcache_path) if xcache_path.exists() else "",
        "CACHE_EXECUTABLE": cache_executable_path,
    }

    return config


def get_ccache_build_flags(board_name: str) -> dict[str, str]:
    """Get environment variables for CCACHE configuration."""
    from ci.compiler.path_manager import FastLEDPaths

    # Check if ccache is available
    ccache_path = shutil.which("ccache")
    if not ccache_path:
        print("CCACHE not found in PATH, compilation will proceed without caching")
        return {}

    print(f"Setting up CCACHE build environment: {ccache_path}")

    # Set up ccache directory in the global .fastled directory
    # Shared across all boards for maximum cache efficiency
    paths = FastLEDPaths(board_name)
    ccache_dir = paths.fastled_root / "ccache"
    ccache_dir.mkdir(parents=True, exist_ok=True)

    print(f"CCACHE cache directory: {ccache_dir}")
    print("CCACHE cache size limit: 2G")

    # Return environment variables that PlatformIO will use
    env_vars = {
        "CACHE_TYPE": "ccache",
        "CCACHE_DIR": str(ccache_dir),
        "CCACHE_MAXSIZE": "2G",
        "CCACHE_PATH": ccache_path,
    }

    return env_vars


def setup_sccache_environment(board_name: str) -> bool:
    """Set up sccache environment variables for the current process."""
    from ci.compiler.path_manager import FastLEDPaths

    # Check if sccache is available
    sccache_path = shutil.which("sccache")
    if not sccache_path:
        print("SCCACHE not found in PATH, compilation will proceed without caching")
        return False

    print(f"Setting up SCCACHE environment: {sccache_path}")

    # Set up sccache directory in the global .fastled directory
    # Shared across all boards for maximum cache efficiency
    paths = FastLEDPaths(board_name)
    sccache_dir = paths.fastled_root / "sccache"
    sccache_dir.mkdir(parents=True, exist_ok=True)

    # Configure sccache environment variables
    os.environ["SCCACHE_DIR"] = str(sccache_dir)
    os.environ["SCCACHE_CACHE_SIZE"] = "2G"

    print(f"SCCACHE cache directory: {sccache_dir}")

    # Set compiler wrapper environment variables that PlatformIO will use
    # PlatformIO respects these environment variables for compiler selection
    original_cc = os.environ.get("CC", "")
    original_cxx = os.environ.get("CXX", "")

    # Only wrap if not already wrapped
    if "sccache" not in original_cc:
        # Set environment variables that PlatformIO/SCons will use
        os.environ["CC"] = (
            f'"{sccache_path}" {original_cc}'
            if original_cc
            else f'"{sccache_path}" gcc'
        )
        os.environ["CXX"] = (
            f'"{sccache_path}" {original_cxx}'
            if original_cxx
            else f'"{sccache_path}" g++'
        )

        print(f"Set CC environment variable: {os.environ['CC']}")
        print(f"Set CXX environment variable: {os.environ['CXX']}")

    print(f"SCCACHE cache directory: {sccache_dir}")
    print("SCCACHE cache size limit: 2G")

    # Show sccache statistics if available
    try:
        result = subprocess.run(
            [sccache_path, "--show-stats"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if result.returncode == 0:
            print("SCCACHE Statistics:")
            for line in result.stdout.strip().split("\n"):
                if line.strip():
                    print(f"   {line}")
        else:
            print("SCCACHE stats not available (cache empty or first run)")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Could not retrieve SCCACHE stats: {e}")

    return True


def setup_ccache_environment(board_name: str) -> bool:
    """Set up ccache environment variables for the current process."""
    from ci.compiler.path_manager import FastLEDPaths

    # Check if ccache is available
    ccache_path = shutil.which("ccache")
    if not ccache_path:
        print("CCACHE not found in PATH, compilation will proceed without caching")
        return False

    print(f"Setting up CCACHE environment: {ccache_path}")

    # Set up ccache directory in the global .fastled directory
    # Shared across all boards for maximum cache efficiency
    paths = FastLEDPaths(board_name)
    ccache_dir = paths.fastled_root / "ccache"
    ccache_dir.mkdir(parents=True, exist_ok=True)

    # Configure ccache environment variables
    os.environ["CCACHE_DIR"] = str(ccache_dir)
    os.environ["CCACHE_MAXSIZE"] = "2G"

    print(f"CCACHE cache directory: {ccache_dir}")

    # Set compiler wrapper environment variables that PlatformIO will use
    # PlatformIO respects these environment variables for compiler selection
    original_cc = os.environ.get("CC", "")
    original_cxx = os.environ.get("CXX", "")

    # Only wrap if not already wrapped
    if "ccache" not in original_cc:
        # Set environment variables that PlatformIO/SCons will use
        os.environ["CC"] = (
            f'"{ccache_path}" {original_cc}' if original_cc else f'"{ccache_path}" gcc'
        )
        os.environ["CXX"] = (
            f'"{ccache_path}" {original_cxx}'
            if original_cxx
            else f'"{ccache_path}" g++'
        )

        print(f"Set CC environment variable: {os.environ['CC']}")
        print(f"Set CXX environment variable: {os.environ['CXX']}")

    print(f"CCACHE cache directory: {ccache_dir}")
    print("CCACHE cache size limit: 2G")

    # Show ccache statistics if available
    try:
        result = subprocess.run(
            [ccache_path, "--show-stats"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if result.returncode == 0:
            print("CCACHE Statistics:")
            for line in result.stdout.strip().split("\n"):
                if line.strip():
                    print(f"   {line}")
        else:
            print("CCACHE stats not available (cache empty or first run)")
        handle_keyboard_interrupt_properly()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Could not retrieve CCACHE stats: {e}")

    return True
