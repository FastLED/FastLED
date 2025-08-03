"""Configure CCACHE for PlatformIO builds."""

import os
import platform
import subprocess
from pathlib import Path
from typing import Any, Protocol


# ruff: noqa: F821
# pyright: reportUndefinedVariable=false
Import("env")  # type: ignore # Import is provided by PlatformIO


class PlatformIOEnv(Protocol):
    """Type information for PlatformIO environment."""

    def get(self, key: str, default: str | None = None) -> str | None:
        """Get a value from the environment."""
        ...

    def Replace(self, **kwargs: Any) -> None:
        """Replace environment variables."""
        ...


def is_ccache_available() -> bool:
    """Check if ccache is available in the system."""
    try:
        subprocess.run(["ccache", "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def get_ccache_path() -> str | None:
    """Get the full path to ccache executable."""
    if platform.system() == "Windows":
        # On Windows, look in chocolatey's bin directory
        ccache_paths = [
            "C:\\ProgramData\\chocolatey\\bin\\ccache.exe",
            os.path.expanduser("~\\scoop\\shims\\ccache.exe"),
        ]
        for path in ccache_paths:
            if os.path.exists(path):
                return path
    else:
        # On Unix-like systems, use which to find ccache
        try:
            return subprocess.check_output(["which", "ccache"]).decode().strip()
        except subprocess.CalledProcessError:
            pass
    return None


def configure_ccache(env: PlatformIOEnv) -> None:  # type: ignore # env is provided by PlatformIO
    """Configure CCACHE for the build environment."""
    if not is_ccache_available():
        print("CCACHE is not available. Skipping CCACHE configuration.")
        return

    ccache_path = get_ccache_path()
    if not ccache_path:
        print("Could not find CCACHE executable. Skipping CCACHE configuration.")
        return

    print(f"Found CCACHE at: {ccache_path}")

    # Set up CCACHE environment variables if not already set
    if "CCACHE_DIR" not in os.environ:
        project_dir = env.get("PROJECT_DIR")
        if project_dir is None:
            project_dir = os.getcwd()

        # Use board-specific ccache directory if PIOENV (board environment) is available
        board_name = env.get("PIOENV")
        if board_name:
            ccache_dir = os.path.join(project_dir, ".ccache", board_name)
        else:
            ccache_dir = os.path.join(project_dir, ".ccache", "default")

        os.environ["CCACHE_DIR"] = ccache_dir
        Path(ccache_dir).mkdir(parents=True, exist_ok=True)
        print(f"Using board-specific CCACHE directory: {ccache_dir}")

    # Configure CCACHE for this build
    project_dir = env.get("PROJECT_DIR")
    if project_dir is None:
        project_dir = os.getcwd()
    os.environ["CCACHE_BASEDIR"] = project_dir
    os.environ["CCACHE_COMPRESS"] = "true"
    os.environ["CCACHE_COMPRESSLEVEL"] = "6"
    os.environ["CCACHE_MAXSIZE"] = "400M"

    # Wrap compiler commands with ccache
    # STRICT: CC and CXX must be explicitly set - NO fallbacks allowed
    original_cc = env.get("CC")
    if not original_cc:
        raise RuntimeError(
            "CRITICAL: CC environment variable is required but not set. "
            "Please set CC to the C compiler path (e.g., gcc, clang)."
        )
    original_cxx = env.get("CXX")
    if not original_cxx:
        raise RuntimeError(
            "CRITICAL: CXX environment variable is required but not set. "
            "Please set CXX to the C++ compiler path (e.g., g++, clang++)."
        )

    # Don't wrap if already wrapped
    if original_cc is not None and "ccache" not in original_cc:
        env.Replace(
            CC=f"{ccache_path} {original_cc}",
            CXX=f"{ccache_path} {original_cxx}",
        )
        print(f"Wrapped CC: {env.get('CC')}")
        print(f"Wrapped CXX: {env.get('CXX')}")

    # Show CCACHE stats
    subprocess.run([ccache_path, "--show-stats"], check=False)


# ruff: noqa: F821
configure_ccache(env)  # type: ignore # env is provided by PlatformIO
