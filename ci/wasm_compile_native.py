from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Native WASM compilation using clang-tool-chain's emscripten.

This script compiles FastLED sketches to WebAssembly using incremental compilation:
  Phase 1: PCH (precompiled header) - cached
  Phase 2: Library (libfastled.a) - incremental compilation of 251 sources
  Phase 3: Sketch compilation (sketch.cpp + entry_point.cpp -> object files)
  Phase 4: Linking (sketch.o + entry_point.o + libfastled.a -> wasm)

The script reads compilation flags from:
  src/platforms/wasm/compiler/build_flags.toml

Usage:
  uv run ci/wasm_compile_native.py <sketch_wrapper.cpp> -o <output.js>
  uv run ci/wasm_compile_native.py --example Blink -o blink.js
"""

import argparse
import subprocess
import sys
import time
import tomllib
from pathlib import Path

from ci.wasm_tools import get_emcc


# Project root
PROJECT_ROOT = Path(__file__).parent.parent

# Paths
BUILD_FLAGS_TOML = (
    PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "build_flags.toml"
)
BUILD_DIR = PROJECT_ROOT / "build" / "wasm"
LTO_CACHE_DIR = BUILD_DIR / "lto_cache"
ENTRY_POINT_CPP = PROJECT_ROOT / "src" / "platforms" / "wasm" / "entry_point.cpp"


def load_build_flags(
    build_mode: str = "quick", target: str = "sketch"
) -> dict[str, list[str]]:
    """
    Load and parse build flags from build_flags.toml.

    Args:
        build_mode: Build mode (debug, fast_debug, quick, release)
        target: Target type ('sketch' for sketch compilation, 'link' for linking)

    Returns:
        Dictionary with 'defines', 'compiler_flags', 'link_flags' lists
    """
    if not BUILD_FLAGS_TOML.exists():
        raise FileNotFoundError(f"Build flags TOML not found: {BUILD_FLAGS_TOML}")

    with open(BUILD_FLAGS_TOML, "rb") as f:
        config = tomllib.load(f)

    # Collect flags from [all] section
    defines = config.get("all", {}).get("defines", [])
    compiler_flags = config.get("all", {}).get("compiler_flags", [])

    if target == "sketch":
        # Add sketch-specific flags (not library flags)
        defines.extend(config.get("sketch", {}).get("defines", []))
        compiler_flags.extend(config.get("sketch", {}).get("compiler_flags", []))

    # Add build mode-specific flags
    build_mode_config = config.get("build_modes", {}).get(build_mode, {})
    compiler_flags.extend(build_mode_config.get("flags", []))

    # Get linking flags (used only during linking phase)
    link_flags = config.get("linking", {}).get("base", {}).get("flags", [])
    link_flags.extend(config.get("linking", {}).get("sketch", {}).get("flags", []))
    link_flags.extend(build_mode_config.get("link_flags", []))

    return {
        "defines": defines,
        "compiler_flags": compiler_flags,
        "link_flags": link_flags,
    }


def ensure_library_built(
    build_mode: str,
    verbose: bool = False,
    force: bool = False,
    unity_chunks: int = 0,
) -> bool:
    """
    Ensure libfastled.a is up-to-date by calling wasm_build_library.py.

    Args:
        build_mode: Build mode to pass to library build
        verbose: Enable verbose output
        force: Force rebuild of library
        unity_chunks: Number of unity build chunks (0 = disabled)

    Returns:
        True if successful
    """
    print("Checking library build...")
    cmd = [
        "uv",
        "run",
        "python",
        str(PROJECT_ROOT / "ci" / "wasm_build_library.py"),
        "--mode",
        build_mode,
    ]
    if verbose:
        cmd.append("--verbose")
    if force:
        cmd.append("--force")
    if unity_chunks > 0:
        cmd.extend(["--unity-chunks", str(unity_chunks)])

    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    return result.returncode == 0


def create_wrapper_for_example(example_name: str, output_path: Path) -> Path:
    """Create a wrapper .cpp file for an example sketch (incremental-safe)."""
    example_dir = PROJECT_ROOT / "examples" / example_name
    ino_file = example_dir / f"{example_name}.ino"

    if not ino_file.exists():
        raise FileNotFoundError(f"Example not found: {ino_file}")

    # Use absolute path so the compiler can find it regardless of working directory
    wrapper_content = f"""// Auto-generated wrapper for {example_name}.ino
// For WASM builds, we use the standard entry point from platforms/wasm/entry_point.cpp
#include "{ino_file.as_posix()}"
"""

    # Only write if content changed (preserve timestamp for incremental builds)
    if output_path.exists():
        with open(output_path, "r") as f:
            existing_content = f.read()
        if existing_content == wrapper_content:
            return output_path  # No change, preserve timestamp

    with open(output_path, "w") as f:
        f.write(wrapper_content)

    return output_path


def needs_linking(
    sketch_object: Path,
    entry_point_object: Path,
    library_archive: Path,
    output_wasm: Path,
) -> bool:
    """
    Check if linking is needed by comparing timestamps.

    Args:
        sketch_object: Sketch .o file
        entry_point_object: Entry point .o file
        library_archive: Library .a archive
        output_wasm: Output .wasm file

    Returns:
        True if linking is needed, False if output is up-to-date
    """
    # If output doesn't exist, we need to link
    if not output_wasm.exists():
        return True

    # Get output modification time
    output_mtime = output_wasm.stat().st_mtime

    # Check if any input is newer than output
    inputs = [sketch_object, entry_point_object, library_archive]
    for input_file in inputs:
        if not input_file.exists():
            return True  # Input missing, need to link
        if input_file.stat().st_mtime > output_mtime:
            return True  # Input is newer, need to link

    # All inputs are older than output, no linking needed
    return False


def needs_compilation(source_file: Path, output_object: Path) -> bool:
    """
    Check if source needs recompilation.

    Args:
        source_file: Source .cpp file
        output_object: Output .o file

    Returns:
        True if compilation needed, False if up-to-date
    """
    if not output_object.exists():
        return True

    source_mtime = source_file.stat().st_mtime
    object_mtime = output_object.stat().st_mtime

    return source_mtime > object_mtime


def compile_object(
    source_file: Path,
    output_object: Path,
    emcc: str,
    flags: dict[str, list[str]],
    verbose: bool = False,
    force: bool = False,
) -> bool:
    """
    Compile a single source file to an object file using PCH.

    Args:
        source_file: Source .cpp file to compile
        output_object: Output .o file path
        emcc: Compiler command (e.g., 'clang-tool-chain-emcc')
        flags: Build flags dictionary
        verbose: Enable verbose output
        force: Force recompilation even if up-to-date

    Returns:
        True if successful
    """
    # Check if compilation is needed
    if not force and not needs_compilation(source_file, output_object):
        if verbose:
            print(f"✓ {source_file.name} is up-to-date")
        return True

    # Build includes
    includes = [
        "-Isrc",
        "-Isrc/platforms/wasm",
        "-Isrc/platforms/wasm/compiler",
    ]

    # PCH path
    pch_file = BUILD_DIR / "wasm_pch.h.pch"

    # Build compilation command
    cmd = (
        [
            emcc,
            "-c",
            str(source_file),
            "-o",
            str(output_object),
            "-include-pch",
            str(pch_file),
        ]
        + includes
        + flags["defines"]
        + flags["compiler_flags"]
    )

    if verbose:
        print(f"Compiling {source_file.name}...")
        print(f"Command: {subprocess.list2cmdline(cmd)}")

    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    return result.returncode == 0


def compile_wasm(
    source_file: Path,
    output_file: Path,
    build_mode: str = "quick",
    verbose: bool = False,
    force: bool = False,
    unity_chunks: int = 0,
) -> int:
    """
    Compile a FastLED sketch to WebAssembly using incremental compilation.

    Build phases:
      1. Ensure PCH is up-to-date (via wasm_build_library.py dependency)
      2. Ensure library is up-to-date (calls wasm_build_library.py)
      3. Compile sketch wrapper to object file (always rebuild)
      4. Compile entry_point.cpp to object file (always rebuild)
      5. Link sketch.o + entry_point.o + libfastled.a -> wasm

    Args:
        source_file: Path to wrapper .cpp file or example name
        output_file: Output .js file path
        build_mode: Build mode (debug, fast_debug, quick, release)
        verbose: Enable verbose output
        force: Force rebuild of all components
        unity_chunks: Number of unity build chunks (0 = disabled)

    Returns:
        Exit code (0 = success)
    """
    try:
        start_time = time.time()
        print(f"Building FastLED sketch to WASM (mode: {build_mode})...")

        # Get tool command
        emcc = get_emcc()
        if verbose:
            print(f"Using emscripten: {emcc}")

        # Create build directories
        BUILD_DIR.mkdir(parents=True, exist_ok=True)
        LTO_CACHE_DIR.mkdir(parents=True, exist_ok=True)

        # Phase 1 & 2: Ensure library is up-to-date (also ensures PCH is up-to-date)
        phase2_start = time.time()
        if not ensure_library_built(build_mode, verbose, force, unity_chunks):
            print("✗ Library build failed")
            return 1
        phase2_time = time.time() - phase2_start
        if verbose:
            print(f"Library build phase: {phase2_time:.2f}s")

        # Load build flags for sketch compilation
        flags = load_build_flags(build_mode, target="sketch")
        if verbose:
            print(
                f"Loaded {len(flags['defines'])} defines, {len(flags['compiler_flags'])} compiler flags, {len(flags['link_flags'])} link flags"
            )

        # Phase 3: Compile sketch wrapper to object file
        phase3_start = time.time()
        sketch_object = BUILD_DIR / "sketch.o"
        if needs_compilation(source_file, sketch_object) or force:
            print(f"Compiling sketch: {source_file.name}")
            if not compile_object(
                source_file, sketch_object, emcc, flags, verbose, force
            ):
                print("✗ Sketch compilation failed")
                return 1
        else:
            print("✓ Sketch is up-to-date")
        phase3_time = time.time() - phase3_start
        if verbose:
            print(f"Sketch compilation: {phase3_time:.2f}s")

        # Phase 3b: Compile entry_point.cpp to object file
        entry_point_object = BUILD_DIR / "entry_point.o"
        if needs_compilation(ENTRY_POINT_CPP, entry_point_object) or force:
            print(f"Compiling entry point: {ENTRY_POINT_CPP.name}")
            if not compile_object(
                ENTRY_POINT_CPP, entry_point_object, emcc, flags, verbose, force
            ):
                print("✗ Entry point compilation failed")
                return 1
        else:
            print("✓ Entry point is up-to-date")
        phase3_time += time.time() - phase3_start

        # Phase 4: Link everything together
        phase4_start = time.time()
        library_archive = BUILD_DIR / "libfastled.a"

        if not library_archive.exists():
            print(f"✗ Library not found: {library_archive}")
            return 1

        # Check if linking is needed (incremental linking)
        wasm_output = output_file.with_suffix(".wasm")
        if not force and not needs_linking(
            sketch_object, entry_point_object, library_archive, wasm_output
        ):
            phase4_time = time.time() - phase4_start
            total_time = time.time() - start_time
            print("✓ WASM output is up-to-date, skipping linking")
            print(f"  - {output_file}")
            if wasm_output.exists():
                print(f"  - {wasm_output}")
            print("\nBuild times:")
            print(f"  Library:  {phase2_time:.2f}s")
            print(f"  Sketch:   {phase3_time:.2f}s")
            print(f"  Linking:  {phase4_time:.2f}s (skipped)")
            print(f"  Total:    {total_time:.2f}s")
            return 0

        print("Linking final WASM module...")

        # Build includes for linking
        includes = [
            "-Isrc",
            "-Isrc/platforms/wasm",
            "-Isrc/platforms/wasm/compiler",
        ]

        # Link command: sketch.o + entry_point.o + libfastled.a -> output
        link_cmd = (
            [
                emcc,
                str(sketch_object),
                str(entry_point_object),
                str(library_archive),
            ]
            + includes
            + flags["defines"]
            + [
                "-o",
                str(output_file),
            ]
            + flags["link_flags"]
        )

        if verbose:
            print(f"Link command: {subprocess.list2cmdline(link_cmd)}")

        result = subprocess.run(link_cmd, cwd=PROJECT_ROOT)
        phase4_time = time.time() - phase4_start

        if result.returncode == 0:
            total_time = time.time() - start_time
            print("✓ Build successful!")
            print(f"  - {output_file}")
            wasm_file = output_file.with_suffix(".wasm")
            if wasm_file.exists():
                print(f"  - {wasm_file}")
            print("\nBuild times:")
            print(f"  Library:  {phase2_time:.2f}s")
            print(f"  Sketch:   {phase3_time:.2f}s")
            print(f"  Linking:  {phase4_time:.2f}s")
            print(f"  Total:    {total_time:.2f}s")
            return 0
        else:
            print(f"✗ Linking failed with return code {result.returncode}")
            return result.returncode

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"✗ Build failed with exception: {e}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Native WASM compilation using clang-tool-chain"
    )
    parser.add_argument("source", nargs="?", help="Source wrapper .cpp file")
    parser.add_argument("--example", help="Example name (e.g., Blink)")
    parser.add_argument("-o", "--output", required=True, help="Output .js file")
    parser.add_argument(
        "--mode",
        default="quick",
        choices=["debug", "fast_debug", "quick", "release"],
        help="Build mode (default: quick)",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument(
        "--force", action="store_true", help="Force rebuild of all components"
    )
    parser.add_argument(
        "--unity-chunks",
        type=int,
        default=0,
        metavar="N",
        help="Enable unity builds with N chunks (default: 0 = disabled)",
    )

    args = parser.parse_args()

    # Determine source file
    if args.example:
        # Create wrapper for example
        wrapper_file = PROJECT_ROOT / "build" / f"{args.example}_wrapper.cpp"
        wrapper_file.parent.mkdir(exist_ok=True)
        source_file = create_wrapper_for_example(args.example, wrapper_file)
    elif args.source:
        source_file = Path(args.source)
        if not source_file.exists():
            print(f"Error: Source file not found: {source_file}", file=sys.stderr)
            return 1
    else:
        parser.error("Either source or --example must be specified")

    output_file = Path(args.output)

    try:
        return compile_wasm(
            source_file,
            output_file,
            args.mode,
            args.verbose,
            args.force,
            args.unity_chunks,
        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\n✗ Build interrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
