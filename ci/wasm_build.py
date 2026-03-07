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

Usage:
    uv run python ci/wasm_build.py --example Blink -o examples/Blink/fastled_js/fastled.js
    uv run python ci/wasm_build.py --example Blink -o output.js --mode debug
    uv run python ci/wasm_build.py --example Blink -o output.js --force
"""

# pyright: reportMissingImports=false

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

from ci.wasm_flags import get_link_flags, get_sketch_compile_flags
from ci.wasm_tools import get_emcc


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


def get_meson_executable() -> str:
    """Get the meson executable path."""
    # Use the same meson that the native build uses
    return "meson"


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


def build_library(build_dir: Path, verbose: bool = False) -> bool:
    """
    Build libfastled.a using ninja.

    Returns True if build succeeded.
    """
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
        return False

    print("[WASM] Library build successful")
    return True


def create_wrapper(example_name: str, build_dir: Path) -> Path:
    """
    Create a wrapper .cpp that includes the sketch .ino file.

    Returns path to the wrapper file.
    """
    example_dir = PROJECT_ROOT / "examples" / example_name
    ino_file = example_dir / f"{example_name}.ino"

    if not ino_file.exists():
        raise FileNotFoundError(f"Example not found: {ino_file}")

    wrapper_path = build_dir / f"{example_name}_wrapper.cpp"
    wrapper_content = f"""// Auto-generated wrapper for {example_name}.ino
// For WASM builds, we use the standard entry point from platforms/wasm/entry_point.cpp
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
    verbose: bool = False,
) -> Path | None:
    """
    Build a sketch-specific PCH with sketch compile flags.

    The library PCH uses library flags (e.g. -O1 -flto=thin) which are
    incompatible with sketch flags (e.g. -O0). This builds a separate PCH
    for sketch compilation so the sketch can use PCH despite different flags.

    Returns path to the sketch PCH, or None if build fails.
    """
    emcc = get_emcc()
    sketch_pch_path = build_dir / "sketch_pch.h.pch"
    sketch_pch_flags_hash_path = build_dir / "sketch_pch.flags_hash"

    sketch_flags = get_sketch_compile_flags(mode)
    flags_hash = hashlib.sha256("\n".join(sketch_flags).encode()).hexdigest()

    # Check if sketch PCH is up-to-date
    if sketch_pch_path.exists() and sketch_pch_flags_hash_path.exists():
        stored_hash = sketch_pch_flags_hash_path.read_text(encoding="utf-8").strip()
        if stored_hash == flags_hash:
            pch_mtime = sketch_pch_path.stat().st_mtime
            header_mtime = WASM_PCH_HEADER.stat().st_mtime
            if header_mtime <= pch_mtime:
                if verbose:
                    print("[WASM] Sketch PCH is up-to-date")
                return sketch_pch_path

    includes = [
        f"-I{PROJECT_ROOT / 'src'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
    ]

    # Meson-injected defaults that must match the compilation environment
    meson_defaults = [
        "-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST",
        "-D_FILE_OFFSET_BITS=64",
    ]

    cmd = (
        [emcc, "-x", "c++-header", str(WASM_PCH_HEADER), "-o", str(sketch_pch_path)]
        + meson_defaults
        + sketch_flags
        + includes
    )

    if verbose:
        print(f"[WASM] Sketch PCH: {subprocess.list2cmdline(cmd)}")

    print("[WASM] Building sketch PCH...")
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if result.returncode != 0:
        print("[WASM] Sketch PCH build failed")
        return None

    sketch_pch_flags_hash_path.write_text(flags_hash, encoding="utf-8")
    print("[WASM] Sketch PCH built successfully")
    return sketch_pch_path


def compile_sketch(
    wrapper_path: Path,
    build_dir: Path,
    mode: str,
    verbose: bool = False,
) -> Path | None:
    """
    Compile the sketch wrapper to an object file.

    Returns path to the object file, or None on failure.
    """
    emcc = get_emcc()
    object_path = build_dir / "sketch.o"

    # Check if recompilation needed (mtime check against wrapper and library)
    library_archive = build_dir / "ci" / "meson" / "wasm" / "libfastled.a"
    if object_path.exists() and not verbose:
        object_mtime = object_path.stat().st_mtime
        wrapper_mtime = wrapper_path.stat().st_mtime
        lib_mtime = library_archive.stat().st_mtime if library_archive.exists() else 0
        if wrapper_mtime <= object_mtime and lib_mtime <= object_mtime:
            print("[WASM] Sketch is up-to-date")
            return object_path

    sketch_flags = get_sketch_compile_flags(mode)

    includes = [
        f"-I{PROJECT_ROOT / 'src'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
    ]

    # PCH usage: prefer sketch-specific PCH (compiled with sketch flags),
    # fall back to library PCH (Meson places it under ci/meson/wasm/)
    sketch_pch_path = build_dir / "sketch_pch.h.pch"
    lib_pch_path = build_dir / "ci" / "meson" / "wasm" / "wasm_pch.h.pch"
    pch_path = sketch_pch_path if sketch_pch_path.exists() else lib_pch_path
    pch_args = []
    if pch_path.exists():
        pch_args = [
            "-include-pch",
            str(pch_path),
            "-Werror=invalid-pch",
            "-fpch-validate-input-files-content",
        ]

    cmd = (
        [emcc, "-c", str(wrapper_path), "-o", str(object_path)]
        + sketch_flags
        + includes
        + pch_args
    )

    if verbose:
        print(f"[WASM] Compile: {subprocess.list2cmdline(cmd)}")

    print(f"[WASM] Compiling sketch: {wrapper_path.name}")
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if result.returncode != 0:
        print(f"[WASM] Sketch compilation failed with return code {result.returncode}")
        return None

    return object_path


def link_wasm(
    sketch_object: Path,
    build_dir: Path,
    output_js: Path,
    mode: str,
    verbose: bool = False,
) -> bool:
    """
    Link sketch.o + libfastled.a → fastled.js + fastled.wasm.

    Returns True on success.
    """
    emcc = get_emcc()
    library_archive = build_dir / "ci" / "meson" / "wasm" / "libfastled.a"

    if not library_archive.exists():
        print(f"[WASM] Library not found: {library_archive}")
        return False

    # Check if linking needed
    wasm_output = output_js.with_suffix(".wasm")
    if wasm_output.exists() and output_js.exists():
        output_mtime = min(wasm_output.stat().st_mtime, output_js.stat().st_mtime)
        inputs_newer = (
            sketch_object.stat().st_mtime > output_mtime
            or library_archive.stat().st_mtime > output_mtime
        )
        if not inputs_newer:
            print("[WASM] Output is up-to-date, skipping linking")
            return True

    link_flags = get_link_flags(mode)

    includes = [
        f"-I{PROJECT_ROOT / 'src'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
    ]

    cmd = (
        [emcc, str(sketch_object), str(library_archive)]
        + includes
        + ["-o", str(output_js)]
        + link_flags
    )

    if verbose:
        print(f"[WASM] Link: {subprocess.list2cmdline(cmd)}")

    print("[WASM] Linking final WASM module...")
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if result.returncode != 0:
        print(f"[WASM] Linking failed with return code {result.returncode}")
        return False

    print(f"[WASM] Output: {output_js}")
    return True


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


def _copy_templates_legacy(output_dir: Path) -> None:  # noqa: ARG001
    """Legacy direct copy — no longer supported after TypeScript migration.

    Raises RuntimeError because raw .ts files cannot be executed by browsers.
    Vite is required to transpile and bundle the TypeScript frontend.
    """
    raise RuntimeError(
        "Legacy template copy is no longer supported. "
        "The frontend source files are TypeScript and require Vite to build. "
        "Run 'npm install' in src/platforms/wasm/compiler/ to install dependencies, "
        "then re-run the build."
    )


def _get_vite_source_mtime(template_dir: Path) -> float:
    """Get the most recent mtime of any Vite frontend source file."""
    max_mtime = 0.0
    for ext in ("*.ts", "*.js", "*.html", "*.css", "*.json"):
        for f in template_dir.rglob(ext):
            # Skip node_modules and dist directories
            parts = f.relative_to(template_dir).parts
            if "node_modules" in parts or "dist" in parts:
                continue
            max_mtime = max(max_mtime, f.stat().st_mtime)
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
    data_extensions = {".json", ".csv", ".txt", ".cfg", ".bin", ".dat", ".mp3", ".wav"}
    data_files: list[dict[str, str | int]] = []

    for file_path in example_dir.rglob("*"):
        if (
            file_path.is_file()
            and file_path.suffix.lower() in data_extensions
            and "fastled_js" not in file_path.parts
        ):
            rel_path = file_path.relative_to(example_dir)
            data_files.append({"path": str(rel_path), "size": file_path.stat().st_size})

    files_json = output_dir / "files.json"
    with open(files_json, "w", encoding="utf-8") as f:
        json.dump(data_files, f, indent=2)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build FastLED sketch to WASM using Meson + Ninja"
    )
    parser.add_argument("--example", required=True, help="Example name (e.g., Blink)")
    parser.add_argument("-o", "--output", required=True, help="Output .js file path")
    parser.add_argument(
        "--mode",
        default="quick",
        choices=["debug", "quick", "release"],
        help="Build mode (default: quick)",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--force", action="store_true", help="Force rebuild")

    args = parser.parse_args()

    try:
        start_time = time.time()
        build_dir = get_build_dir(args.mode)
        output_js = Path(args.output)
        output_dir = output_js.parent

        print(f"[WASM] Building {args.example} (mode: {args.mode})")

        # Step 1: Configure meson
        if not ensure_meson_configured(build_dir, args.mode, args.force):
            return 1

        # Step 2: Build library via ninja
        lib_start = time.time()
        if not build_library(build_dir, args.verbose):
            return 1

        # Step 2b: Build sketch-specific PCH (uses sketch flags, may differ from library PCH)
        sketch_pch = build_sketch_pch(build_dir, args.mode, args.verbose)
        if sketch_pch is None:
            print("[WASM] WARNING: Sketch PCH build failed, continuing without PCH")
        lib_time = time.time() - lib_start

        # Step 3: Create wrapper and compile sketch
        sketch_start = time.time()
        wrapper = create_wrapper(args.example, build_dir)
        sketch_obj = compile_sketch(wrapper, build_dir, args.mode, args.verbose)
        if sketch_obj is None:
            return 1
        sketch_time = time.time() - sketch_start

        # Step 4: Link
        link_start = time.time()
        output_dir.mkdir(parents=True, exist_ok=True)
        if not link_wasm(sketch_obj, build_dir, output_js, args.mode, args.verbose):
            return 1
        link_time = time.time() - link_start

        # Step 5: Copy templates and generate manifest
        copy_templates(output_dir)
        generate_manifest(args.example, output_dir)

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


if __name__ == "__main__":
    sys.exit(main())
