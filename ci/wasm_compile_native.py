#!/usr/bin/env python3
"""
Native WASM compilation using clang-tool-chain's emscripten.

This script compiles FastLED sketches to WebAssembly using the emscripten
toolchain bundled with clang-tool-chain, avoiding Docker overhead for
significantly faster builds (~60-90s vs several minutes).

The script reads compilation flags from:
  src/platforms/wasm/compiler/build_flags.toml

Usage:
  uv run ci/wasm_compile_native.py <sketch_wrapper.cpp> -o <output.js>
  uv run ci/wasm_compile_native.py --example Blink -o blink.js
"""

import argparse
import subprocess
import sys
import tomllib
from pathlib import Path


# Project root
PROJECT_ROOT = Path(__file__).parent.parent

# Paths
BUILD_FLAGS_TOML = (
    PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "build_flags.toml"
)


def find_emscripten() -> Path:
    """Find emscripten em++ compiler in clang-tool-chain."""
    # Check for clang-tool-chain installation
    home = Path.home()
    emcc_path = (
        home
        / ".clang-tool-chain"
        / "emscripten"
        / "win"
        / "x86_64"
        / "emscripten"
        / "em++.bat"
    )

    if emcc_path.exists():
        return emcc_path

    # Try Unix-style path
    emcc_path = (
        home
        / ".clang-tool-chain"
        / "emscripten"
        / "linux"
        / "x86_64"
        / "emscripten"
        / "em++"
    )
    if emcc_path.exists():
        return emcc_path

    # Try macOS path
    emcc_path = (
        home
        / ".clang-tool-chain"
        / "emscripten"
        / "darwin"
        / "x86_64"
        / "emscripten"
        / "em++"
    )
    if emcc_path.exists():
        return emcc_path

    raise FileNotFoundError(
        "Could not find emscripten compiler in clang-tool-chain installation. "
        "Make sure clang-tool-chain is installed with emscripten support."
    )


def load_build_flags(build_mode: str = "quick") -> dict[str, list[str]]:
    """
    Load and parse build flags from build_flags.toml.

    Args:
        build_mode: Build mode (debug, fast_debug, quick, release)

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

    # Add sketch-specific flags (not library flags)
    defines.extend(config.get("sketch", {}).get("defines", []))
    compiler_flags.extend(config.get("sketch", {}).get("compiler_flags", []))

    # Add build mode-specific flags
    build_mode_config = config.get("build_modes", {}).get(build_mode, {})
    compiler_flags.extend(build_mode_config.get("flags", []))

    # Get linking flags
    link_flags = config.get("linking", {}).get("base", {}).get("flags", [])
    link_flags.extend(config.get("linking", {}).get("sketch", {}).get("flags", []))
    link_flags.extend(build_mode_config.get("link_flags", []))

    return {
        "defines": defines,
        "compiler_flags": compiler_flags,
        "link_flags": link_flags,
    }


def get_source_files() -> list[str]:
    """Get all .cpp source files from src/"""
    result = subprocess.run(
        [
            "uv",
            "run",
            "python",
            str(PROJECT_ROOT / "ci" / "meson" / "rglob.py"),
            "src",
            "*.cpp",
        ],
        capture_output=True,
        text=True,
        check=True,
        cwd=PROJECT_ROOT,
    )
    return [line.strip() for line in result.stdout.strip().split("\n") if line.strip()]


def create_wrapper_for_example(example_name: str, output_path: Path) -> Path:
    """Create a wrapper .cpp file for an example sketch."""
    example_dir = PROJECT_ROOT / "examples" / example_name
    ino_file = example_dir / f"{example_name}.ino"

    if not ino_file.exists():
        raise FileNotFoundError(f"Example not found: {ino_file}")

    # Use absolute path so the compiler can find it regardless of working directory
    wrapper_content = f"""// Auto-generated wrapper for {example_name}.ino
// For WASM builds, we use the standard entry point from platforms/wasm/entry_point.cpp
#include "{ino_file.as_posix()}"
"""

    with open(output_path, "w") as f:
        f.write(wrapper_content)

    return output_path


def compile_wasm(
    source_file: Path,
    output_file: Path,
    build_mode: str = "quick",
    verbose: bool = False,
) -> int:
    """
    Compile a FastLED sketch to WebAssembly.

    Args:
        source_file: Path to wrapper .cpp file or example name
        output_file: Output .js file path
        build_mode: Build mode (debug, fast_debug, quick, release)
        verbose: Enable verbose output

    Returns:
        Exit code (0 = success)
    """
    try:
        print(f"Building FastLED sketch to WASM (mode: {build_mode})...")

        # Find emscripten compiler
        emcc = find_emscripten()
        print(f"Using emscripten: {emcc}")

        # Load build flags from TOML
        flags = load_build_flags(build_mode)
        print(
            f"Loaded {len(flags['defines'])} defines, {len(flags['compiler_flags'])} compiler flags, {len(flags['link_flags'])} link flags"
        )

        # Get all source files
        sources = get_source_files()
        print(f"Found {len(sources)} FastLED source files")

        # Create response file for sources
        response_file = PROJECT_ROOT / "build" / "wasm_sources.rsp"
        response_file.parent.mkdir(exist_ok=True)

        with open(response_file, "w") as f:
            for src in sources:
                f.write(f'"{src}"\n')
            f.write(f'"{source_file}"\n')

        # Build includes
        includes = [
            "-Isrc",
            "-Isrc/platforms/wasm",
            "-Isrc/platforms/wasm/compiler",
        ]

        # Build command
        cmd = (
            [
                str(emcc),
                f"@{response_file}",
            ]
            + includes
            + flags["defines"]
            + flags["compiler_flags"]
            + [
                "-o",
                str(output_file),
            ]
            + flags["link_flags"]
        )

        if verbose:
            print(
                f"Command: {subprocess.list2cmdline(cmd[:10])}... (see response file for full source list)"
            )

        print("Compiling...")
        result = subprocess.run(cmd, cwd=PROJECT_ROOT)

        if result.returncode == 0:
            print("✓ Build successful!")
            print(f"  - {output_file}")
            wasm_file = output_file.with_suffix(".wasm")
            if wasm_file.exists():
                print(f"  - {wasm_file}")
            return 0
        else:
            print(f"✗ Build failed with return code {result.returncode}")
            return result.returncode

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"✗ Build failed with exception: {e}", file=sys.stderr)
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
        return compile_wasm(source_file, output_file, args.mode, args.verbose)
    except KeyboardInterrupt:
        print("\n✗ Build interrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
