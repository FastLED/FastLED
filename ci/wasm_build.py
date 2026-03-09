#!/usr/bin/env python3
"""
Thin WASM build orchestrator using Meson + Ninja.

This script replaces ~3000 lines of custom Python build logic with a thin
orchestrator that delegates compilation to Meson/Ninja and only handles:
  - Meson setup with emscripten cross file
  - Ninja invocation for library build
  - Sketch compilation (single emcc call)
  - Sketch linking (single emcc call)
  - Template file copying and manifest generation

All dependency tracking, parallel compilation, PCH management, and
incremental builds are handled by Ninja automatically.

Per-sketch artifacts are cached in .build/wasm/<SketchName>/ to enable
fast switching between sketches without recompilation.

Usage:
    uv run python ci/wasm_build.py --example Blink -o examples/Blink/fastled_js/fastled.js
    uv run python ci/wasm_build.py --example Blink -o output.js --mode debug
    uv run python ci/wasm_build.py --example Blink -o output.js --force
"""

# pyright: reportMissingImports=false

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

from ci.wasm_flags import get_link_flags, get_sketch_compile_flags
from ci.wasm_tools import run_emcc


PROJECT_ROOT = Path(__file__).parent.parent
CROSS_FILE = PROJECT_ROOT / "ci" / "meson" / "wasm_cross_file.ini"
WASM_PCH_HEADER = (
    PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "wasm_pch.h"
)

# Build modes map to meson build directories
BUILD_DIR_PREFIX = PROJECT_ROOT / ".build"


def get_build_dir(mode: str) -> Path:
    """Get the meson build directory for the given mode."""
    return BUILD_DIR_PREFIX / f"meson-wasm-{mode}"


def get_sketch_cache_dir(example_name: str) -> Path:
    """Get per-sketch cache directory next to the .ino file: <sketch_dir>/.build/wasm/."""
    ino_file = PROJECT_ROOT / "examples" / example_name / f"{example_name}.ino"
    if not ino_file.exists():
        raise FileNotFoundError(f"Sketch not found: {ino_file}")
    d = ino_file.parent / ".build" / "wasm"
    d.mkdir(parents=True, exist_ok=True)
    return d


def get_meson_executable() -> str:
    """Get the meson executable path."""
    # Use the same meson that the native build uses
    return "meson"


# ============================================================================
# Library fingerprint — skip meson/ninja when source tree hasn't changed
# ============================================================================


_cached_fingerprint: str | None = None
_cached_fingerprint_time: float = 0.0


def _compute_src_fingerprint() -> str:
    """Fast fingerprint of source tree based on file mtimes.

    Uses os.scandir (3x faster than os.walk on Windows — DirEntry.stat()
    reuses data from FindFirstFile, avoiding extra syscalls).
    Caches the result within the same process invocation (cleared after 0.5s).
    """
    global _cached_fingerprint, _cached_fingerprint_time
    now = time.monotonic()
    if _cached_fingerprint is not None and (now - _cached_fingerprint_time) < 0.5:
        return _cached_fingerprint

    h = hashlib.md5(usedforsecurity=False)
    src_dir = str(PROJECT_ROOT / "src")
    _EXTS = (".cpp", ".h", ".hpp", ".c")

    # Recursive scandir — on Windows, DirEntry.stat() is free (no extra syscall)
    def _scan(path: str) -> None:
        try:
            entries = sorted(os.scandir(path), key=lambda e: e.name)
        except OSError:
            return
        subdirs: list[str] = []
        for entry in entries:
            if entry.is_dir(follow_symlinks=False):
                subdirs.append(entry.path)
            elif entry.is_file(follow_symlinks=False) and entry.name.endswith(_EXTS):
                try:
                    st = entry.stat()
                    h.update(f"{entry.path}:{st.st_mtime:.6f}".encode())
                except OSError:
                    h.update(f"{entry.path}:MISSING".encode())
        for d in subdirs:
            _scan(d)

    _scan(src_dir)

    # Also hash build config files that affect the library
    for config_file in [
        PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "build_flags.toml",
        PROJECT_ROOT / "meson.build",
        PROJECT_ROOT / "ci" / "meson" / "wasm" / "meson.build",
        PROJECT_ROOT / "ci" / "meson" / "wasm_cross_file.ini",
    ]:
        try:
            h.update(f"{config_file}:{config_file.stat().st_mtime:.6f}".encode())
        except OSError:
            h.update(f"{config_file}:MISSING".encode())

    _cached_fingerprint = h.hexdigest()
    _cached_fingerprint_time = now
    return _cached_fingerprint


def _library_is_fresh(build_dir: Path) -> bool:
    """Check if libfastled.a is up-to-date based on source fingerprint."""
    library_archive = build_dir / "ci" / "meson" / "wasm" / "libfastled.a"
    fingerprint_file = build_dir / "library_src_fingerprint"

    if not library_archive.exists() or not fingerprint_file.exists():
        return False

    try:
        stored = fingerprint_file.read_text(encoding="utf-8").strip()
        current = _compute_src_fingerprint()
        return stored == current
    except OSError:
        return False


def _save_library_fingerprint(build_dir: Path) -> None:
    """Save source fingerprint after successful library build."""
    fingerprint_file = build_dir / "library_src_fingerprint"
    try:
        fingerprint_file.write_text(_compute_src_fingerprint(), encoding="utf-8")
    except OSError:
        pass


# ============================================================================
# Build steps
# ============================================================================


def ensure_meson_configured(build_dir: Path, mode: str, force: bool = False) -> bool:
    """
    Ensure meson is configured for WASM cross-compilation.

    Returns True if configuration succeeded.
    """
    if build_dir.exists() and (build_dir / "build.ninja").exists() and not force:
        # Already configured - meson will auto-reconfigure if needed
        return True

    build_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        get_meson_executable(),
        "setup",
        "--cross-file",
        str(CROSS_FILE),
        str(build_dir),
        f"-Dbuild_mode={mode}",
    ]

    if build_dir.exists() and (build_dir / "build.ninja").exists():
        # Reconfigure existing build
        cmd.insert(2, "--reconfigure")

    print(f"[WASM] Configuring meson (mode: {mode})...")
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if result.returncode != 0:
        print(f"[WASM] Meson setup failed with return code {result.returncode}")
        return False

    print("[WASM] Meson setup successful")
    return True


def build_library(build_dir: Path, verbose: bool = False) -> tuple[bool, bool]:
    """
    Build libfastled.a using ninja.

    Skips the build entirely if the source fingerprint hasn't changed.
    Returns (success, was_rebuilt) — was_rebuilt is True if sources changed.
    """
    # Fast path: skip meson/ninja if source tree hasn't changed
    if _library_is_fresh(build_dir):
        if verbose:
            print("[WASM] Library up-to-date (fingerprint match), skipping meson")
        else:
            print("[WASM] Library up-to-date")
        return True, False

    # Check for stale PCH before invoking Ninja (Windows backslash path issue)
    from ci.compile_pch import invalidate_stale_pch

    pch_file = build_dir / "ci" / "meson" / "wasm" / "wasm_pch.h.pch"
    invalidate_stale_pch(pch_file)
    cmd = [get_meson_executable(), "compile", "-C", str(build_dir), "fastled"]
    if verbose:
        cmd.append("-v")

    print("[WASM] Building libfastled.a...")
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if result.returncode != 0:
        print(f"[WASM] Library build failed with return code {result.returncode}")
        return False, False

    _save_library_fingerprint(build_dir)
    print("[WASM] Library build successful")
    return True, True


def create_wrapper(example_name: str, sketch_cache_dir: Path) -> Path:
    """
    Create a wrapper .cpp that includes the sketch .ino file.

    Returns path to the wrapper file.
    """
    example_dir = PROJECT_ROOT / "examples" / example_name
    ino_file = example_dir / f"{example_name}.ino"

    if not ino_file.exists():
        raise FileNotFoundError(f"Example not found: {ino_file}")

    wrapper_path = sketch_cache_dir / f"{example_name}_wrapper.cpp"
    wrapper_content = f"""// Auto-generated wrapper for {example_name}.ino
// C++20 header unit import — ~2x faster than PCH for sketch compilation.
// The .ino's #include "FastLED.h" is a harmless no-op after this import.
import "wasm_pch.h";
#include "{ino_file.as_posix()}"
"""

    # Only write if content changed (preserve timestamp for incremental builds)
    if wrapper_path.exists():
        existing = wrapper_path.read_text()
        if existing == wrapper_content:
            return wrapper_path

    wrapper_path.write_text(wrapper_content)
    return wrapper_path


def build_sketch_pch(
    build_dir: Path,
    mode: str,
    lib_was_rebuilt: bool = False,
    verbose: bool = False,
) -> Path | None:
    """
    Build a C++20 header unit (.pcm) from wasm_pch.h for sketch compilation.

    Uses -fmodules-codegen to pre-compile inline function bodies into a companion
    object file. This reduces sketch backend work by ~63% (509 → 187 codegen
    functions for Blink) while keeping header unit import fast (~6ms vs 34ms PCH).

    The companion object is added to libfastled.a so the linker picks up symbols
    naturally — no changes needed to link commands.

    Args:
        lib_was_rebuilt: If True, library sources changed — invalidate header unit
            since included headers may have changed.

    Returns path to the header unit BMI (.pcm), or None if build fails.
    """
    sketch_pcm_path = build_dir / "wasm_pch.h.pcm"
    sketch_pch_hash_path = build_dir / "sketch_pch.hash"
    pch_codegen_o = build_dir / "pch_codegen.o"

    sketch_flags = get_sketch_compile_flags(mode)
    # Combine flags + source fingerprint to detect both flag and header changes
    src_fp = _compute_src_fingerprint()
    combined = "\n".join(sketch_flags) + "\n" + src_fp
    current_hash = hashlib.sha256(combined.encode()).hexdigest()

    # Check if header unit BMI is up-to-date
    if (
        not lib_was_rebuilt
        and sketch_pcm_path.exists()
        and pch_codegen_o.exists()
        and sketch_pch_hash_path.exists()
    ):
        stored_hash = sketch_pch_hash_path.read_text(encoding="utf-8").strip()
        if stored_hash == current_hash:
            if verbose:
                print("[WASM] Sketch header unit is up-to-date")
            return sketch_pcm_path

    includes = [
        f"-I{PROJECT_ROOT / 'src'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
    ]

    # Meson-injected defaults that must match the compilation environment
    meson_defaults = [
        "-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST",
        "-D_FILE_OFFSET_BITS=64",
    ]

    emcc_args = (
        ["-fmodule-header=user", str(WASM_PCH_HEADER), "-o", str(sketch_pcm_path)]
        + meson_defaults
        + sketch_flags
        + includes
        + ["-Xclang", "-fmodules-codegen"]  # Pre-compile inline function bodies
    )

    if verbose:
        print(f"[WASM] Header unit args: {emcc_args}")

    print("[WASM] Building sketch header unit (.pcm)...")
    rc = run_emcc(emcc_args, cwd=str(PROJECT_ROOT))
    if rc != 0:
        print("[WASM] Header unit build failed")
        return None

    # Compile the .pcm to extract pre-compiled inline function bodies.
    # This produces a companion .o that the linker uses instead of
    # re-compiling inline functions in every sketch TU.
    print("[WASM] Compiling header unit codegen companion...")
    rc = run_emcc(
        ["-c", str(sketch_pcm_path), "-o", str(pch_codegen_o), "-O0", "-g0"],
        cwd=str(PROJECT_ROOT),
    )
    if rc != 0:
        print("[WASM] Codegen companion build failed, continuing without it")
        pch_codegen_o.unlink(missing_ok=True)
    else:
        # Add codegen companion into libfastled.a so linker picks up symbols.
        library_archive = build_dir / "ci" / "meson" / "wasm" / "libfastled.a"
        if library_archive.exists():
            from ci.wasm_tools import get_emar

            emar = get_emar()
            subprocess.run(
                [emar, "r", str(library_archive), str(pch_codegen_o)],
                cwd=str(PROJECT_ROOT),
                check=False,
            )

    sketch_pch_hash_path.write_text(current_hash, encoding="utf-8")
    print("[WASM] Sketch header unit built successfully")
    return sketch_pcm_path


def _parse_clang_from_verbose(stderr_text: str) -> list[str] | None:
    """Parse the clang command from emcc verbose stderr output.

    emcc with EMCC_VERBOSE=1 prints subprocess commands to stderr.
    We look for the clang invocation that has -c (compile mode).
    """
    import shlex
    import sys

    # On Windows, use posix=False so backslash paths aren't treated as escape chars.
    # posix=False preserves literal quotes in tokens — strip them afterwards.
    posix = sys.platform != "win32"

    for line in stderr_text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        parts = stripped.split()
        if parts and "clang" in parts[0].lower() and "-c" in parts:
            try:
                tokens = shlex.split(stripped, posix=posix)
                if not posix:
                    # posix=False keeps outer quotes on tokens — strip them
                    tokens = [t.strip("'\"") for t in tokens]
                return tokens
            except ValueError:
                continue
    return None


def _compile_cache_key(emcc_args: list[str]) -> str:
    """Hash the emcc args to detect when flags change and cache needs refresh."""
    return hashlib.sha256(json.dumps(emcc_args).encode()).hexdigest()[:16]


def _fast_compile(
    wrapper_path: Path,
    object_path: Path,
    build_dir: Path,
    emcc_args: list[str],
    verbose: bool = False,
) -> bool:
    """Fast compile using cached clang args, bypassing emcc Python overhead.

    On first compile, emcc is run with EMCC_VERBOSE=1 to capture the clang
    command it invokes internally. The command is saved as a template with
    {input_cpp} and {output_o} placeholders. Subsequent compiles call clang
    directly (~0.45s vs ~0.9s with emcc).

    Returns True on success, False to fall back to full emcc compile.
    """
    cache_file = build_dir / "clang_compile_args.json"
    cache_key_file = build_dir / "clang_compile_args.key"
    if not cache_file.exists():
        return False

    # Invalidate cache if emcc args changed (new defines, flags, etc.)
    current_key = _compile_cache_key(emcc_args)
    if cache_key_file.exists():
        stored_key = cache_key_file.read_text(encoding="utf-8").strip()
        if stored_key != current_key:
            cache_file.unlink(missing_ok=True)
            cache_key_file.unlink(missing_ok=True)
            return False

    try:
        template_args: list[str] = json.loads(cache_file.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return False

    cmd: list[str] = []
    for arg in template_args:
        arg = arg.replace("{input_cpp}", str(wrapper_path))
        arg = arg.replace("{output_o}", str(object_path))
        cmd.append(arg)

    if verbose:
        print(f"[WASM] Fast compile cmd: {cmd[:3]}...({len(cmd)} args)")

    try:
        result = subprocess.run(cmd, cwd=str(PROJECT_ROOT))
    except OSError:
        # Executable not found (e.g. stale cache with bad path) — fall back
        cache_file.unlink(missing_ok=True)
        cache_key_file.unlink(missing_ok=True)
        return False
    if result.returncode != 0:
        # Cache might be stale — delete it so next run recaptures
        cache_file.unlink(missing_ok=True)
        cache_key_file.unlink(missing_ok=True)
        return False
    return True


def _intercept_emcc_compile(
    emcc_args: list[str],
    wrapper_path: Path,
    object_path: Path,
    build_dir: Path,
) -> int:
    """Run emcc compile with verbose output to capture clang command.

    Runs emcc normally but with EMCC_VERBOSE=1 and EM_FORCE_RESPONSE_FILES=0
    so the full clang subprocess command is printed to stderr. We parse it
    and save a template for future fast compiles via _fast_compile().

    Returns the emcc exit code.
    """
    from ci.wasm_tools import get_emcc

    emcc = get_emcc()

    env = os.environ.copy()
    env["EMCC_VERBOSE"] = "1"
    env["EM_FORCE_RESPONSE_FILES"] = "0"

    result = subprocess.run(
        [emcc] + emcc_args,
        cwd=str(PROJECT_ROOT),
        stderr=subprocess.PIPE,
        text=True,
        env=env,
    )

    if result.returncode != 0:
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        return result.returncode

    # Parse and cache the clang command for future fast compiles
    clang_cmd = _parse_clang_from_verbose(result.stderr)
    if clang_cmd is None:
        return 0  # compile succeeded, just can't cache

    wrapper_str = str(wrapper_path)
    object_str = str(object_path)
    template: list[str] = []
    for arg in clang_cmd:
        arg = arg.replace(wrapper_str, "{input_cpp}")
        arg = arg.replace(object_str, "{output_o}")
        template.append(arg)

    cache_file = build_dir / "clang_compile_args.json"
    cache_file.write_text(json.dumps(template), encoding="utf-8")

    # Save the key so _fast_compile can detect flag changes
    cache_key_file = build_dir / "clang_compile_args.key"
    cache_key_file.write_text(_compile_cache_key(emcc_args), encoding="utf-8")

    return 0


def compile_sketch(
    wrapper_path: Path,
    build_dir: Path,
    sketch_cache_dir: Path,
    mode: str,
    verbose: bool = False,
) -> Path | None:
    """
    Compile the sketch wrapper to an object file.

    Two-tier compilation strategy:
    1. Fast path: run clang directly (~0.45s), bypassing emcc Python overhead
    2. Full path: run emcc with verbose capture (~0.9s), saves clang cmd for next time

    Sketch artifacts are cached per-sketch in sketch_cache_dir.
    Returns path to the object file, or None on failure.
    """
    object_path = sketch_cache_dir / "sketch.o"

    # Check if recompilation needed (mtime check against wrapper, .ino, and library)
    library_archive = build_dir / "ci" / "meson" / "wasm" / "libfastled.a"
    # Find the .ino file that the wrapper #includes
    example_name = wrapper_path.stem.replace("_wrapper", "")
    ino_file = PROJECT_ROOT / "examples" / example_name / f"{example_name}.ino"
    if object_path.exists():
        object_mtime = object_path.stat().st_mtime
        wrapper_mtime = wrapper_path.stat().st_mtime
        ino_mtime = ino_file.stat().st_mtime if ino_file.exists() else 0
        lib_mtime = library_archive.stat().st_mtime if library_archive.exists() else 0
        if (
            wrapper_mtime <= object_mtime
            and ino_mtime <= object_mtime
            and lib_mtime <= object_mtime
        ):
            print("[WASM] Sketch is up-to-date")
            return object_path

    sketch_flags = get_sketch_compile_flags(mode)

    includes = [
        f"-I{PROJECT_ROOT / 'src'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
    ]

    # Header unit usage: use the .pcm BMI built by build_sketch_pch()
    sketch_pcm_path = build_dir / "wasm_pch.h.pcm"
    pch_args = []
    if sketch_pcm_path.exists():
        pch_args = [
            f"-fmodule-file={sketch_pcm_path}",
        ]

    emcc_args = (
        ["-c", str(wrapper_path), "-o", str(object_path)]
        + sketch_flags
        + includes
        + pch_args
    )

    print(f"[WASM] Compiling sketch: {wrapper_path.name}")

    # Native path: ctc-emcc handles command capture + caching internally.
    # Only one command capture ever happens (inside ctc-emcc's auto-cache).
    from ci.wasm_tools import get_native_emcc

    native_emcc = get_native_emcc()
    if native_emcc is not None:
        if verbose:
            print(f"[WASM] Using native emcc: {native_emcc}")
        result = subprocess.run(
            [native_emcc] + emcc_args,
            cwd=str(PROJECT_ROOT),
        )
        if result.returncode == 0:
            return object_path
        print(f"[WASM] Sketch compilation failed with return code {result.returncode}")
        return None

    # Python fallback (no native tools): two-tier intercept
    if _fast_compile(wrapper_path, object_path, build_dir, emcc_args, verbose):
        return object_path

    if verbose:
        print(f"[WASM] Compile args: {emcc_args}")

    rc = _intercept_emcc_compile(emcc_args, wrapper_path, object_path, build_dir)
    if rc != 0:
        print(f"[WASM] Sketch compilation failed with return code {rc}")
        return None

    return object_path


def _parse_wasm_ld_from_verbose(stderr_text: str) -> list[str] | None:
    """Parse the wasm-ld command from emcc verbose stderr output.

    emcc with EMCC_VERBOSE=1 prints subprocess commands as:
      /path/to/wasm-ld.exe arg1 arg2 ...
    """
    import shlex
    import sys

    # On Windows, use posix=False so backslash paths aren't treated as escape chars.
    # posix=False preserves literal quotes in tokens — strip them afterwards.
    posix = sys.platform != "win32"

    for line in stderr_text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        # emcc verbose output prefixes commands with a space
        if "wasm-ld" in stripped.split()[0] if stripped.split() else False:
            try:
                tokens = shlex.split(stripped, posix=posix)
                if not posix:
                    # posix=False keeps outer quotes on tokens — strip them
                    tokens = [t.strip("'\"") for t in tokens]
                return tokens
            except ValueError:
                continue
    return None


def _intercept_emcc_link(
    emcc_args: list[str],
    sketch_object: Path,
    cached_wasm: Path,
    build_dir: Path,
    cwd: str,
) -> int:
    """Run emcc link as subprocess with verbose output to capture wasm-ld command.

    Uses EMCC_VERBOSE=1 to make emcc print the wasm-ld command to stderr,
    then saves it (with placeholders) for future fast re-linking via _fast_link().

    This is a one-time operation per build directory — subsequent links use
    _fast_link() which runs wasm-ld directly (~0.2s vs ~1.1s).
    """
    from ci.wasm_tools import get_emcc

    emcc = get_emcc()

    # Set up env to capture wasm-ld command and preserve temp files
    emcc_tmp = build_dir / "emcc_tmp"
    emcc_tmp.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["EMCC_VERBOSE"] = "1"
    env["EMCC_DEBUG"] = "1"  # preserves temp files (js_symbols stub)
    env["EMCC_TEMP_DIR"] = str(emcc_tmp)
    env["EM_FORCE_RESPONSE_FILES"] = "0"  # ensure full command, no @file

    result = subprocess.run(
        [emcc] + emcc_args,
        cwd=cwd,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
    )

    if result.returncode != 0:
        # Print captured stderr so user sees the error
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        return result.returncode

    # Parse wasm-ld command from verbose output
    wasm_ld_cmd = _parse_wasm_ld_from_verbose(result.stderr)
    if wasm_ld_cmd is None:
        print("[WASM] Warning: could not capture wasm-ld command from verbose output")
        return 0  # link succeeded, just can't cache the fast path

    # Copy js_symbols stub from temp dir to build_dir for persistence
    emcc_temp_dir = emcc_tmp / "emscripten_temp"
    cached_stub = build_dir / "libemscripten_js_symbols.so"
    for i, arg in enumerate(wasm_ld_cmd):
        if "js_symbols" in arg:
            stub_src = Path(arg)
            if stub_src.exists():
                shutil.copy2(str(stub_src), str(cached_stub))
                wasm_ld_cmd[i] = str(cached_stub)
            break

    # Save wasm-ld command template with placeholders
    sketch_o_str = str(sketch_object)
    wasm_str = str(cached_wasm)
    template: list[str] = []
    for arg in wasm_ld_cmd:
        arg = arg.replace(sketch_o_str, "{sketch_o}")
        arg = arg.replace(wasm_str, "{output_wasm}")
        template.append(arg)
    cache_file = build_dir / "wasm_ld_args.json"
    cache_file.write_text(json.dumps(template), encoding="utf-8")

    # Clean up emcc temp dir (we've saved what we need)
    try:
        shutil.rmtree(str(emcc_temp_dir), ignore_errors=True)
    except OSError:
        pass

    return 0


def _fast_link(
    sketch_object: Path,
    cached_wasm: Path,
    build_dir: Path,
    verbose: bool = False,
) -> bool:
    """Fast link using cached wasm-ld args + cached JS glue.

    Bypasses emcc entirely (~0.2s vs ~1.1s).
    Returns True on success, False to fall back to full emcc link.
    """
    cached_js_glue = build_dir / "fastled_glue.js"
    if not cached_js_glue.exists():
        return False

    cache_file = build_dir / "wasm_ld_args.json"
    if not cache_file.exists():
        return False

    # Check that the js_symbols stub exists
    cached_stub = build_dir / "libemscripten_js_symbols.so"
    if not cached_stub.exists():
        return False

    try:
        template_args: list[str] = json.loads(cache_file.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return False

    # Substitute placeholders and upgrade --strip-debug → --strip-all
    # (safe because all needed symbols use explicit --export flags)
    cmd: list[str] = []
    for arg in template_args:
        arg = arg.replace("{sketch_o}", str(sketch_object))
        arg = arg.replace("{output_wasm}", str(cached_wasm))
        if arg == "--strip-debug":
            arg = "--strip-all"
        cmd.append(arg)

    if verbose:
        print(f"[WASM] Fast link cmd: {cmd}")

    print("[WASM] Fast linking (wasm-ld only)...")
    try:
        result = subprocess.run(cmd, cwd=str(PROJECT_ROOT))
    except OSError:
        # Executable not found (e.g. stale cache with bad path) — fall back
        cache_file.unlink(missing_ok=True)
        return False
    if result.returncode != 0:
        print("[WASM] Fast link failed, falling back to full emcc link")
        return False

    # Copy cached JS glue to sketch cache dir (use copy, not copy2,
    # so mtime is current — prevents stale mtime from triggering re-links)
    js_dest = cached_wasm.parent / "fastled.js"
    shutil.copy(str(cached_js_glue), str(js_dest))
    return True


def link_wasm(
    sketch_object: Path,
    build_dir: Path,
    sketch_cache_dir: Path,
    output_js: Path,
    mode: str,
    verbose: bool = False,
) -> bool:
    """
    Link sketch.o + libfastled.a → fastled.js + fastled.wasm.

    Two-tier linking strategy:
    1. Fast path: reuse cached JS glue + run wasm-ld directly (~0.2s)
    2. Full path: run emcc to generate JS glue + wasm (~1.1s), then cache

    The JS glue is identical across all sketches (no EM_ASM/EM_JS), so after
    one full link the JS is cached and subsequent links skip emcc entirely.

    Returns True on success.
    """
    library_archive = build_dir / "ci" / "meson" / "wasm" / "libfastled.a"

    if not library_archive.exists():
        print(f"[WASM] Library not found: {library_archive}")
        return False

    # Intermediate linked output in per-sketch cache
    cached_js = sketch_cache_dir / "fastled.js"
    cached_wasm = sketch_cache_dir / "fastled.wasm"

    # Check if linking needed (compare inputs vs cached output)
    if cached_js.exists() and cached_wasm.exists():
        output_mtime = min(cached_js.stat().st_mtime, cached_wasm.stat().st_mtime)
        inputs_newer = (
            sketch_object.stat().st_mtime > output_mtime
            or library_archive.stat().st_mtime > output_mtime
        )
        if not inputs_newer:
            _copy_linked_output(sketch_cache_dir, output_js)
            print("[WASM] Link output up-to-date (cached)")
            return True

    # Fast path: reuse cached JS glue + run wasm-ld directly
    if _fast_link(sketch_object, cached_wasm, build_dir, verbose):
        _copy_linked_output(sketch_cache_dir, output_js)
        print(f"[WASM] Output: {output_js}")
        return True

    # Full emcc link — generates JS glue + wasm, captures wasm-ld command
    link_flags = get_link_flags(mode)

    js_library = (
        PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "js_library.js"
    )

    includes = [
        f"-I{PROJECT_ROOT / 'src'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
    ]

    emcc_args = (
        [str(sketch_object), str(library_archive)]
        + includes
        + [f"--js-library={js_library}"]
        + ["-o", str(cached_js)]
        + link_flags
    )

    if verbose:
        print(f"[WASM] Link args: {emcc_args}")

    print("[WASM] Linking final WASM module...")
    rc = _intercept_emcc_link(
        emcc_args, sketch_object, cached_wasm, build_dir, str(PROJECT_ROOT)
    )
    if rc != 0:
        print(f"[WASM] Linking failed with return code {rc}")
        return False

    # Cache JS glue for fast re-linking (identical across all sketches)
    cached_js_glue = build_dir / "fastled_glue.js"
    shutil.copy2(str(cached_js), str(cached_js_glue))

    _copy_linked_output(sketch_cache_dir, output_js)
    print(f"[WASM] Output: {output_js}")
    return True


def _copy_linked_output(sketch_cache_dir: Path, output_js: Path) -> None:
    """Copy linked .js and .wasm from sketch cache to final output directory."""
    output_dir = output_js.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    cached_js = sketch_cache_dir / "fastled.js"
    cached_wasm = sketch_cache_dir / "fastled.wasm"

    if cached_js.exists():
        shutil.copy2(str(cached_js), str(output_js))
    if cached_wasm.exists():
        shutil.copy2(str(cached_wasm), str(output_js.with_suffix(".wasm")))

    # Copy any other generated files (worker JS, etc.)
    for f in sketch_cache_dir.iterdir():
        if f.suffix in (".js", ".wasm") and f.name not in (
            "fastled.js",
            "fastled.wasm",
        ):
            shutil.copy2(str(f), str(output_dir / f.name))


def _copy_from_dist(dist_dir: Path, output_dir: Path) -> None:
    """Copy Vite build output from dist/ to the output directory."""
    for item in dist_dir.iterdir():
        dst = output_dir / item.name
        if item.is_dir():
            if dst.exists():
                shutil.rmtree(dst)
            shutil.copytree(item, dst)
        else:
            shutil.copy2(item, dst)


def _run_npm(args: list[str], cwd: Path) -> int:
    """Run npm via nodejs-wheel."""
    from nodejs_wheel import npm as npm_func  # type: ignore[import-untyped]

    return npm_func(args, cwd=str(cwd))


def _run_npx(args: list[str], cwd: Path) -> int:
    """Run npx via nodejs-wheel."""
    from nodejs_wheel import npx as npx_func  # type: ignore[import-untyped]

    return npx_func(args, cwd=str(cwd))


def _get_vite_source_mtime(template_dir: Path) -> float:
    """Get the most recent mtime of any Vite frontend source file.

    Uses os.scandir with directory-level pruning instead of rglob to avoid
    walking node_modules (2626 files, 420ms → 4ms). On Windows, DirEntry.stat()
    reuses FindFirstFile data (no extra syscall per file).
    """
    max_mtime = 0.0
    _EXTS = frozenset((".ts", ".js", ".html", ".css", ".json"))
    _SKIP = frozenset(("node_modules", "dist"))
    stack = [str(template_dir)]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as it:
                for entry in it:
                    try:
                        if entry.is_dir(follow_symlinks=False):
                            if entry.name not in _SKIP:
                                stack.append(entry.path)
                        elif entry.is_file(follow_symlinks=False):
                            _, ext = os.path.splitext(entry.name)
                            if ext in _EXTS:
                                mtime = entry.stat(follow_symlinks=False).st_mtime
                                if mtime > max_mtime:
                                    max_mtime = mtime
                    except OSError:
                        pass
        except OSError:
            pass
    return max_mtime


def copy_templates(output_dir: Path) -> None:
    """Copy template files to the output directory.

    Requires Vite to build the TypeScript frontend into browser-runnable JS.
    Caches the Vite build output — skips rebuild if sources haven't changed.
    Raises RuntimeError if Vite build is not possible.
    """
    template_dir = PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler"

    if not (template_dir / "node_modules").exists():
        print("[WASM] Installing frontend dependencies (npm install)...")
        rc = _run_npm(["install"], cwd=template_dir)
        if rc != 0:
            raise RuntimeError("npm install failed")

    dist_dir = template_dir / "dist"
    # Check if output_dir already has Vite assets and they're up-to-date
    output_index = output_dir / "index.html"
    needs_build = True
    if dist_dir.exists() and output_index.exists():
        source_mtime = _get_vite_source_mtime(template_dir)
        output_mtime = output_index.stat().st_mtime
        if source_mtime <= output_mtime:
            needs_build = False

    if needs_build:
        print("[WASM] Building frontend with Vite...")
        rc = _run_npx(["vite", "build"], cwd=template_dir)
        if rc != 0:
            raise RuntimeError("Vite build failed")

        if not dist_dir.exists():
            raise RuntimeError(
                "Vite build succeeded but dist/ directory was not created."
            )

        print("[WASM] Copying Vite build output...")
        _copy_from_dist(dist_dir, output_dir)
    else:
        print("[WASM] Frontend assets up-to-date, skipping Vite build")


def generate_manifest(example_name: str, output_dir: Path) -> None:
    """Generate files.json manifest for data files."""
    example_dir = PROJECT_ROOT / "examples" / example_name
    data_extensions = frozenset(
        (".json", ".csv", ".txt", ".cfg", ".bin", ".dat", ".mp3", ".wav")
    )
    data_files: list[dict[str, str | int]] = []
    _SKIP = frozenset(("fastled_js", ".build"))

    # Use os.scandir with directory pruning — avoids walking fastled_js/ and .build/
    # On Windows, DirEntry.stat() reuses FindFirstFile data (no extra syscall)
    example_str = str(example_dir)
    example_len = len(example_str)
    if not example_str.endswith(os.sep):
        example_len += 1
    stack = [example_str]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as it:
                for entry in it:
                    try:
                        if entry.is_dir(follow_symlinks=False):
                            if entry.name not in _SKIP:
                                stack.append(entry.path)
                        elif entry.is_file(follow_symlinks=False):
                            _, ext = os.path.splitext(entry.name)
                            if ext.lower() in data_extensions:
                                rel_path = entry.path[example_len:]
                                size = entry.stat(follow_symlinks=False).st_size
                                data_files.append({"path": rel_path, "size": size})
                    except OSError:
                        pass
        except OSError:
            pass

    files_json = output_dir / "files.json"
    with open(files_json, "w", encoding="utf-8") as f:
        json.dump(data_files, f, indent=2)


def build(
    example: str,
    output: str,
    mode: str = "quick",
    verbose: bool = False,
    force: bool = False,
) -> int:
    """Core build function — callable directly without argparse overhead."""
    try:
        start_time = time.time()
        build_dir = get_build_dir(mode)
        sketch_cache_dir = get_sketch_cache_dir(example)
        output_js = Path(output)
        output_dir = output_js.parent

        print(f"[WASM] Building {example} (mode: {mode})")

        # Step 1: Configure meson
        if not ensure_meson_configured(build_dir, mode, force):
            return 1

        # Step 2: Build library via ninja (skipped if source fingerprint matches)
        lib_start = time.time()
        lib_ok, lib_was_rebuilt = build_library(build_dir, verbose)
        if not lib_ok:
            return 1

        # Step 2b: Build sketch-specific PCH (invalidated when library sources change)
        sketch_pch = build_sketch_pch(
            build_dir, mode, lib_was_rebuilt=lib_was_rebuilt, verbose=verbose
        )
        if sketch_pch is None:
            print("[WASM] WARNING: Sketch PCH build failed, continuing without PCH")
        lib_time = time.time() - lib_start

        # Step 3: Create wrapper and compile sketch (per-sketch cache)
        sketch_start = time.time()
        wrapper = create_wrapper(example, sketch_cache_dir)
        sketch_obj = compile_sketch(wrapper, build_dir, sketch_cache_dir, mode, verbose)
        if sketch_obj is None:
            return 1
        sketch_time = time.time() - sketch_start

        # Step 4: Link (per-sketch cache, copy to output)
        link_start = time.time()
        output_dir.mkdir(parents=True, exist_ok=True)
        if not link_wasm(
            sketch_obj, build_dir, sketch_cache_dir, output_js, mode, verbose
        ):
            return 1
        link_time = time.time() - link_start

        # Step 5: Copy templates and generate manifest
        copy_templates(output_dir)
        generate_manifest(example, output_dir)

        total_time = time.time() - start_time
        print(f"\n[WASM] Build successful!")
        print(f"  Library:  {lib_time:.2f}s")
        print(f"  Sketch:   {sketch_time:.2f}s")
        print(f"  Linking:  {link_time:.2f}s")
        print(f"  Total:    {total_time:.2f}s")
        return 0

    except KeyboardInterrupt:
        print("\n[WASM] Build interrupted by user")
        raise
    except Exception as e:
        print(f"[WASM] Build failed: {e}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        return 1


def main() -> int:
    """CLI entry point — parses args and delegates to build()."""
    parser = argparse.ArgumentParser(description="Build WASM example")
    parser.add_argument("--example", required=True, help="Example name (e.g., Blink)")
    parser.add_argument("-o", "--output", required=True, help="Output JS file path")
    parser.add_argument(
        "--mode",
        default="quick",
        choices=["quick", "debug", "release"],
        help="Build mode",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument(
        "--force", action="store_true", help="Force meson reconfiguration"
    )
    args = parser.parse_args()
    return build(args.example, args.output, args.mode, args.verbose, args.force)


if __name__ == "__main__":
    sys.exit(main())
