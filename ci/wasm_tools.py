#!/usr/bin/env python3
"""
Utility functions for accessing WASM compilation tools.

PERFORMANCE OPTIMIZATION: Instead of using clang-tool-chain Python entry point
wrappers (which have ~3-5 second startup overhead per invocation), this module
resolves raw binary paths and sets up the required environment once.

On first call, setup_emscripten_env() configures EMSCRIPTEN, EM_CONFIG, and
PATH in os.environ. Subsequent get_emcc()/get_emar()/get_wasm_ld() calls
return direct paths to the tools, bypassing the Python wrapper entirely.

For maximum speed, run_emcc() calls emcc.py in-process via importlib,
eliminating subprocess + Python startup overhead entirely (~0.5s per call).

Benchmarks (Windows, per invocation):
  - clang-tool-chain-emcc wrapper:  ~5000ms
  - Direct emcc.bat:                ~1400ms  (3.7x faster)
  - In-process emcc.main():          ~400ms  (12x faster)
  - clang-tool-chain-wasm-ld:       ~5400ms
  - Direct wasm-ld.exe:               ~60ms  (90x faster)
"""

import importlib.util
import os
import platform
import subprocess
import sys
from pathlib import Path
from typing import Optional


_env_setup_done = False
_fast_emcc: Optional[str] = None
_fast_emar: Optional[str] = None
_fast_wasm_ld: Optional[str] = None
_emscripten_dir: Optional[Path] = None
_emcc_module: Optional[object] = None

# Native launcher paths (ctc-emcc, ctc-wasm-ld)
_native_tools_dir: Optional[Path] = None
_native_emcc: Optional[str] = None
_native_wasm_ld: Optional[str] = None


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

        # Skip emscripten sanity checks — we already validated tool existence above.
        # Saves ~0.3s per emcc invocation (sanity check probes LLVM, node, etc.).
        os.environ["EMCC_SKIP_SANITY_CHECK"] = "1"

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
        _emscripten_dir = emscripten_dir
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


def ensure_native_tools() -> bool:
    """Compile ctc-emcc and ctc-wasm-ld native launchers if not already present.

    Uses clang-tool-chain's compile-native command to build native C++ wrappers
    that bypass Python/Node startup overhead for emcc and wasm-ld invocations.
    The binaries are cached in .build/native-tools/ and reused across builds.

    Returns True if native tools are available.
    """
    global _native_tools_dir, _native_emcc, _native_wasm_ld

    if _native_emcc is not None:
        return True

    project_root = Path(__file__).parent.parent
    tools_dir = project_root / ".build" / "native-tools"
    _native_tools_dir = tools_dir

    is_windows = platform.system().lower() == "windows"
    exe_suffix = ".exe" if is_windows else ""

    emcc_bin = tools_dir / f"ctc-emcc{exe_suffix}"
    wasm_ld_bin = tools_dir / f"ctc-wasm-ld{exe_suffix}"

    if emcc_bin.exists() and wasm_ld_bin.exists():
        _native_emcc = str(emcc_bin)
        _native_wasm_ld = str(wasm_ld_bin)
        return True

    # Compile native tools using clang-tool-chain
    try:
        from clang_tool_chain.commands.compile_native import compile_native

        print("[WASM] Compiling native WASM launchers (one-time)...")
        rc = compile_native(str(tools_dir))
        if rc == 0 and emcc_bin.exists() and wasm_ld_bin.exists():
            _native_emcc = str(emcc_bin)
            _native_wasm_ld = str(wasm_ld_bin)
            return True
    except Exception as e:
        print(f"[WASM] Native tool compilation failed: {e}")

    return False


def get_native_emcc() -> Optional[str]:
    """Get path to native ctc-emcc launcher, or None if not available."""
    ensure_native_tools()
    return _native_emcc


def get_native_wasm_ld() -> Optional[str]:
    """Get path to native ctc-wasm-ld launcher, or None if not available."""
    ensure_native_tools()
    return _native_wasm_ld


def _load_emcc_module():
    """Lazily load emcc.py as a Python module for in-process invocation."""
    global _emcc_module
    if _emcc_module is not None:
        return _emcc_module

    setup_emscripten_env()
    if _emscripten_dir is None:
        return None

    emcc_py = _emscripten_dir / "emcc.py"
    if not emcc_py.exists():
        return None

    # Add emscripten dir to sys.path so emcc.py can import its 'tools' package
    emscripten_str = str(_emscripten_dir)
    if emscripten_str not in sys.path:
        sys.path.insert(0, emscripten_str)

    spec = importlib.util.spec_from_file_location("emcc", str(emcc_py))
    if spec is None or spec.loader is None:
        return None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    _emcc_module = mod
    return _emcc_module


def run_emcc(args: list[str], cwd: Optional[str] = None) -> int:
    """Run emcc with the given args, trying in-process first, falling back to subprocess.

    Args:
        args: Command-line arguments (WITHOUT the 'emcc' prefix — just flags and files).
        cwd: Working directory for the invocation.

    Returns:
        Process return code (0 = success).
    """
    mod = _load_emcc_module()
    if mod is not None:
        # In-process invocation — save/restore global state
        old_argv = sys.argv
        old_cwd = os.getcwd()
        try:
            sys.argv = ["emcc"] + args
            if cwd:
                os.chdir(cwd)
            return mod.main(sys.argv)  # type: ignore[union-attr]
        except SystemExit as e:
            return e.code if isinstance(e.code, int) else 1
        finally:
            sys.argv = old_argv
            os.chdir(old_cwd)

    # Fallback: subprocess
    emcc = get_emcc()
    result = subprocess.run([emcc] + args, cwd=cwd)
    return result.returncode
