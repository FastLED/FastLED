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
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

from ci.wasm_tools import get_emcc


PROJECT_ROOT = Path(__file__).parent.parent
CROSS_FILE = PROJECT_ROOT / "ci" / "meson" / "wasm_cross_file.ini"

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


def get_sketch_compile_args(build_dir: Path) -> list[str]:
    """
    Extract sketch compilation flags from meson introspection.

    Falls back to reading them from the meson.build variables via
    the built library's compile commands.
    """
    # Use meson introspect to get the compile args from the fastled target
    # This ensures flags stay in sync with meson.build
    try:
        result = subprocess.run(
            [get_meson_executable(), "introspect", "--targets", str(build_dir)],
            capture_output=True,
            text=True,
            cwd=PROJECT_ROOT,
        )
        if result.returncode == 0:
            targets = json.loads(result.stdout)
            for target in targets:
                if (
                    target.get("name") == "fastled"
                    and target.get("type") == "static library"
                ):
                    # Found the library target - get its compile args
                    # We can extract them from the build commands
                    break
    except (json.JSONDecodeError, KeyError, subprocess.SubprocessError):
        pass

    # Fallback: construct flags directly (these must match meson.build wasm_sketch_compile_args)
    # This is the authoritative list, kept in sync with meson.build
    return _get_wasm_sketch_flags(build_dir)


def _get_wasm_sketch_flags(build_dir: Path) -> list[str]:
    """
    Get WASM sketch compilation flags.

    These MUST match the wasm_sketch_compile_args in meson.build.
    Any changes to meson.build flags must be reflected here.
    """
    defines = [
        "-DFASTLED_ENGINE_EVENTS_MAX_LISTENERS=50",
        "-DFASTLED_FORCE_NAMESPACE=1",
        "-DFASTLED_USE_PROGMEM=0",
        "-DUSE_OFFSET_CONVERTER=0",
        "-DGL_ENABLE_GET_PROC_ADDRESS=0",
        "-D_REENTRANT=1",
        "-DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0",
        # Sketch-specific
        "-DSKETCH_COMPILE=1",
        "-DFASTLED_WASM_USE_CCALL",
    ]

    flags = [
        "-std=gnu++17",
        "-fpermissive",
        "-Wno-constant-logical-operand",
        "-Wnon-c-typedef-for-linkage",
        "-Werror=bad-function-cast",
        "-Werror=cast-function-type",
        "-fno-threadsafe-statics",
        "-fno-exceptions",
        "-fno-rtti",
        "-pthread",
        "-fpch-instantiate-templates",
    ]

    includes = [
        f"-I{PROJECT_ROOT / 'src'}",
        f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
    ]

    # PCH usage
    pch_path = build_dir / "wasm_pch.h.pch"
    pch_args = []
    if pch_path.exists():
        pch_args = [
            "-include-pch",
            str(pch_path),
            "-Werror=invalid-pch",
            "-fpch-validate-input-files-content",
        ]

    return defines + flags + includes + pch_args


def _get_wasm_mode_flags(mode: str) -> list[str]:
    """Get mode-specific compilation flags."""
    if mode == "debug":
        return ["-g3", "-gsource-map", "-fno-inline", "-O0"]
    elif mode == "quick":
        return [
            "-flto=thin",
            "-O1",
            "-g0",
            "-fno-inline-functions",
            "-fno-vectorize",
            "-fno-unroll-loops",
            "-fno-strict-aliasing",
            "-fno-delayed-template-parsing",
            "-fmax-type-align=4",
            "-ffast-math",
            "-fno-finite-math-only",
            "-fno-math-errno",
            "-fno-exceptions",
            "-fno-rtti",
        ]
    elif mode == "release":
        return ["-Oz"]
    return []


def _get_wasm_link_flags(mode: str) -> list[str]:
    """Get all WASM link flags (base + sketch + mode-specific)."""
    base = [
        "-sWASM=1",
        "-fuse-ld=wasm-ld",
        "-pthread",
        "-sUSE_PTHREADS=1",
        "-sPROXY_TO_PTHREAD",
    ]

    sketch = [
        "-sMODULARIZE=1",
        "-sEXPORT_NAME=fastled",
        "-sALLOW_MEMORY_GROWTH=1",
        "-sINITIAL_MEMORY=134217728",
        "-sAUTO_NATIVE_LIBRARIES=0",
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','UTF8ToString','lengthBytesUTF8','HEAPU8','getValue']",
        "-sEXPORTED_FUNCTIONS=['_malloc','_free','_main','_extern_setup','_extern_loop','_fastled_declare_files','_getStripPixelData','_getFrameData','_getScreenMapData','_freeFrameData','_getFrameVersion','_hasNewFrameData','_js_fetch_success_callback','_js_fetch_error_callback','_pushAudioSamples']",
        "-sEXIT_RUNTIME=0",
        "-sFILESYSTEM=0",
        "-Wl,--strip-debug",
        "-Wl,--no-export-dynamic",
    ]

    if mode == "debug":
        mode_link = [
            "--profiling-funcs",
            "-sSEPARATE_DWARF_URL=fastled.wasm.dwarf",
            "-sSTACK_OVERFLOW_CHECK=2",
            "-sASSERTIONS=1",
        ]
    elif mode == "quick":
        mode_link = [
            "-Wl,--thinlto-cache-dir=build/wasm/lto_cache",
            "--profiling-funcs",
            "--emit-symbol-map",
            "-sINITIAL_MEMORY=67108864",
            "-sASSERTIONS=0",
            "-sSTACK_OVERFLOW_CHECK=0",
            "-sWASM_BIGINT=0",
            "-sERROR_ON_UNDEFINED_SYMBOLS=1",
        ]
    elif mode == "release":
        mode_link = []
    else:
        mode_link = []

    return base + sketch + mode_link


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

    # Check if recompilation needed (simple mtime check)
    if object_path.exists() and not verbose:
        wrapper_mtime = wrapper_path.stat().st_mtime
        object_mtime = object_path.stat().st_mtime
        if wrapper_mtime <= object_mtime:
            print("[WASM] Sketch is up-to-date")
            return object_path

    sketch_flags = _get_wasm_sketch_flags(build_dir) + _get_wasm_mode_flags(mode)

    cmd = [emcc, "-c", str(wrapper_path), "-o", str(object_path)] + sketch_flags

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

    link_flags = _get_wasm_link_flags(mode)

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


def copy_templates(output_dir: Path) -> None:
    """Copy template files to the output directory."""
    template_dir = PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler"

    # Individual files
    for name in [
        "index.html",
        "index.css",
        "index.js",
        "jsconfig.json",
        "types.d.ts",
        "emscripten.d.ts",
    ]:
        src = template_dir / name
        if src.exists():
            shutil.copy2(src, output_dir / name)

    # Directories
    for dirname in ["modules", "vendor"]:
        src = template_dir / dirname
        dst = output_dir / dirname
        if src.exists():
            if dst.exists():
                shutil.rmtree(dst)
            shutil.copytree(src, dst)


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
