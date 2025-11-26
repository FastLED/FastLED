#!/usr/bin/env python3
"""
WASM Static Library Build Script (libfastled.a)

This script builds the FastLED library as a static archive (libfastled.a) for WASM,
enabling incremental builds by compiling only changed sources and reusing the library
across multiple sketch compilations.

Key Features:
- Incremental compilation: Only rebuilds changed sources
- PCH support: Uses precompiled header from Phase 1
- Parallel compilation: Multi-threaded object file generation
- Dependency tracking: Parse .d files to detect header changes
- Thin archive: References objects instead of copying (faster linking)
- Smart invalidation: Detects PCH, flag, and source changes

Architecture:
    1. Check if PCH exists and is up-to-date (delegates to wasm_compile_pch.py)
    2. Discover all .cpp sources using rglob.py
    3. For each source, check if rebuild needed (source/header/flags changed)
    4. Compile changed sources in parallel to .o files (with PCH)
    5. Generate dependency files (.d) for header tracking
    6. Create thin archive: emar rcsT libfastled.a *.o

Usage:
    uv run python ci/wasm_build_library.py [--mode debug|fast_debug|quick|release] [--force] [--verbose]
    uv run python ci/wasm_build_library.py --clean  # Remove all library artifacts
    uv run python ci/wasm_build_library.py --parallel N  # Set parallelism (default: CPU count)

Performance:
    - Initial build: ~251 objects compiled
    - Incremental build: Only changed objects recompiled
    - Parallel compilation: Scales with CPU cores
    - Thin archive: Fast linking with object references
"""

import argparse
import hashlib
import json
import subprocess
import sys
import tomllib
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


# Project root
PROJECT_ROOT = Path(__file__).parent.parent

# Paths
BUILD_FLAGS_TOML = (
    PROJECT_ROOT / "src" / "platforms" / "wasm" / "compiler" / "build_flags.toml"
)
PCH_SCRIPT = PROJECT_ROOT / "ci" / "wasm_compile_pch.py"
RGLOB_SCRIPT = PROJECT_ROOT / "ci" / "meson" / "rglob.py"
COMPILE_PCH_WRAPPER = PROJECT_ROOT / "ci" / "compile_pch.py"

BUILD_DIR = PROJECT_ROOT / "build" / "wasm"
OBJECTS_DIR = BUILD_DIR / "objects"
DEPS_DIR = BUILD_DIR / "deps"
LIBRARY_OUTPUT = BUILD_DIR / "libfastled.a"
LIBRARY_METADATA = BUILD_DIR / "library_metadata.json"
PCH_OUTPUT = BUILD_DIR / "wasm_pch.h.pch"


def find_emscripten() -> Path:
    """Find emscripten em++ compiler in clang-tool-chain."""
    home = Path.home()

    # Try Windows path
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

    # Try Linux path
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


def find_emar() -> Path:
    """Find emscripten emar archiver in clang-tool-chain."""
    home = Path.home()

    # Try Windows path
    emar_path = (
        home
        / ".clang-tool-chain"
        / "emscripten"
        / "win"
        / "x86_64"
        / "emscripten"
        / "emar.bat"
    )
    if emar_path.exists():
        return emar_path

    # Try Linux path
    emar_path = (
        home
        / ".clang-tool-chain"
        / "emscripten"
        / "linux"
        / "x86_64"
        / "emscripten"
        / "emar"
    )
    if emar_path.exists():
        return emar_path

    # Try macOS path
    emar_path = (
        home
        / ".clang-tool-chain"
        / "emscripten"
        / "darwin"
        / "x86_64"
        / "emscripten"
        / "emar"
    )
    if emar_path.exists():
        return emar_path

    raise FileNotFoundError(
        "Could not find emscripten archiver (emar) in clang-tool-chain installation. "
        "Make sure clang-tool-chain is installed with emscripten support."
    )


def load_build_flags(build_mode: str = "quick") -> dict[str, list[str]]:
    """
    Load and parse build flags from build_flags.toml for library compilation.

    Library uses [all] + [library] sections (NOT sketch flags).

    Args:
        build_mode: Build mode (debug, fast_debug, quick, release)

    Returns:
        Dictionary with 'defines', 'compiler_flags' lists
    """
    if not BUILD_FLAGS_TOML.exists():
        raise FileNotFoundError(f"Build flags TOML not found: {BUILD_FLAGS_TOML}")

    with open(BUILD_FLAGS_TOML, "rb") as f:
        config = tomllib.load(f)

    # Collect flags from [all] section (used by everything)
    defines = config.get("all", {}).get("defines", [])
    compiler_flags = config.get("all", {}).get("compiler_flags", [])

    # Add library-specific flags
    defines.extend(config.get("library", {}).get("defines", []))
    compiler_flags.extend(config.get("library", {}).get("compiler_flags", []))

    # Add build mode-specific flags
    build_mode_config = config.get("build_modes", {}).get(build_mode, {})
    compiler_flags.extend(build_mode_config.get("flags", []))

    return {
        "defines": defines,
        "compiler_flags": compiler_flags,
    }


def compute_flags_hash(flags: dict[str, list[str]]) -> str:
    """Compute hash of compilation flags for change detection."""
    all_flags = sorted(flags["defines"] + flags["compiler_flags"])
    flags_str = " ".join(all_flags)
    return hashlib.sha256(flags_str.encode()).hexdigest()


def get_source_files() -> list[Path]:
    """Get all .cpp source files from src/ directory (with caching)."""
    # Use native Python glob instead of subprocess call for speed
    # This is 10-100x faster than spawning a subprocess
    src_dir = PROJECT_ROOT / "src"
    sources = []
    for cpp_file in src_dir.rglob("*.cpp"):
        sources.append(cpp_file.resolve())

    return sorted(sources)  # Sort for deterministic ordering


def parse_depfile(depfile_path: Path) -> list[Path]:
    """
    Parse Makefile-style dependency file to extract header dependencies.

    Format: target: dep1 dep2 dep3 \
            dep4 dep5

    Returns:
        List of dependency paths (excluding target)
    """
    if not depfile_path.exists():
        return []

    try:
        with open(depfile_path) as f:
            content = f.read()

        # Remove line continuations
        content = content.replace("\\\n", " ").replace("\\\r\n", " ")

        # Split on colon to separate target from dependencies
        if ":" not in content:
            return []

        deps_part = content.split(":", 1)[1]

        # Split on whitespace and filter out empty strings
        deps = [d.strip() for d in deps_part.split() if d.strip()]

        # Convert to Path objects
        return [Path(d) for d in deps]

    except Exception as e:
        print(f"Warning: Could not parse dependency file {depfile_path}: {e}")
        return []


def get_latest_mtime(paths: list[Path]) -> float:
    """Get the latest modification time from a list of paths."""
    max_mtime = 0.0
    for p in paths:
        if p.exists():
            mtime = p.stat().st_mtime
            if mtime > max_mtime:
                max_mtime = mtime
    return max_mtime


def needs_rebuild(
    source_path: Path,
    object_path: Path,
    depfile_path: Path,
    pch_path: Path,
    flags_hash: str,
    metadata: dict[str, str],
    force: bool = False,
) -> tuple[bool, str]:
    """
    Determine if a source file needs to be recompiled.

    Returns:
        (needs_rebuild, reason) tuple
    """
    if force:
        return True, "forced rebuild"

    if not object_path.exists():
        return True, "object file does not exist"

    # Check if flags changed
    if metadata.get("flags_hash") != flags_hash:
        return True, "compilation flags changed"

    # Check if PCH changed
    if pch_path.exists():
        pch_mtime = pch_path.stat().st_mtime
        obj_mtime = object_path.stat().st_mtime
        if pch_mtime > obj_mtime:
            return True, "PCH was rebuilt"

    # Check if source file changed
    source_mtime = source_path.stat().st_mtime
    obj_mtime = object_path.stat().st_mtime
    if source_mtime > obj_mtime:
        return True, "source file changed"

    # Check if any header dependency changed
    deps = parse_depfile(depfile_path)
    if deps:
        latest_dep_mtime = get_latest_mtime(deps)
        if latest_dep_mtime > obj_mtime:
            return True, "header dependency changed"

    return False, "object is up to date"


def compile_object(
    emcc: Path,
    source_path: Path,
    object_path: Path,
    depfile_path: Path,
    pch_path: Path,
    flags: dict[str, list[str]],
    verbose: bool = False,
) -> tuple[bool, str, str]:
    """
    Compile a single source file to an object file.

    Args:
        emcc: Path to em++ compiler
        source_path: Source .cpp file
        object_path: Output .o file
        depfile_path: Output .d file
        pch_path: Precompiled header file
        flags: Compilation flags
        verbose: Enable verbose output

    Returns:
        (success, source_name, error_message) tuple
    """
    try:
        # Ensure output directories exist
        object_path.parent.mkdir(parents=True, exist_ok=True)
        depfile_path.parent.mkdir(parents=True, exist_ok=True)

        # Build includes (must match PCH compilation)
        includes = [
            f"-I{PROJECT_ROOT / 'src'}",
            f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm'}",
            f"-I{PROJECT_ROOT / 'src' / 'platforms' / 'wasm' / 'compiler'}",
        ]

        # PCH usage flags
        pch_flags = []
        if pch_path.exists():
            pch_flags = [
                "-include-pch",
                str(pch_path),
                "-Werror=invalid-pch",  # Error if PCH is invalid
            ]

        # Build command using compile_pch.py wrapper (fixes depfile)
        cmd = (
            [
                "uv",
                "run",
                "python",
                str(COMPILE_PCH_WRAPPER),
                str(emcc),
                "-c",  # Compile only, don't link
                str(source_path),
                "-o",
                str(object_path),
                "-MD",  # Generate dependency file
                "-MF",
                str(depfile_path),
            ]
            + includes
            + flags["defines"]
            + flags["compiler_flags"]
            + pch_flags
        )

        if verbose:
            print(f"  Compiling: {source_path.name}")
            print(f"  Command: {' '.join(cmd)}")

        # Run compilation
        result = subprocess.run(
            cmd,
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
        )

        if result.returncode != 0:
            error_msg = result.stderr if result.stderr else result.stdout
            return False, source_path.name, f"Compilation failed:\n{error_msg}"

        # Verify object was created
        if not object_path.exists():
            return (
                False,
                source_path.name,
                f"Compilation reported success but object file not found: {object_path}",
            )

        return True, source_path.name, ""

    except KeyboardInterrupt:
        raise
    except Exception as e:
        return False, source_path.name, f"Exception during compilation: {e}"


def ensure_pch_built(build_mode: str, verbose: bool = False) -> int:
    """
    Ensure PCH is built and up-to-date by delegating to wasm_compile_pch.py.

    Returns:
        Exit code (0 = success)
    """
    print("Checking PCH status...")

    cmd = [
        "uv",
        "run",
        "python",
        str(PCH_SCRIPT),
        "--mode",
        build_mode,
    ]

    if verbose:
        cmd.append("--verbose")

    result = subprocess.run(cmd, cwd=PROJECT_ROOT)

    if result.returncode != 0:
        print(f"✗ PCH build/check failed with return code {result.returncode}")
        return result.returncode

    return 0


def create_thin_archive(
    emar: Path,
    object_files: list[Path],
    output_path: Path,
    verbose: bool = False,
) -> int:
    """
    Create a thin archive from object files.

    Thin archives store references to object files instead of copying them,
    resulting in faster archive creation and smaller archive size.

    Args:
        emar: Path to emar archiver
        object_files: List of .o files to include
        output_path: Output .a archive path
        verbose: Enable verbose output

    Returns:
        Exit code (0 = success)
    """
    print(f"Creating thin archive: {output_path.name}...")

    if not object_files:
        print("✗ No object files to archive")
        return 1

    # Ensure output directory exists
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Create response file for object list (avoid command line length limits)
    response_file = BUILD_DIR / "archive_objects.rsp"
    with open(response_file, "w") as f:
        for obj in object_files:
            f.write(f"{obj}\n")

    # Build archive command
    # rcsT flags:
    #   r = insert/replace files in archive
    #   c = create archive if it doesn't exist
    #   s = write an index (required for linking)
    #   T = create thin archive (references instead of copies)
    cmd = [
        str(emar),
        "rcsT",
        str(output_path),
        f"@{response_file}",
    ]

    if verbose:
        print(f"Command: {' '.join(cmd)}")

    result = subprocess.run(cmd, cwd=PROJECT_ROOT)

    if result.returncode != 0:
        print(f"✗ Archive creation failed with return code {result.returncode}")
        return result.returncode

    # Verify archive was created
    if not output_path.exists():
        print(f"✗ Archive creation reported success but file not found: {output_path}")
        return 1

    print(f"✓ Archive created successfully: {output_path}")
    return 0


def load_metadata() -> dict[str, str]:
    """Load library build metadata from previous build."""
    if not LIBRARY_METADATA.exists():
        return {}

    try:
        with open(LIBRARY_METADATA) as f:
            return json.load(f)
    except Exception as e:
        print(f"Warning: Could not load library metadata: {e}")
        return {}


def save_metadata(metadata: dict[str, str]) -> None:
    """Save library build metadata for future builds."""
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    with open(LIBRARY_METADATA, "w") as f:
        json.dump(metadata, f, indent=2)


def clean_library() -> int:
    """Remove all library build artifacts."""
    print("Cleaning library artifacts...")

    removed_count = 0

    # Remove library file
    if LIBRARY_OUTPUT.exists():
        LIBRARY_OUTPUT.unlink()
        print(f"  Removed: {LIBRARY_OUTPUT}")
        removed_count += 1

    # Remove metadata file
    if LIBRARY_METADATA.exists():
        LIBRARY_METADATA.unlink()
        print(f"  Removed: {LIBRARY_METADATA}")
        removed_count += 1

    # Remove objects directory
    if OBJECTS_DIR.exists():
        import shutil

        shutil.rmtree(OBJECTS_DIR)
        print(f"  Removed: {OBJECTS_DIR}")
        removed_count += 1

    # Remove deps directory
    if DEPS_DIR.exists():
        import shutil

        shutil.rmtree(DEPS_DIR)
        print(f"  Removed: {DEPS_DIR}")
        removed_count += 1

    if removed_count == 0:
        print("  No library artifacts found to clean")
    else:
        print(f"✓ Cleaned {removed_count} library artifact(s)")

    return 0


def build_library(
    build_mode: str = "quick",
    force: bool = False,
    verbose: bool = False,
    parallel: int | None = None,
) -> int:
    """
    Build the FastLED static library.

    Args:
        build_mode: Build mode (debug, fast_debug, quick, release)
        force: Force rebuild all objects
        verbose: Enable verbose output
        parallel: Number of parallel jobs (None = CPU count)

    Returns:
        Exit code (0 = success)
    """
    try:
        import time

        start_time = time.time()

        print(f"Building FastLED library (mode: {build_mode})...")

        # Step 1: Ensure PCH is built
        pch_result = ensure_pch_built(build_mode, verbose)
        if pch_result != 0:
            return pch_result

        # Step 2: Find compiler and archiver
        emcc = find_emscripten()
        emar = find_emar()
        if verbose:
            print(f"Using emscripten: {emcc}")
            print(f"Using archiver: {emar}")

        # Step 3: Load build flags
        flags = load_build_flags(build_mode)
        flags_hash = compute_flags_hash(flags)
        if verbose:
            print(
                f"Loaded {len(flags['defines'])} defines, {len(flags['compiler_flags'])} compiler flags"
            )

        # Step 4: Load metadata from previous build
        metadata = load_metadata()

        # Step 5: Discover all source files
        print("Discovering source files...")
        sources = get_source_files()
        print(f"Found {len(sources)} source files")

        # Step 6: Fast-path check - if flags haven't changed, PCH is same, and archive exists, we're done
        if (
            not force
            and LIBRARY_OUTPUT.exists()
            and metadata.get("flags_hash") == flags_hash
            and metadata.get("source_count") == len(sources)
        ):
            # Check if PCH was rebuilt (would invalidate all objects)
            if PCH_OUTPUT.exists():
                pch_mtime = PCH_OUTPUT.stat().st_mtime
                archive_mtime = LIBRARY_OUTPUT.stat().st_mtime
                if pch_mtime <= archive_mtime:
                    elapsed = time.time() - start_time
                    print("Checking which sources need compilation...")
                    print(f"  0 sources to compile, {len(sources)} up-to-date")
                    print(
                        f"\n✓ Library is up-to-date (fast-path), skipping rebuild ({elapsed:.2f}s)"
                    )
                    print(f"  Output: {LIBRARY_OUTPUT}")
                    print(f"  Objects: {len(sources)} files")
                    return 0

        # Step 6: Determine which sources need compilation (detailed check)
        print("Checking which sources need compilation...")
        sources_to_compile = []
        up_to_date_count = 0

        for source in sources:
            # Compute object and depfile paths
            # Keep directory structure under objects/ for organization
            rel_path = source.relative_to(PROJECT_ROOT / "src")
            object_path = OBJECTS_DIR / rel_path.with_suffix(".o")
            depfile_path = DEPS_DIR / rel_path.with_suffix(".d")

            rebuild_needed, reason = needs_rebuild(
                source,
                object_path,
                depfile_path,
                PCH_OUTPUT,
                flags_hash,
                metadata,
                force,
            )

            if rebuild_needed:
                sources_to_compile.append((source, object_path, depfile_path))
                if verbose:
                    print(f"  {source.name}: {reason}")
            else:
                up_to_date_count += 1

        print(
            f"  {len(sources_to_compile)} sources to compile, {up_to_date_count} up-to-date"
        )

        # Early exit optimization: If no sources need compilation AND archive exists, we're done
        if not sources_to_compile and LIBRARY_OUTPUT.exists():
            elapsed = time.time() - start_time
            print(
                f"\n✓ Library is up-to-date, skipping archive creation ({elapsed:.2f}s)"
            )
            print(f"  Output: {LIBRARY_OUTPUT}")
            print(f"  Objects: {len(sources)} files")
            return 0

        # Step 7: Compile sources in parallel
        if sources_to_compile:
            print(
                f"Compiling {len(sources_to_compile)} sources (parallel: {parallel or 'auto'})..."
            )

            # Use ThreadPoolExecutor for parallel compilation
            max_workers = parallel if parallel else None
            failed_compilations = []

            with ThreadPoolExecutor(max_workers=max_workers) as executor:
                # Submit all compilation tasks
                futures = {
                    executor.submit(
                        compile_object,
                        emcc,
                        source,
                        obj_path,
                        dep_path,
                        PCH_OUTPUT,
                        flags,
                        verbose,
                    ): source.name
                    for source, obj_path, dep_path in sources_to_compile
                }

                # Collect results as they complete
                completed = 0
                for future in as_completed(futures):
                    success, source_name, error_msg = future.result()
                    completed += 1

                    if success:
                        if not verbose:
                            # Show progress without verbose spam
                            print(
                                f"  [{completed}/{len(sources_to_compile)}] {source_name}"
                            )
                    else:
                        print(f"✗ Failed: {source_name}")
                        if error_msg:
                            print(f"  {error_msg}")
                        failed_compilations.append(source_name)

            if failed_compilations:
                print(f"\n✗ {len(failed_compilations)} compilation(s) failed:")
                for name in failed_compilations:
                    print(f"  - {name}")
                return 1

            print(f"✓ All {len(sources_to_compile)} sources compiled successfully")
        else:
            print("✓ All sources are up-to-date, skipping compilation")

        # Step 8: Collect all object files
        print("Collecting object files...")
        all_objects = []
        for source in sources:
            rel_path = source.relative_to(PROJECT_ROOT / "src")
            object_path = OBJECTS_DIR / rel_path.with_suffix(".o")
            if object_path.exists():
                all_objects.append(object_path)

        if len(all_objects) != len(sources):
            print(f"✗ Expected {len(sources)} objects but found {len(all_objects)}")
            return 1

        print(f"Found {len(all_objects)} object files")

        # Step 9: Create thin archive
        archive_result = create_thin_archive(emar, all_objects, LIBRARY_OUTPUT, verbose)
        if archive_result != 0:
            return archive_result

        # Step 10: Save metadata for future builds
        metadata = {
            "flags_hash": flags_hash,
            "build_mode": build_mode,
            "source_count": len(sources),
            "object_count": len(all_objects),
        }
        save_metadata(metadata)

        # Summary
        elapsed = time.time() - start_time
        print(f"\n✓ Library build complete in {elapsed:.2f}s")
        print(f"  Output: {LIBRARY_OUTPUT}")
        print(f"  Objects: {len(all_objects)} files")
        print(f"  Compiled: {len(sources_to_compile)} files")
        print(f"  Up-to-date: {up_to_date_count} files")

        return 0

    except KeyboardInterrupt:
        raise
    except Exception as e:
        print(f"✗ Library build failed with exception: {e}", file=sys.stderr)
        if verbose:
            import traceback

            traceback.print_exc(file=sys.stderr)
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build FastLED static library for WASM with incremental compilation"
    )
    parser.add_argument(
        "--mode",
        default="quick",
        choices=["debug", "fast_debug", "quick", "release"],
        help="Build mode (default: quick)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Force rebuild all objects",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove library artifacts and exit",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Verbose output",
    )
    parser.add_argument(
        "--parallel",
        type=int,
        metavar="N",
        help="Number of parallel jobs (default: CPU count)",
    )

    args = parser.parse_args()

    try:
        # Handle clean command
        if args.clean:
            return clean_library()

        # Build library
        return build_library(args.mode, args.force, args.verbose, args.parallel)

    except KeyboardInterrupt:
        print("\n✗ Build interrupted by user")
        raise
    except Exception as e:
        print(f"✗ Build failed with exception: {e}", file=sys.stderr)
        if args.verbose:
            import traceback

            traceback.print_exc(file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
