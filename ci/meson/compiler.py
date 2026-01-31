"""Compiler detection and Meson executable resolution."""

import os
import subprocess
import sys
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.timestamp_print import ts_print as _ts_print


def get_meson_executable() -> str:
    """
    Resolve meson executable path, preferring venv installation.

    This ensures consistent meson version usage regardless of invocation method
    (direct Python vs uv run). The venv meson version is controlled by pyproject.toml.

    Returns:
        Path to meson executable (venv version preferred, falls back to PATH)
    """
    # Try to find venv meson first
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent.parent

    # Platform-specific meson executable name
    is_windows = sys.platform.startswith("win") or os.name == "nt"
    meson_exe_name = "meson.exe" if is_windows else "meson"
    venv_meson = project_root / ".venv" / "Scripts" / meson_exe_name

    if venv_meson.exists():
        return str(venv_meson)

    # Fallback to PATH resolution (will use system meson)
    return "meson"


def check_meson_installed() -> bool:
    """Check if Meson is installed and accessible."""
    try:
        result = subprocess.run(
            [get_meson_executable(), "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
        return result.returncode == 0
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def get_meson_version() -> str:
    """
    Get the version of the meson executable that will be used.

    Returns:
        Version string (e.g., "1.10.1") or "unknown" on error
    """
    try:
        result = subprocess.run(
            [get_meson_executable(), "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
        if result.returncode == 0:
            return result.stdout.strip()
        return "unknown"
    except (subprocess.SubprocessError, FileNotFoundError):
        return "unknown"


def check_meson_version_compatibility(build_dir: Path) -> tuple[bool, str]:
    """
    Check if the current meson version is compatible with the build directory.

    CRITICAL: Meson versions 1.9.x and 1.10.x create INCOMPATIBLE build directories!
    A build directory created by one version cannot be used by the other version.
    This function checks for version mismatches that would cause reconfiguration.

    Args:
        build_dir: Path to the meson build directory

    Returns:
        Tuple of (is_compatible, message)
        - is_compatible: True if versions match or no stored version exists
        - message: Description of any incompatibility found
    """
    current_version = get_meson_version()
    if current_version == "unknown":
        return True, "Could not determine meson version"

    # Check the stored meson version in coredata.dat
    coredata_path = build_dir / "meson-private" / "coredata.dat"
    if not coredata_path.exists():
        return True, "Build directory not configured yet"

    try:
        import pickle

        with open(coredata_path, "rb") as f:
            coredata = pickle.load(f)
            stored_version = getattr(coredata, "version", None)

        if stored_version is None:
            return True, "No version info in coredata"

        # Check if major.minor versions match
        current_major_minor = ".".join(current_version.split(".")[:2])
        stored_major_minor = ".".join(stored_version.split(".")[:2])

        if current_major_minor != stored_major_minor:
            return False, (
                f"Meson version mismatch! Build directory was created with meson {stored_version}, "
                f"but current meson is {current_version}. These versions are INCOMPATIBLE. "
                f"Either delete the build directory or ensure you use the same meson version."
            )

        return (
            True,
            f"Meson versions compatible ({current_version} vs {stored_version})",
        )

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        # If we can't check, assume compatible to avoid breaking builds
        return True, f"Could not check version compatibility: {e}"


def get_compiler_version(compiler_path: str) -> str:
    """
    Get compiler version string for cache invalidation.

    This function queries the compiler's version to detect when the compiler
    has been upgraded, which requires invalidating cached PCH and object files.

    Args:
        compiler_path: Path to compiler executable (e.g., clang-tool-chain-cpp)

    Returns:
        Version string (e.g., "clang version 21.1.5") or "unknown" on error
    """
    try:
        result = subprocess.run(
            [compiler_path, "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=True,
            timeout=10,
        )
        # Return first line which contains version info
        return result.stdout.split("\n")[0].strip()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"[MESON] Warning: Could not get compiler version: {e}")
        return "unknown"
