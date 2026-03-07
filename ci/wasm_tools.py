#!/usr/bin/env python3
"""
Utility functions for accessing WASM compilation tools.

PERFORMANCE OPTIMIZATION: Instead of using clang-tool-chain Python entry point
wrappers (which have ~3-5 second startup overhead per invocation), this module
resolves raw binary paths and sets up the required environment once.

On first call, setup_emscripten_env() configures EMSCRIPTEN, EM_CONFIG, and
PATH in os.environ. Subsequent get_emcc()/get_emar()/get_wasm_ld() calls
return direct paths to the tools, bypassing the Python wrapper entirely.

Benchmarks (Windows, per invocation):
  - clang-tool-chain-emcc wrapper:  ~5000ms
  - Direct emcc.bat:                ~1400ms  (3.7x faster)
  - clang-tool-chain-wasm-ld:       ~5400ms
  - Direct wasm-ld.exe:               ~60ms  (90x faster)
"""

import os
import platform
import sys
from pathlib import Path
from typing import Optional


_env_setup_done = False
_fast_emcc: Optional[str] = None
_fast_emar: Optional[str] = None
_fast_wasm_ld: Optional[str] = None


def _get_platform_info() -> tuple[str, str]:
    """Detect platform and architecture without importing clang_tool_chain."""
    system = platform.system().lower()
    machine = platform.machine().lower()

    if system == "windows":
        platform_name = "win"
    elif system == "linux":
        platform_name = "linux"
    elif system == "darwin":
        platform_name = "darwin"
    else:
        raise RuntimeError(f"Unsupported platform: {system}")

    if machine in ("x86_64", "amd64"):
        arch = "x86_64"
    elif machine in ("arm64", "aarch64"):
        arch = "arm64"
    else:
        raise RuntimeError(f"Unsupported architecture: {machine}")

    return platform_name, arch


def setup_emscripten_env() -> bool:
    """
    Set up emscripten environment variables in the current process.

    Sets EMSCRIPTEN, EMSCRIPTEN_ROOT, EM_CONFIG, and adds emscripten bin
    directory to PATH. Only runs once per process.

    Returns:
        True if fast paths were resolved, False if falling back to wrappers.
    """
    global _env_setup_done, _fast_emcc, _fast_emar, _fast_wasm_ld

    if _env_setup_done:
        return _fast_emcc is not None

    _env_setup_done = True

    try:
        platform_name, arch = _get_platform_info()
        is_windows = platform_name == "win"

        home = Path.home()
        install_dir = home / ".clang-tool-chain" / "emscripten" / platform_name / arch
        emscripten_dir = install_dir / "emscripten"
        config_path = install_dir / ".emscripten"
        bin_dir = install_dir / "bin"

        if not emscripten_dir.exists() or not config_path.exists():
            return False

        # Resolve tool paths
        if is_windows:
            emcc_path = emscripten_dir / "emcc.bat"
            emar_path = emscripten_dir / "emar.bat"
            wasm_ld_path = bin_dir / "wasm-ld.exe"
        else:
            emcc_path = emscripten_dir / "emcc"
            emar_path = emscripten_dir / "emar"
            wasm_ld_path = bin_dir / "wasm-ld"

        if not emcc_path.exists() or not emar_path.exists():
            return False

        # Set up environment variables that emscripten needs
        os.environ["EMSCRIPTEN"] = str(emscripten_dir)
        os.environ["EMSCRIPTEN_ROOT"] = str(emscripten_dir)
        os.environ["EM_CONFIG"] = str(config_path)

        # Ensure emscripten uses the same Python as us
        os.environ["EMSDK_PYTHON"] = sys.executable

        # Add emscripten bin directory to PATH (for clang, node, etc.)
        current_path = os.environ.get("PATH", "")
        bin_dir_str = str(bin_dir)
        if bin_dir_str not in current_path:
            os.environ["PATH"] = f"{bin_dir_str}{os.pathsep}{current_path}"

        # Ensure Node.js is available - try clang-tool-chain's bundled node first
        try:
            from clang_tool_chain.execution.emscripten import ensure_nodejs_available

            node_path = ensure_nodejs_available()
            node_bin_dir = str(node_path.parent)
            if node_bin_dir not in os.environ["PATH"]:
                os.environ["PATH"] = f"{node_bin_dir}{os.pathsep}{os.environ['PATH']}"
        except Exception:
            pass  # Node.js might already be in PATH

        _fast_emcc = str(emcc_path)
        _fast_emar = str(emar_path)
        if wasm_ld_path.exists():
            _fast_wasm_ld = str(wasm_ld_path)

        return True

    except Exception:
        return False


def get_wasm_ld() -> str:
    """
    Get wasm-ld linker command.

    Returns:
        Path to wasm-ld binary, or clang-tool-chain wrapper as fallback.
    """
    setup_emscripten_env()
    if _fast_wasm_ld is not None:
        return _fast_wasm_ld
    return "clang-tool-chain-wasm-ld"


def get_emcc() -> str:
    """
    Get emscripten C compiler command.

    Returns:
        Path to emcc script, or clang-tool-chain wrapper as fallback.
    """
    setup_emscripten_env()
    if _fast_emcc is not None:
        return _fast_emcc
    return "clang-tool-chain-emcc"


def get_emar() -> str:
    """
    Get emscripten archiver command.

    Returns:
        Path to emar script, or clang-tool-chain wrapper as fallback.
    """
    setup_emscripten_env()
    if _fast_emar is not None:
        return _fast_emar
    return "clang-tool-chain-emar"
