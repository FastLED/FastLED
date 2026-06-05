#!/usr/bin/env python3
"""Native launcher resolution for Meson native files and WASM cross-files.

Resolves compiled ``ctc-*`` native launcher binaries produced by
``clang_tool_chain.commands.compile_native``. Native launchers replace the
Python ``clang-tool-chain-*`` wrappers with ~34ms startup vs ~1200ms, saving
multiple seconds during meson setup (which probes the compiler ~10 times).

Also provides zccache binary discovery for the compiler cache integration.
"""
# pyright: reportMissingImports=false, reportUnknownVariableType=false

import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from typeguard import typechecked

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


@dataclass(slots=True)
class FastNativeEntries:
    cc: Optional[str]
    cxx: Optional[str]
    ar: Optional[str]


@typechecked
@dataclass(slots=True)
class EmccNativeLaunchers:
    """Compiled emscripten-tool launcher paths produced by ``compile_native()``.

    Every field is a guaranteed-present absolute path. ``compile_native()``
    in clang-tool-chain 1.5.1+ produces all seven launchers in one call;
    if any is missing afterward the resolver raises rather than silently
    falling back. clang-tool-chain >=1.5.1 is pinned in pyproject.toml.
    """

    emcc: str
    empp: str
    emar: str
    emstrip: str
    emranlib: str
    emnm: str
    wasm_ld: str


@typechecked
@dataclass(slots=True)
class WasmNativeEntries:
    """Native tool entries for the WASM cross-file.

    All fields are absolute paths to compiled ctc-* native launchers.
    No Python-wrapper fallbacks — clang-tool-chain >=1.5.1 is a hard
    requirement and ``compile_native()`` is invoked eagerly.

    ``wasm_ld`` is not consumed by meson directly. emcc invokes wasm-ld
    internally; the integration is via the ``EMCC_WASM_LD`` env var picked
    up by the shared.py patch that ``ensure_emscripten_available`` applies.
    Carried here so the build orchestration can set the env var.
    """

    c: str
    cpp: str
    ar: str
    strip: str
    ranlib: str
    nm: str
    wasm_ld: str


def _native_launcher_output_dir(project_root: Path) -> Path:
    """Directory where compiled native launchers live."""
    return project_root / ".cached" / "clang-native"


def _ensure_native_launcher(project_root: Path) -> tuple[Optional[str], Optional[str]]:
    """
    Ensure ctc-clang/ctc-clang++ native launchers are compiled.

    The native launcher is a compiled C++ binary that replaces the Python
    clang-tool-chain wrapper with near-zero startup overhead (~34ms vs ~1200ms).
    It auto-detects the platform, finds the clang installation, and adds all
    necessary flags (--target, --sysroot, -stdlib, includes, etc.) automatically.

    Returns:
        Tuple of (ctc_clang_path, ctc_clangpp_path), or (None, None) on failure.
    """
    output_dir = _native_launcher_output_dir(project_root)
    exe_suffix = ".exe" if sys.platform == "win32" else ""
    ctc_clang = output_dir / f"ctc-clang{exe_suffix}"
    ctc_clangpp = output_dir / f"ctc-clang++{exe_suffix}"

    if ctc_clang.exists() and ctc_clangpp.exists():
        return str(ctc_clang), str(ctc_clangpp)

    try:
        from clang_tool_chain.commands.compile_native import compile_native

        output_dir.mkdir(parents=True, exist_ok=True)
        rc = compile_native(str(output_dir))
        if rc == 0 and ctc_clang.exists() and ctc_clangpp.exists():
            return str(ctc_clang), str(ctc_clangpp)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
    except Exception:
        pass
    return None, None


def _ensure_emcc_native_launcher(project_root: Path) -> EmccNativeLaunchers:
    """
    Build and return all seven WASM-related ctc-* native launcher paths.

    ``compile_native()`` from clang-tool-chain 1.5.1+ produces every binary
    in a single invocation (clang + emcc + wasm-ld + emtool family with all
    its hardlinked aliases). If any binary is missing afterward, raises
    RuntimeError — no Python-wrapper fallback. clang-tool-chain >=1.5.1 is
    pinned in pyproject.toml so this is a hard requirement.

    Raises:
        RuntimeError: if ``compile_native()`` fails or any launcher binary
            is missing after a successful build.
    """
    output_dir = _native_launcher_output_dir(project_root)
    exe_suffix = ".exe" if sys.platform == "win32" else ""
    binaries = {
        "emcc": output_dir / f"ctc-emcc{exe_suffix}",
        "empp": output_dir / f"ctc-em++{exe_suffix}",
        "emar": output_dir / f"ctc-emar{exe_suffix}",
        "emstrip": output_dir / f"ctc-emstrip{exe_suffix}",
        "emranlib": output_dir / f"ctc-emranlib{exe_suffix}",
        "emnm": output_dir / f"ctc-emnm{exe_suffix}",
        "wasm_ld": output_dir / f"ctc-wasm-ld{exe_suffix}",
    }

    # Warm-path presence check: one os.scandir() enumerates the whole dir
    # in ~10-20 ms; 7 individual Path.exists() calls cost ~30 ms each on
    # Windows NTFS (~240 ms total). Set-membership lookups are O(1).
    try:
        present_names: set[str] = {entry.name for entry in os.scandir(output_dir)}
    except OSError:
        present_names = set()
    all_present = all(p.name in present_names for p in binaries.values())

    if not all_present:
        from clang_tool_chain.commands.compile_native import compile_native

        output_dir.mkdir(parents=True, exist_ok=True)
        rc = compile_native(str(output_dir))
        if rc != 0:
            raise RuntimeError(
                f"clang-tool-chain compile_native() failed with rc={rc} "
                f"in {output_dir}; cannot resolve WASM native launchers"
            )
        missing = [name for name, p in binaries.items() if not p.exists()]
        if missing:
            raise RuntimeError(
                f"clang-tool-chain compile_native() succeeded but expected "
                f"binaries are missing in {output_dir}: {missing}. "
                f"Ensure clang-tool-chain >=1.5.1 is installed."
            )

    return EmccNativeLaunchers(**{name: str(p) for name, p in binaries.items()})


def resolve_wasm_native_entries(project_root: Path) -> WasmNativeEntries:
    """
    Resolve compiler/archiver entries for the Meson WASM cross-file.

    Returns absolute paths to the seven compiled ctc-* native launchers.
    No Python-wrapper fallback — if ``compile_native()`` fails or any
    binary is missing, raises (clang-tool-chain >=1.5.1 is pinned and
    guarantees all seven launchers).

    Raises:
        RuntimeError: if ``compile_native()`` fails or any binary is missing.
    """
    launchers = _ensure_emcc_native_launcher(project_root)
    return WasmNativeEntries(
        c=launchers.emcc,
        cpp=launchers.empp,
        ar=launchers.emar,
        strip=launchers.emstrip,
        ranlib=launchers.emranlib,
        nm=launchers.emnm,
        wasm_ld=launchers.wasm_ld,
    )


def _find_zccache_binary() -> Optional[str]:
    """
    Find the zccache binary for compiler caching.

    zccache is a blazing-fast local compiler cache daemon (43x faster than
    other caches on warm hits). It works as a drop-in compiler launcher prefix.

    Search order:
    1. Sibling zccache repo (../zccache/target/{release,debug}) — dev builds
    2. PATH (via shutil.which)
    3. Common cargo install locations

    Returns:
        Path to zccache binary, or None if not found.
    """
    suffix = (
        ".exe"
        if os.name == "nt" or sys.platform.startswith(("win", "msys", "cygwin"))
        else ""
    )

    # Check project .venv first (installed release version)
    repo_root = Path(__file__).resolve().parent.parent.parent  # ci/meson/ -> repo root
    venv_candidate = (
        repo_root
        / ".venv"
        / ("Scripts" if os.name == "nt" else "bin")
        / f"zccache{suffix}"
    )
    if venv_candidate.is_file():
        return str(venv_candidate)

    # Check sibling zccache repo (pick most recently built binary)
    sibling_zccache = repo_root.parent / "zccache"
    best: Optional[Path] = None
    best_mtime: float = 0
    for profile in ("release", "debug"):
        candidate = sibling_zccache / "target" / profile / f"zccache{suffix}"
        if candidate.is_file():
            mtime = candidate.stat().st_mtime
            if mtime > best_mtime:
                best = candidate
                best_mtime = mtime
    if best is not None:
        return str(best)

    # Check PATH
    path_result = shutil.which("zccache")
    if path_result:
        return path_result

    # Check common cargo bin locations
    for candidate_home in [
        os.environ.get("CARGO_HOME", ""),
        os.path.join(os.path.expanduser("~"), ".cargo"),
    ]:
        if candidate_home:
            candidate_path = os.path.join(candidate_home, "bin", f"zccache{suffix}")
            if os.path.isfile(candidate_path):
                return candidate_path

    return None


def get_zccache_version(zccache_path: Optional[str] = None) -> str:
    """
    Get zccache version string for cache invalidation.

    When the zccache version changes, cached compilation artifacts may be
    incompatible and require a full rebuild.

    Args:
        zccache_path: Path to zccache binary. If None, auto-detected.

    Returns:
        Version string (e.g., "zccache 1.0.3") or "" if not available.
    """
    if zccache_path is None:
        zccache_path = _find_zccache_binary()
    if not zccache_path:
        return ""
    try:
        result = subprocess.run(
            [zccache_path, "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
        )
        if result.returncode == 0:
            return result.stdout.strip()
        return ""
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return ""


def _resolve_fast_native_entries() -> FastNativeEntries:
    """
    Resolve native launcher binaries for the Meson native file.

    Uses ctc-clang/ctc-clang++ native launchers which handle all platform-
    specific flags automatically (--target, --sysroot, -stdlib, includes, etc.)
    with near-zero startup overhead (~34ms vs ~1200ms for Python wrappers).

    Note: zccache wrapping is intentionally NOT applied here. The meson
    configure phase runs probes like ``<cc> -xc++ -E -v -`` to scan the
    compiler's include search paths and capabilities. zccache (and other
    full-path compiler caches that meson does not recognise — see
    ``mesonbuild/envconfig.py:BinaryTable.parse_entry`` which only strips
    the literal strings ``'ccache'`` / ``'sccache'``) wraps those probes
    and causes them to hang or return non-parseable output, leading to
    "No include directory found" warnings followed by failing capability
    probes. Instead, zccache is applied as a post-patch on the generated
    ``build.ninja`` via ``inject_zccache_wrapping`` — probes run against
    the bare compiler, build runs through zccache as before. See issue
    #2714.

    Returns:
        FastNativeEntries with cc, cxx, and ar entries for the native file,
        or all-None fields if resolution fails (falls back to wrappers).
    """
    _none = FastNativeEntries(cc=None, cxx=None, ar=None)
    try:
        from clang_tool_chain.platform.paths import (
            find_tool_binary,
        )
    except ImportError:
        return _none

    try:
        ar_path = str(find_tool_binary("llvm-ar"))

        project_root = Path(__file__).resolve().parent.parent.parent
        ctc_c, ctc_cpp = _ensure_native_launcher(project_root)
        if ctc_c is None or ctc_cpp is None:
            return _none

        # Escape any single quotes in the path so the generated Meson
        # list-of-strings remains valid even if a path happens to contain
        # one. Paths under ``.cached/clang-native/`` are extremely unlikely
        # to contain ``'`` in practice, but defensive escaping costs nothing
        # and avoids a hard-to-debug native-file parse error.
        def _q(p: str) -> str:
            return p.replace("'", "\\'")

        return FastNativeEntries(
            cc=f"['{_q(ctc_c)}']",
            cxx=f"['{_q(ctc_cpp)}']",
            ar=f"['{_q(ar_path)}']",
        )

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        return _none  # unreachable, satisfies type checker
    except Exception:
        return _none


def _resolve_fast_compiler_binary() -> Optional[str]:
    """
    Resolve raw clang++ binary path, bypassing the Python entry point wrapper.

    The Python wrapper (clang-tool-chain-cpp.EXE) has ~3.6 second startup
    overhead. Using the raw binary directly saves this overhead for operations
    like --version checks.

    Returns:
        Path to raw clang++ binary, or None if resolution fails.
    """
    try:
        from clang_tool_chain.platform.paths import find_tool_binary

        return str(find_tool_binary("clang++"))
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        return None  # unreachable, satisfies type checker
    except Exception:
        return None
